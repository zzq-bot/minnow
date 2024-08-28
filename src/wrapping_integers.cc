#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { ( static_cast<uint32_t>( n ) + zero_point.raw_value_ ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t base = this->raw_value_ - zero_point.raw_value_;
  if ( base >= checkpoint ) {
    return base;
  }
  uint64_t bias = 1ULL << 32;
  uint64_t bias_num = ( checkpoint - base + ( bias >> 1 ) ) / bias;
  return base + bias_num * bias;
}
