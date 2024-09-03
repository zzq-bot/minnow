#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  routing_table_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // check all datagrams received by datagrams
  for ( const auto& interface : _interfaces ) {
    // go over datagram_received
    auto& dgrams = interface->datagrams_received();
    while ( !dgrams.empty() ) {
      auto dgram = dgrams.front();
      dgrams.pop();

      if ( dgram.header.ttl-- <= 1 ) {
        continue;
      }
      dgram.header.compute_checksum();
      const uint32_t dst_ipv4 = dgram.header.dst;
      // route the dgram
      bool flag = false;
      uint8_t max_prefix_length = 0;
      optional<Address> best_matched_next_hop = {};
      size_t best_matched_interface_num = 0;

      // find best matched corresponding next hop and interface
      for ( auto it = routing_table_.begin(); it != routing_table_.end(); it = next( it ) ) {
        if ( it->prefix_length == 0 ) {
          if ( !flag ) {
            max_prefix_length = 0;
            best_matched_next_hop = it->next_hop;
            best_matched_interface_num = it->interface_num;
            flag = true;
          }
        } else {
          if ( ( it->route_prefix >> ( 32 - it->prefix_length ) ) == ( dst_ipv4 >> ( 32 - it->prefix_length ) )
               && it->prefix_length > max_prefix_length ) {
            max_prefix_length = it->prefix_length;
            best_matched_next_hop = it->next_hop;
            best_matched_interface_num = it->interface_num;
            flag = true;
          }
        }
      }

      if ( flag ) {
        // transmit
        if ( best_matched_next_hop.has_value() ) {
          this->interface( best_matched_interface_num )->send_datagram( dgram, best_matched_next_hop.value() );
        } else {
          this->interface( best_matched_interface_num )
            ->send_datagram( dgram, Address::from_ipv4_numeric( dst_ipv4 ) );
        }
      }
    }
  }
}
