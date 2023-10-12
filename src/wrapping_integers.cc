#include "wrapping_integers.hh"
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 {zero_point + static_cast<uint32_t>(n & 0xFFFFFFFF)};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t residual = this->raw_value_ - zero_point.raw_value_;
  uint32_t l32bits = static_cast<uint32_t>(checkpoint & 0xFFFFFFFF);
  uint32_t diff = residual >= l32bits ? residual - l32bits : l32bits - residual;
  uint64_t result = (checkpoint & 0xFFFFFFFF00000000) + static_cast<uint64_t>(residual);
  if (diff > (1UL << 31)) {
    result = residual >= l32bits ? 
            (result >= (1UL << 32) ? result - (1UL << 32) : result) : 
            result + (1UL << 32);
  }
  return result;
}
