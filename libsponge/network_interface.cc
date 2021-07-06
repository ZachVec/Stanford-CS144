#include "network_interface.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include <iostream>
using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

/**
 * @brief Send a Ethernet frame out of \param dgram
 * 1. If the destination Ethernet address is already known, send it right away.
 *  - Create an Ethernet frame (with type=EthernetHeader::TYPE_IPv4)
 *  - set the payload to be the serialized datagram
 *  - set the source and destination address.
 * 2. Otherwise
 *  - broadcast an ARP request for the next hop's Ethernet address
 *  - Queue the IP datagram so it can be sent after the ARP reply is received.
 * 3. Except you don't want to flood the network with ARP request.
 *  - if the an ARP request about the same IP address has been sent in last 5 seconds.
 *    don't send a second request, wait for the reply of first one.
 * 
 * @param dgram the IPv4 datagram to be sent
 * @param next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
 * @note the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.
 */
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src  = _ethernet_address;
    frame.payload()     = dgram.serialize();
    const auto &it = _mappings.find(next_hop_ip);
    if(it != _mappings.end()) { // send it right now
        frame.header().dst  = it->second.addr;
        _frames_out.push(frame);
    } else if (broadcast_arp(next_hop_ip)) { // broadcast arp
        _outstanding_arp.emplace(next_hop_ip, _time + 5000);
        _outstanding_frm[next_hop_ip].emplace(frame);
    } else { // already broadcasted in last 5 seconds
        _outstanding_frm[next_hop_ip].emplace(frame);
    }
}

/**
 * @brief This method is called when an Ethernet frame arrives from the network.
 * should ignore any frames not destined for the network interface(broadcast or _ethernet_address)
 * 
 * 1. if \param frame is IPv4, parse the payload as an InternetDatagram (call parse())
 *  - if ParseResult::NoError, return the resulting InternetDatagram to the caller
 *  - otherwise, return nothing
 * 2. if \param frame is ARP,  parse the payload as an ARPMessage,
 *  - if successful, remember the mapping between the sender's IP address and Ethernet address for 30 seconds.
 *  - then send an appropriate ARP reply.
 * 
 * @param frame the incoming Ethernet frame
 * @return optional<InternetDatagram> 
 */
std::optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const auto &hdr = frame.header();
    if(hdr.dst != ETHERNET_BROADCAST && hdr.dst != _ethernet_address) {
        return std::nullopt; 
    } else if (hdr.type == EthernetHeader::TYPE_IPv4) {
        return recv_ipv4(frame);
    } else {
        recv_arp(frame);
        return std::nullopt;
    }
}

/**
 * @brief This is called as time passes.
 * \todo 
 * 1. increase timer
 * 2. Expire(erase) any IP-to-Ethernet mappings that have expired.
 * 3. Expire(erase) any outstanding arp that have expired
 * @param ms_since_last_tick time since last tick in ms.
 */
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
    for(auto it = _mappings.begin(); it != _mappings.end();) {
        if(it->second.timeout < _time) it = _mappings.erase(it);
        else ++it;
    }
    for(auto it = _outstanding_arp.begin(); it != _outstanding_arp.end();) {
        if(it->second < _time) it = _outstanding_arp.erase(it);
        else ++it;
    }
}

/**
 * @brief broadcast an ARP message
 * 
 * @param next_hop ip address of next hop
 * @return true if broadcast
 * @return false if not(due to the existence of this case in the _arp_tracker)
 */
bool NetworkInterface::broadcast_arp(uint32_t next_hop) {
    // if already exists, do not broadcast and return false
    if(_outstanding_arp.find(next_hop) != _outstanding_arp.end()) return false;

    //! \todo send an arp message
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REQUEST;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ip_address = next_hop;
    msg.sender_ethernet_address = _ethernet_address;

    EthernetFrame frm;
    frm.header().type = EthernetHeader::TYPE_ARP;
    frm.header().src  = _ethernet_address;
    frm.header().dst  = ETHERNET_BROADCAST;
    frm.payload()     = move(msg.serialize());
    _frames_out.emplace(frm);
    return true;
}

void NetworkInterface::recv_arp(const EthernetFrame &frame) {
    ARPMessage arp_get;
    if(arp_get.parse(frame.payload()) != ParseResult::NoError) {
        return;
    } else if(arp_get.target_ip_address != _ip_address.ipv4_numeric()) {
        return;
    } else {
        _mappings[arp_get.sender_ip_address] = {arp_get.sender_ethernet_address, _time + 30000};
        send_outstanding(arp_get.sender_ip_address);
        auto it = _outstanding_arp.find(arp_get.sender_ip_address);
        if(it != _outstanding_arp.end()) _outstanding_arp.erase(it);
        if(arp_get.opcode == ARPMessage::OPCODE_REPLY) return;

        // preparing ARPMessage
        ARPMessage arp_ret;
        arp_ret.opcode = ARPMessage::OPCODE_REPLY;
        arp_ret.sender_ip_address = _ip_address.ipv4_numeric();
        arp_ret.target_ip_address = arp_get.sender_ip_address;
        arp_ret.sender_ethernet_address = _ethernet_address;
        arp_ret.target_ethernet_address = arp_get.sender_ethernet_address;

        // preparing EthernetFrame
        EthernetFrame reply;
        reply.header().type = EthernetHeader::TYPE_ARP;
        reply.header().src  = _ethernet_address;
        reply.header().dst  = arp_get.sender_ethernet_address;
        reply.payload()     = move(arp_ret.serialize());

        // sending
        _frames_out.emplace(reply);
    }
}

std::optional<InternetDatagram> NetworkInterface::recv_ipv4(const EthernetFrame &frame) {
    InternetDatagram ret;
    if(ret.parse(frame.payload()) == ParseResult::NoError) {
        return ret;
    } else {
        return std::nullopt;
    }
}

void NetworkInterface::send_outstanding(uint32_t next_hop) {
    const EthernetAddress eth_addr = _mappings[next_hop].addr;
    for(queue<EthernetFrame> &q = _outstanding_frm[next_hop]; !q.empty(); q.pop()) {
        q.front().header().dst = eth_addr;
        _frames_out.emplace(q.front());
    }
}
