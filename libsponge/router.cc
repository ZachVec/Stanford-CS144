#include "router.hh"
#include <iostream>
using namespace std;

/**
 * @brief This method adds a route to the routing table. 
 * Track this info by adding a private member in the Router class.
 * 
 * @param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
 * @param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
 * @param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
 * @param[in] interface_num The index of the interface to send the datagram out on.
 */
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    _routes.insert({route_prefix, prefix_length, next_hop, interface_num});
}

/**
 * @brief return whether dst and route match
 * 
 * @param route target route to compare
 * @param dst target ip address
 * @return true if match
 * @return false otherwise
 */
bool Router::match(const uint32_t &route, const uint32_t &dst, const uint8_t &prefix_len) {
    uint32_t mask = prefix_len == 32 ? 0 : ~(UINT32_MAX >> prefix_len);
    return (route & mask) == (dst & mask);
}

/**
 * @brief Forward datagram(by "longest-prefix match")
 *  1. find the route with greatest value of prefix_length
 *      call interface(interfacenum).send_datagram(InternetDatagram, const Address &)
 *  2. otherwise (no match)
 *      drop the dgram
 *  3. router also decrement ttl by 1 if match found
 * 
 * @param[in] dgram The datagram to be routed.
 */
void Router::route_one_datagram(InternetDatagram &dgram) {
    const IPv4Header &hdr = dgram.header();
    if(hdr.ttl <= 1) return;
    for(const Route& route : _routes) {
        if(!match(route.route_prefix, hdr.dst, route.prefix_length)) continue;
        dgram.header().ttl--;
        const Address &next_hop = route.next_hop.value_or(Address::from_ipv4_numeric(hdr.dst));
        interface(route.interface_num).send_datagram(dgram, next_hop);
        break;
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
