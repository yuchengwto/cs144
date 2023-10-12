#include "reassembler.hh"
#include <iostream>
#include <sstream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if (_bitmap.empty()) {
    // 初始化
    _bitmap = std::deque<bool>(output.available_capacity(), false);
    _pending = std::deque<char>(output.available_capacity(), '\0');
  }
  if (output.is_closed()) return;
  if (is_last_substring) _eof = true;
  uint64_t len = data.size();
  uint64_t real_len;  // trim后的数据长度
  uint64_t offset;    // 相对于base的偏移
  if (len == 0 && _eof && bytes_pending() == 0) {
    output.close();
    return;
  }
  if (first_index >= _uass_base + output.available_capacity()) return;
  if (first_index >= _uass_base) {
    // 需要pending
    offset = first_index - _uass_base;
    real_len = min(len, output.available_capacity() - offset);
    if (real_len < len) _eof = false; // 数据未完
    for (uint64_t i=0; i<real_len; i++) {
      if (_bitmap[i + offset]) continue;
      _pending[i + offset] = data[i];
      _bitmap[i + offset] = true;
    }
  } else if (first_index + len > _uass_base) {
    // 存在有效数据
    offset = _uass_base - first_index;
    real_len = min(len - offset, output.available_capacity());
    if (real_len < len - offset) _eof = false; // 数据未完
    for (uint64_t i=0; i<real_len; i++) {
      if (_bitmap[i]) continue;
      _bitmap[i] = true;
      _pending[i] = data[i + offset];
    }
  }
  else {
    // 过期
  }

  _try_push(output);
  if (_eof && bytes_pending() == 0) {
    output.close();
  }
}

void Reassembler::_try_push(Writer& output) {
  std::stringstream ss;
  while (!_bitmap.empty() && _bitmap.front()) {
    ss << _pending.front();
    _pending.pop_front();
    _bitmap.pop_front();
    _pending.push_back('\0');
    _bitmap.push_back(false);
  }
  std::string tmp = ss.str();
  if (!tmp.empty()) {
    output.push(tmp);
    _uass_base += tmp.size();
  }
}



uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  uint64_t count = 0;
  for (const auto& i: _bitmap) {
    count += i;
  }
  return count;
}
