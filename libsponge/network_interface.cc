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
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
/**
 * @brief This is called as time passes. Expire any IP-to-Ethernet mappings that have expired.
 * 
 * @param ms_since_last_tick time since last tick in ms.
 */
void NetworkInterface::tick(const size_t ms_since_last_tick) {}
