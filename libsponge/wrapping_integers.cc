#include "wrapping_integers.hh"
using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // absolute seqno(start at 0) -> seqno(start at isn)
    return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // seqno(start at isn) -> absolute seqno(start at 0), depending on checkpoint
    static constexpr uint64_t span = 1ul << 31;
    uint64_t offset = static_cast<uint64_t>(n.raw_value()-isn.raw_value());

    if(checkpoint < span) return offset;
    uint64_t target = (checkpoint & ~static_cast<uint64_t>(UINT32_MAX)) + offset;
    if(target >= checkpoint + span) return target - (span << 1);
    if(target <  checkpoint - span) return target + (span << 1);
    return target;
}
