#include "tcp_connection.hh"
#include <iostream>
using namespace std;
using RState = TCPReceiver::State;
using SState = TCPSender::State;

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

  const State &state1 = make_pair(_receiver.state(), _sender.state());
  if(!vaild_seg(state1, seg)) return;

  _time_since_last_receive = 0;

  // receiving
  _receiver.segment_received(seg);
  if(seg.header().ack) _sender.ack_received(seg.header().ackno, seg.header().win);

  const State &state2 = make_pair(_receiver.state(), _sender.state());
  if(state1 == make_pair(RState::FIN_RECV, SState::FIN_SENT) && state2 == make_pair(RState::FIN_RECV, SState::FIN_ACKED) && !_linger_after_streams_finish) {
    // if transit from state `LAST_ACK` to `CLOSED`, set _active to false
    _active = false;
  }
  if(!reply_seg(state1, state2, seg)) return;

  // replying
  _sender.fill_window();
  if(seg.length_in_sequence_space() && _sender.segments_out().empty()) {
    _sender.send_empty_segment();
  }
  send_segments();

  // if connection transit into state `CLOSE_WAIT`, which means passive close
  if(state2 == make_pair(RState::FIN_RECV, SState::SYN_ACKED)) {
    _linger_after_streams_finish = false;
  }
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

  const State &state = make_pair(_receiver.state(), _sender.state());
  _time_since_last_receive += ms_since_last_tick;
  _sender.tick(ms_since_last_tick);
  if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
    send_reset();
    unclean_shutdown();
  } else if (state == make_pair(RState::FIN_RECV, SState::FIN_ACKED) && _time_since_last_receive >= 10 * _cfg.rt_timeout) {
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

//! This method gets called when:
//! 1. RST received
//! 2. Retxcounts outnumbered _cfg.MAX_RETX_ATTEMPTS
//! 3. Instance destroied while still active
void TCPConnection::unclean_shutdown() {
  _receiver.stream_out().set_error();
  _sender.stream_in().set_error();
  _active = _linger_after_streams_finish = false;
}


bool TCPConnection::vaild_seg(const State &state, const TCPSegment &seg) {
  if(seg.header().rst) {
    // receiving a RST segment, unclean_shutdown
    unclean_shutdown();
    return false;
  } else if (state == make_pair(RState::LISTEN, SState::CLOSED) && !seg.header().syn) {
    // segment without SYN in LISTEN state cannot be received.
    return false;
  }
  return true;
}

bool TCPConnection::reply_seg(const State &state1, const State &state2, const TCPSegment &seg) {
  // receiving an empty seg with ack for FIN, no reply
  if(state1.second == SState::FIN_SENT && state2.second == SState::FIN_ACKED && !seg.length_in_sequence_space()) {
    return false;
  }
  return true;
}