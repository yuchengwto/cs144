#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
#include <iostream>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  :
   isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , ackno_(0)
  , window_size_(1)
  , checkpoint_(0)
  , syn_(false)
  , ack_syn_(false)
  , fin_(false)
  , retrans_count_(0)
  , rto_(initial_RTO_ms)
  , timer_()
  , outstanding_({})
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t n = 0;
  for (const auto& p: outstanding_) {
    n += p.second.sequence_length();
  }
  return n;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_count_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  std::cout << "[MAYBESEND] try to send, outstanding queue size: " << outstanding_.size() 
            << std::endl;

  TCPSenderMessage t{};
  bool has_send = false;
  for (auto it=outstanding_.begin(); it!=outstanding_.end(); it++) {
    if (!it->first.already_send) {
      t = it->second;
      it->first.already_send = true;
      has_send = true;
      break;
    }
  }
  if (has_send) {
    std::cout << "send one\n";
    return t;
  } else {
    std::cout << "send nullopt\n";
    return std::nullopt;
  }
}

void TCPSender::push( Reader& outbound_stream )
{
  std::cout << "[PUSH] syn state: " << syn_ 
            << "\tack syn state: " << ack_syn_
            << "\tfin state: " << fin_
            << std::endl;
  if (fin_) return;
  if (!syn_) {
    std::cout << "push syn data" << std::endl; 
    TCPSenderMessage t{};
    t.seqno = isn_;
    t.SYN = true;
    t.FIN = outbound_stream.is_finished();
    this->syn_ = true;
    if (t.FIN) fin_ = true;
    outstanding_.push_back(std::make_pair(MessageInfo(), t));
    checkpoint_ += (t.SYN + t.FIN);
    return;
  }
  if (!ack_syn_) return;

  uint64_t window_size = window_size_; 
  if (window_size == 0) {
    window_size = 1;
    std::cout << "pretend window size == 1" << std::endl; 
  }

  // 逐个生成message直到用尽容量
  uint64_t available_size = window_size > this->sequence_numbers_in_flight() ? window_size - this->sequence_numbers_in_flight() : 0;
  std::cout << "available size: " << available_size << std::endl;
  uint64_t sent_size = 0;
  uint64_t pack_size = 0;
  while (available_size > 0) {
    TCPSenderMessage t{};
    t.seqno = isn_ + checkpoint_;
    bool is_closed = outbound_stream.is_finished();
    if (is_closed) {
      fin_ = true;
      t.FIN = true;
      sent_size += (t.SYN + t.FIN);
      checkpoint_ += (t.SYN + t.FIN);
      pack_size ++;
      outstanding_.push_back(std::make_pair(MessageInfo(), t));
      break;
    }

    auto view = outbound_stream.peek();
    uint64_t send_size = std::min(std::min(view.size(), available_size), TCPConfig::MAX_PAYLOAD_SIZE);
    if (send_size == 0) break;
    auto data_view = view.substr(0, send_size);
    std::string data(data_view);
    outbound_stream.pop(send_size);
    t.payload = data;
    is_closed = outbound_stream.is_finished();
    bool data_with_fin = false;
    if (is_closed && available_size > send_size) {
      data_with_fin = true;
      fin_ = true;
      t.FIN = true;
      send_size ++;
    }

    sent_size += send_size;
    checkpoint_ += send_size;
    pack_size ++;
    available_size -= send_size;
    outstanding_.push_back(std::make_pair(MessageInfo(), t));
    if (data_with_fin) break;
  }
  std::cout << "send " << sent_size << " bytes and pack size " << pack_size << std::endl;
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  std::cout << "[EMPTYSEND]\n";
  TCPSenderMessage t{};
  t.seqno = isn_ + checkpoint_;
  t.payload = std::string("");
  return t;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  std::cout << "[RECV]\n";
  if (msg.ackno.has_value()) {
    if (!ack_syn_ && msg.ackno.value() == isn_ + 1) {
      // 第一次响应同步
      std::cout << "first sync, window_size: " << msg.window_size << std::endl;
      this->ack_syn_ = true;
      this->ackno_ = msg.ackno.value();
      this->window_size_ = msg.window_size;
    } else {
      int comp = compare_seqno(ackno_, msg.ackno.value());
      std::cout << "compare result: " << comp << "\n";
      if (comp <= 0) {
        // 仅更新window size
        std::cout << "set window size: " << msg.window_size << std::endl;
        this->window_size_ = msg.window_size;
      }
      if (comp < 0) {
        // 数据有效更新
        this->ackno_ = msg.ackno.value();
        rto_ = initial_RTO_ms_;
        // 重置timer and count
        retrans_count_ = 0;
        timer_.time_passed = 0;
      }
    }
  }

  if (!ack_syn_) return;
  // 遍历outstanding，丢掉ack数据包
  for (auto it=outstanding_.begin(); it!=outstanding_.end();) {
    if (it->first.already_send && compare_seqno(ackno_, (it->second.seqno + it->second.sequence_length())) >= 0 ) {
      // 已经ack，丢掉
      std::cout << "drop ack data" << std::endl;
      it = outstanding_.erase(it);
      std::cout << "outstanding size after drop: " << outstanding_.size() << std::endl;
    } else {
      it ++;
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  std::cout << "[TICK] for ms " << ms_since_last_tick << std::endl;

  for (auto it=outstanding_.begin(); it!=outstanding_.end();) {
    // 由远及近遍历outstanding，找出timeout数据包并重发
    if (ack_syn_ && it->first.already_send && compare_seqno(ackno_, (it->second.seqno + it->second.sequence_length())) >= 0) {
      // 已经ack，丢掉
      std::cout << "old data, drop" << std::endl;
      it = outstanding_.erase(it);
      std::cout << "outstanding size after drop: " << outstanding_.size() << std::endl;
    } else {
      it ++;
    }
  }


  timer_.time_pass(ms_since_last_tick);
  if (timer_.is_timeout(rto_)) {
    // timeout
    std::cout << "timeout, resend, time passed: " << timer_.time_passed
              << "\trto: " << rto_
              << std::endl;
    for (auto it=outstanding_.begin(); it!=outstanding_.end(); it++) {
      if (it->first.already_send) {
        it->first.already_send = false;
        timer_.time_passed = 0;
        if (!ack_syn_ || window_size_ > 0) {
          this->retrans_count_ ++;
          rto_ *= 2;
          std::cout << "double rto and increase retrans count, now rto: " << rto_
                    << "\tretrans count: " << retrans_count_
                    << std::endl;
        }
        break;
      }
    }
  }
}
