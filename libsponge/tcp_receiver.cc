#include "tcp_receiver.hh"
using namespace std;

//! \brief this method needs to set ISN if necessary and push the payload into reassembler.
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // return if cannot translate
    const auto &hdr = seg.header();
    if(!_isn.has_value() && !hdr.syn) return;
    const auto &pld = seg.payload().copy();
    const auto ckpt = stream_out().bytes_written();
    if(hdr.syn) {
        _isn = hdr.seqno;
        const uint64_t abs_seqno = unwrap(hdr.seqno+1, _isn.value(), ckpt); // +1 for SYN
        _reassembler.push_substring(pld, abs_seqno-1, hdr.fin);
    } else {
        const uint64_t abs_seqno = unwrap(hdr.seqno, _isn.value(), ckpt);
        _reassembler.push_substring(pld, abs_seqno-1, hdr.fin);
    }
}

//! \brief find next seqno that receiver interested in.
//! \returns WrappingInt32, containing SYN and FIN.
optional<WrappingInt32> TCPReceiver::ackno() const {
    const size_t abs_seqno = stream_out().bytes_written() + 1;
    if(!_isn.has_value()) { // not even start.
        return {};
    } else if(!stream_out().input_ended()) { // not yet ended
        return wrap(abs_seqno, _isn.value());
    } else { // ended. add 1 for FIN
        return wrap(abs_seqno, _isn.value()) + 1;
    }
}

//! \brief find the distance between first unassembled and first unacceptable.
//! It's apparent that _capacity = stream_out().buffer_size() + window_size.
//! so \returns _capacity - stream_out().buffer_size()
size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
