#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return send_cnt_ - ack_cnt_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retx_attempts_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint16_t corrected_window_size = window_size_ == 0 ? 1 : window_size_;
  while ( static_cast<uint64_t>( corrected_window_size ) > sequence_numbers_in_flight() ) {
    auto message = make_empty_message();
    if ( message.RST ) {
      transmit( message );
      return;
    }
    // set SYN, FIN, payload of message
    if ( !is_syn_ ) {
      message.SYN = true;
      is_syn_ = true;
    }
    // get maximum payload size
    uint64_t max_packet_size = static_cast<uint64_t>( corrected_window_size ) - sequence_numbers_in_flight();
    uint64_t max_payload_size_ = min( max_packet_size, TCPConfig::MAX_PAYLOAD_SIZE );
    while ( message.sequence_length() < max_payload_size_ && reader().bytes_buffered() ) {
      uint64_t len = min( max_payload_size_ - message.sequence_length(), reader().bytes_buffered() );
      auto peeked_data = reader().peek();
      message.payload.append( peeked_data.substr( 0, len ) );
      input_.reader().pop( len );
    }
    // set FIN flag
    if ( !is_fin_ && reader().is_finished() && message.sequence_length() < max_packet_size ) {
      message.FIN = true;
      is_fin_ = true;
    }
    if ( message.sequence_length() == 0 ) {
      break;
    }
    transmit( message );
    send_queue_.emplace( message );
    send_cnt_ += message.sequence_length();
    if ( !is_timer_on ) {
      is_timer_on = true;
      timer_ = 0;
    }
    if ( is_fin_ ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // set seqno to send_cnt_ + isn_
  Wrap32 seqno = Wrap32::wrap( send_cnt_, isn_ );
  TCPSenderMessage msg { seqno, false, {}, false, input_.has_error() };
  return TCPSenderMessage { seqno, false, {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  if ( msg.ackno.has_value() ) {

    uint64_t absolute_ackno = msg.ackno.value().unwrap( isn_, send_cnt_ );

    while ( !send_queue_.empty() ) {
      if ( absolute_ackno > send_cnt_ ) {
        break;
      }

      auto cur_msg = send_queue_.front();
      uint64_t msg_seqno = cur_msg.seqno.unwrap( isn_, send_cnt_ );

      if ( msg_seqno + cur_msg.sequence_length() > absolute_ackno ) {
        break;
      }
      ack_cnt_ += cur_msg.sequence_length();
      send_queue_.pop();
      current_RTO_ms_ = initial_RTO_ms_;
      timer_ = 0;
      retx_attempts_ = 0;

      if ( send_queue_.empty() ) {
        is_timer_on = false;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer_ += ms_since_last_tick;

  if ( timer_ >= current_RTO_ms_ ) {
    if ( !send_queue_.empty() ) {
      transmit( send_queue_.front() );
      if ( window_size_ > 0 ) { // if zero: means receiver's window is full, should not resend now.
        retx_attempts_++;
        current_RTO_ms_ *= 2;
      }
      timer_ = 0;
    }
  }
}
