#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <deque>



struct Timer {
  uint64_t time_passed {0};

  bool is_timeout(uint64_t rto) {
    return this->time_passed >= rto;
  }
  void time_pass(uint64_t ms_since_last_tick) {
    this->time_passed += ms_since_last_tick;
  }
};


struct MessageInfo {
  bool already_send {false};
};




class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?

private:
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  Wrap32 ackno_;
  uint64_t window_size_;
  uint64_t checkpoint_;
  bool syn_;
  bool ack_syn_;
  bool fin_;
  uint64_t retrans_count_;
  uint64_t rto_;
  Timer timer_;
  std::deque<std::pair<MessageInfo, TCPSenderMessage>> outstanding_;

  int compare_seqno(Wrap32 left, Wrap32 right) {
    if (left.unwrap(isn_, checkpoint_) < right.unwrap(isn_, checkpoint_)) return -1;
    if (left.unwrap(isn_, checkpoint_) == right.unwrap(isn_, checkpoint_)) return 0;
    return 1;
  }
};
