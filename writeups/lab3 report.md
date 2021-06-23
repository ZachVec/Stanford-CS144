# Lab3: the TCP sender

本实验为CS144的第四个实验。本实验除了C++之外，还需深刻理解[指导手册](https://cs144.github.io/assignments/lab3.pdf)中提到的概念或过程。

## 实验环境

系统：Ubuntu20.04 LTS

机器：阿里云ECS

实例规格：ecs.t5-lc1m2.small

## 排坑指南

由于指导手册对于函数行为描述的有些模糊，本文在此附上一些个人理解。在梳理之前，我们先定义一些词（可以先看后边）：

- Sender：本文中特指TCP Sender
- Receiver：本文中特指TCP Receiver
- Header：本文特指TCP的报头。
- Payload：本文特指TCP的数据段。
- Segment：由 TCP Header 及 Payload 组成。
- ack：指Receiver确认接收到。
- ackno：本文中为一个absolute seqno，指向下一个待读取的字节。
- rto：本文中 **r**etransmission **t**ime**o**ut，即超时重发的时间。不特指初始值。
- outstanding：意为悬而未决的。本文特指Sender发出，但Receiver还没有ack。
- window size：与[Lab 2: the TCP receiver](https://cs144.github.io/assignments/lab2.pdf)中的window size一样，指接收方接受窗口大小。即总容量减去已写未读的容量。
- timer：计时器。用来计时，以进行超时重发。

先说明Timer没有说清楚的地方：

Timer如果已经启动了，再启动就不用管了。重置(恢复初始rto)之后的第一次启动，时间要从新的时间开始。至少目前我的实现如此。

接下来分函数来说明（说明的函数为startercode中以提供的、需要实现且需要说明的接口）：

```c++
uint64_t TCPSender::bytes_in_flight() const;
```

该函数的返回值指的是Sender已经发出，但是Receiver还没放进自己的ByteStream的字节个数。

```c++
void TCPSender::fill_window();
```

本函数是从`ByteStream`中读取字节并填入窗口。需要注意：

1. 没有发SYN，要先发SYN
2. 在FIN已经被发出去之后（不管有没有ack），都不能再发了。
3. 初始`ByteStream.input_ended()`为假，即可以继续往`ByteStream`里写数据。
4. 不能发送空segment，即`TCPSegment::length_in_sequence_space() == 0`的segment。注意`ByteStream.input_ended()`为假且`Bytestream.buffer_size() == 0`的时候。
5. 如果窗口大小为0，则按照窗口大小为1对待（发下一个字节）。另：最好不要修改窗口大小，因为在窗口大小为0时重发不需要exponential backoff。
6. `TCPConfig::MAX_PAYLOAD_SIZE`正如变量名，只能限制payload的大小，不能限制segment的大小（不能管制是否加FIN）。如：窗口大小为`TCPConfig::MAX_PAYLOAD_SIZE + 1`且无outstanding segment，已经`ByteStream::end_input()`了，且`ByteStream::buffer_size() == TCPConfig::MAX_PAYLOAD_SIZE`。则此次会发一个`TCPSegment::length_in_sequence_space() == TCPConfig::MAX_PAYLOAD_SIZE + 1`的segment。

```c++
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size);
```

本函数比较简单，只需注意：

1. 发送来的ackno是否无效（小于上次的ackno，大于`_next_seqno`）
2. 如果和上次的ackno一样，是不是带来了更大的window size
3. 如果都不是，即有新数据被接收了。那就正常更新。

```c++
void TCPSender::tick(const size_t ms_since_last_tick);
```

用来模拟时间流逝的方法。需要注意的是：

1. 当timer并没有在跑，且时间到了的时候，不能处理为timer触发了
2. 当触发时，如果窗口为0，则不是因为网络原因导致重发。所以不能增加重发次数或是让timer exponential backoff。

## 实验结果

```shell
$ make check_lab3
[100%] Testing the TCP sender...
Test project /home/vector/cs144/build
      Start  1: t_wrapping_ints_cmp
 1/33 Test  #1: t_wrapping_ints_cmp ..............   Passed    0.00 sec
      Start  2: t_wrapping_ints_unwrap
 2/33 Test  #2: t_wrapping_ints_unwrap ...........   Passed    0.01 sec
      Start  3: t_wrapping_ints_wrap
 3/33 Test  #3: t_wrapping_ints_wrap .............   Passed    0.00 sec
      Start  4: t_wrapping_ints_roundtrip
 4/33 Test  #4: t_wrapping_ints_roundtrip ........   Passed    0.21 sec
      Start  5: t_recv_connect
 5/33 Test  #5: t_recv_connect ...................   Passed    0.00 sec
      Start  6: t_recv_transmit
 6/33 Test  #6: t_recv_transmit ..................   Passed    0.06 sec
      Start  7: t_recv_window
 7/33 Test  #7: t_recv_window ....................   Passed    0.00 sec
      Start  8: t_recv_reorder
 8/33 Test  #8: t_recv_reorder ...................   Passed    0.00 sec
      Start  9: t_recv_close
 9/33 Test  #9: t_recv_close .....................   Passed    0.00 sec
      Start 10: t_recv_special
10/33 Test #10: t_recv_special ...................   Passed    0.00 sec
      Start 11: t_send_connect
11/33 Test #11: t_send_connect ...................   Passed    0.00 sec
      Start 12: t_send_transmit
12/33 Test #12: t_send_transmit ..................   Passed    0.05 sec
      Start 13: t_send_retx
13/33 Test #13: t_send_retx ......................   Passed    0.01 sec
      Start 14: t_send_window
14/33 Test #14: t_send_window ....................   Passed    0.04 sec
      Start 15: t_send_ack
15/33 Test #15: t_send_ack .......................   Passed    0.01 sec
      Start 16: t_send_close
16/33 Test #16: t_send_close .....................   Passed    0.00 sec
      Start 17: t_send_extra
17/33 Test #17: t_send_extra .....................   Passed    0.01 sec
      Start 18: t_strm_reassem_single
18/33 Test #18: t_strm_reassem_single ............   Passed    0.00 sec
      Start 19: t_strm_reassem_seq
19/33 Test #19: t_strm_reassem_seq ...............   Passed    0.01 sec
      Start 20: t_strm_reassem_dup
20/33 Test #20: t_strm_reassem_dup ...............   Passed    0.01 sec
      Start 21: t_strm_reassem_holes
21/33 Test #21: t_strm_reassem_holes .............   Passed    0.01 sec
      Start 22: t_strm_reassem_many
22/33 Test #22: t_strm_reassem_many ..............   Passed    0.11 sec
      Start 23: t_strm_reassem_overlapping
23/33 Test #23: t_strm_reassem_overlapping .......   Passed    0.01 sec
      Start 24: t_strm_reassem_win
24/33 Test #24: t_strm_reassem_win ...............   Passed    0.10 sec
      Start 25: t_strm_reassem_cap
25/33 Test #25: t_strm_reassem_cap ...............   Passed    0.10 sec
      Start 26: t_byte_stream_construction
26/33 Test #26: t_byte_stream_construction .......   Passed    0.00 sec
      Start 27: t_byte_stream_one_write
27/33 Test #27: t_byte_stream_one_write ..........   Passed    0.00 sec
      Start 28: t_byte_stream_two_writes
28/33 Test #28: t_byte_stream_two_writes .........   Passed    0.01 sec
      Start 29: t_byte_stream_capacity
29/33 Test #29: t_byte_stream_capacity ...........   Passed    0.56 sec
      Start 30: t_byte_stream_many_writes
30/33 Test #30: t_byte_stream_many_writes ........   Passed    0.01 sec
      Start 53: t_address_dt
31/33 Test #53: t_address_dt .....................   Passed    0.01 sec
      Start 54: t_parser_dt
32/33 Test #54: t_parser_dt ......................   Passed    0.00 sec
      Start 55: t_socket_dt
33/33 Test #55: t_socket_dt ......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 33

Total Test time (real) =   1.44 sec
[100%] Built target check_lab3
```
