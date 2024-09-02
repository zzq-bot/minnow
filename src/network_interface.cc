#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  IPADDR_TYPE next_hop_ip = next_hop.ipv4_numeric();
  if ( arp_table_.contains(next_hop_ip) ) {
    // shall send it right away
    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = this->ethernet_address_;
    frame.header.dst = arp_table_[next_hop_ip].eth_addr;
    frame.payload = serialize(dgram);
    transmit(frame);
  } else {
    // broadcast ARP request for next_hop_ip
    if ( broadcast_table_.contains(next_hop_ip) ) {
      // already broadcasted
      return;
    }
    ARPMessage arp_request;
    arp_request.opcode = ARPMessage::OPCODE_REQUEST;
    arp_request.sender_ethernet_address = this->ethernet_address_;
    arp_request.sender_ip_address = this->ip_address_.ipv4_numeric();
    arp_request.target_ethernet_address = {}; // unknown ,set all zeros
    arp_request.target_ip_address = next_hop_ip;

    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.src = this->ethernet_address_;
    frame.header.dst = ETHERNET_BROADCAST;
    frame.payload = serialize(arp_request);
    transmit(frame);

    // broadcast_table_[next_hop_ip] = {dgram, 0}; // mark as broadcasted with timer 0
    broadcast_table_[next_hop_ip].first.emplace_back(dgram);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  EthernetHeader header = frame.header;
  // EthernetAddress src = header.src;
  EthernetAddress dst = header.dst;
  uint16_t type = header.type;

  if ( dst != this-> ethernet_address_ && dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse(dgram, frame.payload) ) {
      datagrams_received_.push(dgram);
    }
  } else if ( type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_message;
    if ( parse(arp_message, frame.payload) ) {
      // get the sender's IP addr and Ethernet addr; record it into the ARP table
      IPADDR_TYPE sender_ip = arp_message.sender_ip_address;
      EthernetAddress sender_eth = arp_message.sender_ethernet_address;
      arp_table_[sender_ip] = {sender_eth, 0};
      // If it requests for my IP address, send an appropriate ARP reply
      if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST && this->ip_address_.ipv4_numeric() == arp_message.target_ip_address ) {
        ARPMessage arp_reply;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = this->ethernet_address_;
        arp_reply.sender_ip_address = this->ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = sender_eth;
        arp_reply.target_ip_address = sender_ip;

        EthernetFrame reply_frame;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.header.src = this->ethernet_address_;
        reply_frame.header.dst = sender_eth;
        reply_frame.payload = serialize(arp_reply);
        transmit(reply_frame);
      } else if ( broadcast_table_.contains(sender_ip) ) {
        // transmit it 
        // std::cout << "DEBUG: Here transmit the datagram with src ethernet address " << to_string(this->ethernet_address_) << " and dst ethernet address " << to_string(arp_table_[sender_ip].eth_addr) << std::endl;
        // EthernetFrame new_frame;
        // new_frame.header.type = EthernetHeader::TYPE_IPv4;
        // new_frame.header.src = this->ethernet_address_;
        // new_frame.header.dst = sender_eth;
        // new_frame.payload = serialize(broadcast_table_[sender_ip].dgram);
        // transmit(new_frame);

        for (auto dgram : broadcast_table_[sender_ip].first) {
          EthernetFrame new_frame;
          new_frame.header.type = EthernetHeader::TYPE_IPv4;
          new_frame.header.src = this->ethernet_address_;
          new_frame.header.dst = sender_eth;
          new_frame.payload = serialize(dgram);
          transmit(new_frame);
        }
        broadcast_table_.erase(sender_ip);
        
      }
    }
  
  }

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // wrong way to update the timer for arp_table_
  // for ( auto& [ip, value] : arp_table_ ) {
  //   value.timer += ms_since_last_tick;
  //   if ( value.timer >= MAX_ARP_HOLD_T ) {
  //     arp_table_.erase(ip);
  //   }
  // }
  for (auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    it->second.timer += ms_since_last_tick;
    if (it->second.timer >= MAX_ARP_HOLD_T) {
      it = arp_table_.erase(it); 
    } else {
      ++it; 
    }
  }

  // update the broadcast_table_
  for (auto it = broadcast_table_.begin(); it != broadcast_table_.end(); ) {
    // it->second.timer += ms_since_last_tick;
    // if (it->second.timer >= MAX_WAIT_BROADCAST_T) {
    
    it->second.second += ms_since_last_tick;
    if (it->second.second >= MAX_WAIT_BROADCAST_T) {
      it = broadcast_table_.erase(it); 
    } else {
      ++it; 
    }
  }
}
