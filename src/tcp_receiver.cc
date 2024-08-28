#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }

  if ( !received_syn_ ) {
    if ( message.SYN ) {
      zero_point_ = message.seqno;
      received_syn_ = true;
    } else {
      return;
    }
  }
  // write message into reassembler

  uint64_t absolute_seqno = message.seqno.unwrap( zero_point_, writer().bytes_pushed() );
  reassembler_.insert( absolute_seqno - 1 + message.SYN, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage recv_msg {};
  recv_msg.window_size
    = static_cast<uint16_t>( std::min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  recv_msg.RST = writer().has_error();

  if ( received_syn_ ) {
    uint64_t absolute_ack_no = writer().bytes_pushed() + 1; // add 1 because SYN occupies one;
    if ( writer().is_closed() ) {
      absolute_ack_no++;
    }
    recv_msg.ackno = Wrap32::wrap( absolute_ack_no, zero_point_ );
  } else {
    recv_msg.ackno = std::nullopt;
  }
  return recv_msg;
}
