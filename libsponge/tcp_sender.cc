#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <random>
using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout, [](uint16_t val) -> uint16_t { return 2 * val; }) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_ackno; }

void TCPSender::fill_window() {
    // don't fill if FIN has been sent
    if(fin_sent()) return;

    // fill the window
    size_t target_size = std::max(1ul, _window_size) - bytes_in_flight(); // need this many characters
    for(size_t seglen; target_size > 0; target_size -= seglen) {
        // get a segment out of _stream
        const TCPSegment &segment = get_segment(target_size);

        // if no more data to be read but _stream not ended yet,
        // break and DO NOT send an empty segment
        if((seglen = segment.length_in_sequence_space()) == 0) break;

        _next_seqno += seglen;
        _outstanding.emplace(_next_seqno, segment);
        _segments_out.emplace(segment);
        _timer.turnon();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    const uint64_t abseqno_ack = unwrap(ackno, _isn, _last_ackno);

    // abseqno_ack in [_last_ackno, _next_seqno] is valid.
    if(abseqno_ack > _next_seqno) { // invalid: acknowleged data that haven't been sent
        return;
    } else if(abseqno_ack < _last_ackno) { // invalid: acknowledged data that have been acknowledged.
        return;
    } else if(abseqno_ack == _last_ackno){ // valid: no new data acknowledged, but probably a greater window size
        _window_size = static_cast<uint64_t>(window_size);
    } else { // valid: new data have been acknowledged.
        //update _window_size and _last_ackno
        _window_size = static_cast<uint64_t>(window_size);
        _last_ackno = abseqno_ack;

        // erase those have been fully acknowleged.
        while(!_outstanding.empty() && _outstanding.begin()->first <= abseqno_ack) {
            _outstanding.erase(_outstanding.begin());
        }

        // reset timer and retransmission counter
        _retxcounter = 0;
        _timer.reset(_initial_retransmission_timeout); // reset would turn off the _timer
        if(!_outstanding.empty()) _timer.turnon();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);

    if(!_timer.goesoff() || _outstanding.empty()) return;

    //! retransmit earliest segment that hasn't been fully acknowleged if it exists, abort otherwise.
    _segments_out.emplace(_outstanding.begin()->second);

    // only if receiver wish to get this segment can this sending be considered as retransmission.
    if(_window_size) {
        _retxcounter++;
        _timer.backoff();
    }
    _timer.turnon();
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retxcounter; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.emplace(segment);
}

//! \brief get a segment of size up to `size` from ByteStream
TCPSegment TCPSender::get_segment(size_t segsize) {
    TCPSegment segment;
    if(fin_sent()) {
        return segment;
    } else if(_next_seqno == 0) {
        // if SYN datagram hasn't been sent, then set SYN flag
        segment.header().syn   = true;
        segment.header().seqno = wrap(_next_seqno, _isn);
    } else { // _stream might be not ended, and probably be written into afterwards.
        // TCPConfig::MAX_PAYLOAD_SIZE only limits PAYLOAD size, not the segment size
        // which means even if the payload.size() == TCPConfig::MAX_PAYLOAD_SIZE
        // FIN flag COULD be set if necessary.
        segment.payload()      = _stream.read(min(segsize, TCPConfig::MAX_PAYLOAD_SIZE));
        segment.header().fin   = segment.payload().size() < segsize && _stream.eof();
        segment.header().seqno = wrap(_next_seqno, _isn);
    }
    return segment;
}
