#include "tcp_connection.hh"
#include <iostream>
using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_receive; }

/**
 * @brief This method get called when new segment received. Send segments after receiving.
 * The complicated part is introduced by the dual identity of TCPConnection, i.e.,
 * client or server. This method should behave differently playing different role.
 * 
 * @param seg segment incomming
 */
void TCPConnection::segment_received(const TCPSegment &seg) {
  if(!_active) return;

  // receiving a RST segment, unclean_shutdown
  if(seg.header().rst) {
    unclean_shutdown();
    return;
  }

  // segment without SYN in LISTEN state cannot be received.
  if(_receiver.state() == TCPReceiver::State::LISTEN && _sender.state() == TCPSender::State::CLOSED && !seg.header().syn) {
    return;
  }

  recv_segments(seg);
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
  // no data is allowed to write if stream has ended.
  if(_sender.stream_in().input_ended()) return 0;

  size_t ret = _sender.stream_in().write(data);
  _sender.fill_window();
  send_segments();
  return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  if(!_active) return;

  _time_since_last_receive += ms_since_last_tick;
  _sender.tick(ms_since_last_tick);
  if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
    send_reset();
    unclean_shutdown();
  } else if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED && _time_since_last_receive >= 10 * _cfg.rt_timeout) {
    _linger_after_streams_finish = _active = false;
  } else {
    send_segments();
  }
}

void TCPConnection::end_input_stream() {
  _sender.stream_in().end_input();
  _sender.fill_window();
  send_segments();
}

void TCPConnection::connect() {
  _sender.fill_window();
  send_segments();
}

TCPConnection::~TCPConnection() {
  try {
    if (active()) {
      send_reset();
      unclean_shutdown();
    }
  } catch (const exception &e) {
    std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}

//! \brief obviously, it's a method used to send reset segment
void TCPConnection::send_reset() {
  _sender.send_empty_segment();
  _sender.segments_out().front().header().rst = true;
  _segments_out.emplace(_sender.segments_out().front());
  _sender.segments_out().pop();
}

//! \brief combine segs from sender and ackno&winsize from receiver
//! and push them into segments_out.
void TCPConnection::send_segments() {
  const auto &ackno = _receiver.ackno();
  size_t winsz = _receiver.window_size();
  winsz = std::min(winsz, static_cast<uint64_t>(UINT16_MAX));
  while(!_sender.segments_out().empty()) {
    TCPHeader &header  = _sender.segments_out().front().header();
    header.ack   = ackno.has_value();
    header.ackno = ackno.value_or(WrappingInt32{0});
    header.win   = static_cast<uint16_t>(winsz);
    _segments_out.emplace(_sender.segments_out().front());
    _sender.segments_out().pop();
  }
}

//! \brief this method does the real work of receiving segment and send back
void TCPConnection::recv_segments(const TCPSegment &seg) {
  _time_since_last_receive = 0;
  const auto &hdr = seg.header();
  const auto &rb = _receiver.state(); // receiver state before receive
  const auto &sb = _sender.state();   // sender   state before receive

  //receiving
  _receiver.segment_received(seg);
  if(hdr.ack) _sender.ack_received(hdr.ackno, hdr.win);

  const auto &ra = _receiver.state(); // receiver state after receive
  const auto &sa = _sender.state();   // sender   state after receive

  // if receiving an ack for FIN, no reply
  if(sb == TCPSender::State::FIN_SENT && sa == TCPSender::State::FIN_ACKED && rb == ra) {
    _active = _linger_after_streams_finish;
    return;
  }

  // sending
  _sender.fill_window();
  if(seg.length_in_sequence_space() && _sender.segments_out().empty()) {
    _sender.send_empty_segment();
  }
  send_segments();

  // set _linger to false if necessary
  if(ra == TCPReceiver::State::FIN_RECV && sa == TCPSender::State::SYN_ACKED) {
    _linger_after_streams_finish = false;
  }
}

//! This method gets called when:
//! 1. RST received
//! 2. Retxcounts outnumbered _cfg.MAX_RETX_ATTEMPTS
//! 3. Instance destroied while still active
void TCPConnection::unclean_shutdown() {
  _receiver.stream_out().set_error();
  _sender.stream_in().set_error();
  _active = _linger_after_streams_finish = false;
}
