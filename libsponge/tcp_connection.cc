#include "tcp_connection.hh"
#include <iostream>
using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_receive; }

size_t TCPConnection::remaining_outbound_capacity() const { return {}; }

size_t TCPConnection::bytes_in_flight() const { return {}; }

size_t TCPConnection::unassembled_bytes() const { return {}; }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

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
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

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
      cerr << "Warning: Unclean shutdown of TCPConnection\n";
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
  const auto &winsz = std::min(_receiver.window_size(), static_cast<uint64_t>(UINT16_MAX));
  while(!_sender.segments_out().empty()) {
    TCPSegment &seg    = _sender.segments_out().front();
    seg.header().ack   = ackno.has_value();
    seg.header().ackno = ackno.value_or(WrappingInt32{0});
    seg.header().win   = static_cast<uint16_t>(winsz);
    _segments_out.emplace(seg);
    _sender.segments_out().pop();
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
