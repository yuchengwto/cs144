#include "tcp_receiver.hh"
#include <random>
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN && !_syn) {
    // 设置ISN
    _isn = Wrap32(message.seqno);
    _syn = true;
  }
  if (!_syn) return;
  uint64_t abs_no = message.seqno.unwrap(_isn, reassembler.get_unass_base());
  if (!message.SYN && abs_no == 0) return;
  uint64_t stream_no = abs_no > 0 ? abs_no - 1 : 0;
  std::string &data = message.payload;
  reassembler.insert(stream_no, data, message.FIN, inbound_stream);
  _fin = inbound_stream.is_closed();
  std::cout << reassembler.get_unass_base() << "\t" << abs_no << "\t" << stream_no << std::endl;
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  uint16_t window_size_ = inbound_stream.available_capacity() < UINT16_MAX ? inbound_stream.available_capacity() : UINT16_MAX;
  if (_syn) {
    Wrap32 ackno_ = Wrap32::wrap(inbound_stream.bytes_pushed(), _isn) + 1 + _fin;
    std::cout << inbound_stream.bytes_pushed() << "\t" << window_size_ << std::endl;
    return TCPReceiverMessage {
        ackno_,
        window_size_
      };
  } else {
    return TCPReceiverMessage {
        std::nullopt,
        window_size_
      };
  }
;
}