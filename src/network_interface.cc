#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{

  auto ip_key = next_hop.ipv4_numeric();
  if (arp_map_.find(ip_key) == arp_map_.end()) {
    // ARP launch!
    // and queue
    // 判断是否5s内已经有发送arp
    if (arp_timer_.find(ip_key) != arp_timer_.end() && !arp_timer_[ip_key].is_timeout(5000) ) {
      // 不再发送arp
    } else {
      EthernetFrame ef{};
      ef.header.type = EthernetHeader::TYPE_ARP;
      ef.header.src = this->ethernet_address_;
      ef.header.dst = ETHERNET_BROADCAST;
      ARPMessage am{};
      am.opcode = ARPMessage::OPCODE_REQUEST;
      am.sender_ethernet_address = this->ethernet_address_;
      am.sender_ip_address = this->ip_address_.ipv4_numeric();
      am.target_ip_address = ip_key;
      ef.payload = serialize(am);
      this->queue_.push_back(std::make_pair(ip_key, ef));
    }
  }

  // queue the dgram without dst
  // dst will assign at maybe_send function
  EthernetFrame ef{};
  ef.header.type = EthernetHeader::TYPE_IPv4;
  ef.header.src = this->ethernet_address_;
  ef.payload = serialize(dgram);
  this->queue_.push_back(std::make_pair(ip_key, ef));
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != this->ethernet_address_) {
    return std::nullopt;
  }

  uint32_t ip_key;
  uint32_t dst_ip;
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage am{};
    if (!parse(am, frame.payload)) return std::nullopt;
    ip_key = am.sender_ip_address;
    dst_ip = am.target_ip_address;
    if (dst_ip != this->ip_address_.ipv4_numeric()) return std::nullopt;
    arp_map_[ip_key] = am.sender_ethernet_address;
    flush_timer(ip_key);

    if (am.opcode == ARPMessage::OPCODE_REQUEST) {
      // reply the request
      EthernetFrame ef{};
      ef.header.type = EthernetHeader::TYPE_ARP;
      ef.header.src = this->ethernet_address_;
      ef.header.dst = frame.header.src;
      ARPMessage replyam{};
      replyam.opcode = ARPMessage::OPCODE_REPLY;
      replyam.sender_ethernet_address = this->ethernet_address_;
      replyam.sender_ip_address = this->ip_address_.ipv4_numeric();
      replyam.target_ethernet_address = frame.header.src;
      replyam.target_ip_address = ip_key;
      ef.payload = serialize(replyam);
      this->queue_.push_back(std::make_pair(ip_key, ef));
    }
    return std::nullopt;
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram id{};
    if (!parse(id, frame.payload)) return std::nullopt;
    return id;
  }

  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  (void)ms_since_last_tick;
  for (auto it=arp_timer_.begin(); it!=arp_timer_.end(); it++) {
    // 加钟
    it->second.time_pass(ms_since_last_tick);
  }
  
  for (auto it=arp_timer_.begin(); it!=arp_timer_.end(); ) {
    if (it->second.is_timeout(30000)) {
      // 淘汰过期ip
      if (arp_map_.find(it->first) != arp_map_.end()) {
        // delete in arp map
        arp_map_.erase(it->first);
      }

      // delete in arp timer
      it = arp_timer_.erase(it);
    } else {
      it ++;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  // 遍历queue，找到可以发送的frame
  uint32_t ip_key;
  uint16_t t;
  for (auto it=queue_.begin(); it!=queue_.end(); ) {
    ip_key = it->first;
    t = it->second.header.type;
    if (t == EthernetHeader::TYPE_ARP) {
      // arp request
      EthernetFrame ef = it->second;
      queue_.erase(it);
      flush_timer(ip_key);
      return ef;
    }

    if (t == EthernetHeader::TYPE_IPv4) {
      // ip datagram
      if (arp_map_.find(ip_key) == arp_map_.end()) {
        // 跳过
        it++;
        continue;
      } else {
        // 发送
        EthernetFrame ef = it->second;
        ef.header.dst = arp_map_[ip_key];
        queue_.erase(it);
        return ef;
      }
    }

    it++;
  }

  return std::nullopt;
}
