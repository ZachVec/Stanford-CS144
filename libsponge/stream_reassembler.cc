#include "stream_reassembler.hh"
using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
_output(capacity), _capacity(capacity), stream_len(-1), _unassembled(0), unassembled() {}

size_t StreamReassembler::first_unassembled()  { return _output.bytes_written(); }
size_t StreamReassembler::first_unacceptable() { return _output.bytes_read() + _capacity; }

//! \details This function accecpts a substring, check whether the substring has already been
//! written or beyound the scope of capacity. If that is true, then return false; otherwise
//! return true. If the data has been partially written or beyound the scope, then trim it
//! and return true.
bool StreamReassembler::validate_substr(std::string &data, size_t &index) {
    // return true if [first_unassembled(), first_unacceptable()).
    if(index + data.size() < first_unassembled()) return false;
    if(index >= first_unacceptable()) return false;

    // trim front
    if(index < first_unassembled() && index + data.size() >= first_unassembled()) {
        data = move(data.substr(first_unassembled() - index));
        index = _output.bytes_written();
    }

    // trim back
    if(index < first_unacceptable() && index + data.size() >= first_unacceptable()) {
        data = move(data.substr(0, first_unacceptable() - index));
    }
    return true;
}

//! \details This function does the real work of manipulating member unassembled.
//! \param[in] data mustn't overlap with out terms in unassembled, except for
//! the one with same index
void StreamReassembler::push_available(const std::string &data, const size_t index) {
    auto it = unassembled.find(index);
    if(it == unassembled.end()) {
        _unassembled += data.size();
        unassembled.emplace(index, move(data));
    } else if (!data.empty() && data.size() > it->second.size()) {
        _unassembled += data.size() - it->second.size();
        it->second = move(data);
    }
}

//! \details This function remove all the duplicated part of \param[in] data
//! and call push_available to do the real work of pushing the substring.
//! \param[in] data is guaranteed to be in legal range of capacity.
void StreamReassembler::push_substring(string data, size_t index) {
    std::map<size_t, string>::iterator it = unassembled.lower_bound(index);
    if(it != unassembled.begin()) { // former term exists
        --it;
        const auto &[idx, size] = make_pair(it->first, it->second.size());
        // idx < index holds
        if(index + data.size() < idx + size) return;
        if(index < idx + size) {
            data = move(data.substr(idx + size - index));
            index = idx + size;
        }
    }
    
    if(data.empty()) {
        push_available(data, index);
        return;
    }
    auto hi = unassembled.lower_bound(index + data.size());
    for(it = unassembled.upper_bound(index); it != hi; it++) {
        const auto &[idx, size] = make_pair(it->first, it->second.size());
        // index <= idx holds in any condition
        if(index + data.size() <= idx + size) {
            data = move(data.substr(0, idx - index));
        } else {
            push_available(move(data.substr(0, idx - index)), index);
            data = move(data.substr(idx + size - index));
            index = idx + size;
        }
    }
    push_available(move(data), index);
}

void StreamReassembler::pop_substring() {
    _unassembled -= unassembled.begin()->second.size();
    unassembled.erase(unassembled.begin());
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(_output.input_ended()) return;
    if(eof) stream_len = index + data.size();
    string str = move(data);
    size_t idx = move(index); 

    if(!validate_substr(str, idx)) return;
    push_substring(move(str), idx);
    while(!unassembled.empty() && unassembled.begin()->first == first_unassembled()) {
        _output.write(move(unassembled.begin()->second));
        pop_substring();
        if(_output.bytes_written() == stream_len) {
            _output.end_input();
            break;
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return unassembled.empty() && _output.buffer_empty(); }
