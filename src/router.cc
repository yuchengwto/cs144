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

  bool overwrite = false;
  for (auto &it: table_) {
    if (it.route_prefix == route_prefix && it.prefix_length == prefix_length) {
      // overwrite
      it.next_hop = next_hop;
      it.interface_num = interface_num;
      overwrite = true;
      break;
    }
  }

  if (!overwrite) {
    // 没得覆盖，直接追加
    this->table_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
  }
}

static bool match_prefix(uint32_t ip, uint32_t prefix, uint8_t prefix_length, uint32_t &match_value) {
  if (prefix_length == 0) {
      match_value = 0;
      return true;
  }

  if (prefix_length >= 32) {
    if (ip == prefix) {
      match_value = ip;
      return true;
    } else {
      return false;
    }
  }

  uint8_t shift_length = 32 - prefix_length;
  if ((ip >> shift_length) == (prefix >> shift_length)) {
    match_value = (ip >> shift_length);
    return true;
  } else {
    return false;
  }
}


void Router::route() {
  // 遍历网卡，看有没有数据报
  for (size_t ii=0; ii<interfaces_.size(); ii++) {

    auto optdg = interface(ii).maybe_receive();
    if (!optdg.has_value()) continue;

    // 处理数据报
    InternetDatagram dg = optdg.value();
    uint32_t dst = dg.header.dst;
    // 到route table中寻找最匹配的route
    size_t match_idx = 0;
    uint32_t match_value = 0;
    uint32_t max_match_value = 0;
    bool is_match = false;
    for (size_t ri=0; ri<table_.size(); ri++) {
      if (match_prefix(dst, table_[ri].route_prefix, table_[ri].prefix_length, match_value)) {
        if (match_value >= max_match_value) {
          is_match = true;
          max_match_value = match_value;
          match_idx = ri;
        }
      }
    }

    // 丢弃数据报
    if (!is_match) continue;
    if (dg.header.ttl <= 1) continue;
    dg.header.ttl --;
    dg.header.compute_checksum();

    // 发送数据报到对应匹配route
    auto rt = table_[match_idx];
    if (!rt.next_hop.has_value()) {
      // direct，发送给dst地址
      this->interface(rt.interface_num).send_datagram(dg, Address::from_ipv4_numeric(dst));
    } else {
      // 转发
      this->interface(rt.interface_num).send_datagram(dg, rt.next_hop.value());
    }

  }


}
