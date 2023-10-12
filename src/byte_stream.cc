#include <stdexcept>
#include <iostream>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : 
capacity_( capacity ),
total_byte_in(0),
total_byte_out(0),
occupied_byte(0),
is_close(false),
_has_error(false)
{
}

void Writer::push( string data )
{
  if (is_closed()) return;
  auto ac = available_capacity();
  auto dc = data.size();
  if (ac <= 0) return;
  if (ac >= dc) {
    // 全写入
    for (size_t i=0; i<dc; i++) {
      this->buffer.push_back(data[i]);
    }
    occupied_byte += dc;
    total_byte_in += dc;
  } else {
    // 部分写入
    for (size_t i=0; i<ac; i++) {
      this->buffer.push_back(data[i]);
    }
    occupied_byte = capacity_;
    total_byte_in += ac;
  }
}

void Writer::close()
{
  is_close = true;
}

void Writer::set_error()
{
  _has_error = true;
}

bool Writer::is_closed() const
{
  return is_close;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - occupied_byte;
}

uint64_t Writer::bytes_pushed() const
{
  return total_byte_in;
}

string_view Reader::peek() const
{
  if (occupied_byte > 0) {
    return string_view(buffer);
  } else {
    return {};
  }
}

bool Reader::is_finished() const
{
  return (is_close && occupied_byte == 0);
}

bool Reader::has_error() const
{
  return _has_error;
}

void Reader::pop( uint64_t len )
{
  buffer = buffer.substr(len);
  occupied_byte -= len;
  total_byte_out += len;
}

uint64_t Reader::bytes_buffered() const
{
  return occupied_byte;
}

uint64_t Reader::bytes_popped() const
{
  return total_byte_out;
}
