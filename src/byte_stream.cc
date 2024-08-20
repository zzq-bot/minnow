#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( error_ || closed_ || data.empty() ) {
    return;
  }
  const size_t len = min( available_capacity(), data.size() );
  queue_.append( data.substr( 0, len ) );
  pushed_len_ += len;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - pushed_len_ + popped_len_;
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_len_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ && queue_.empty();
}

uint64_t Reader::bytes_popped() const
{
  return popped_len_;
}

string_view Reader::peek() const
{
  return { queue_ }; // 返回临时对象，隐式修改
}

void Reader::pop( uint64_t len )
{
  if ( error_ || queue_.empty() || len == 0 ) {
    return;
  }

  len = min( len, bytes_buffered() );
  queue_.erase( 0, len );
  popped_len_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return pushed_len_ - popped_len_;
}
