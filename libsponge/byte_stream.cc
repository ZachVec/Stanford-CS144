#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity):
buf(capacity, '\0'), _cap(capacity), reader(0), writer(0), _eof(false), _error(false) {}

size_t ByteStream::write(const string &data) {
    size_t i;
    for(i = 0; i < data.size() && buffer_size() < this->_cap; i++){
        this->buf[this->writer%this->_cap] = data[i];
        this->writer += 1;
    }
    return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t valid_len = min(len, buffer_size());
    size_t reader_index = this->reader % this->_cap;
    size_t writer_index = this->writer % this->_cap;

    if(reader_index < writer_index) {
        return this->buf.substr(reader_index, valid_len);
    } else if (valid_len < this->_cap-reader_index){
        return this->buf.substr(reader_index, valid_len);
    } else {
        size_t remain_len = valid_len - (this->_cap - reader_index);
        return this->buf.substr(reader_index) + this->buf.substr(0, remain_len);
    }
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    this->reader += min(len, buffer_size());
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { this->_eof = true; }

bool ByteStream::input_ended() const { return this->_eof; }

size_t ByteStream::buffer_size() const { return this->writer - this->reader; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return this->writer; }

size_t ByteStream::bytes_read() const { return this->reader; }

size_t ByteStream::remaining_capacity() const { return this->_cap - buffer_size(); }
