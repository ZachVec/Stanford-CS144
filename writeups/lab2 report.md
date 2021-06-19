# Lab 2: the TCP receiver

本实验为CS144的第三个实验。本实验除了C++外，还需要了解一些TCP的概念。实验的[指导手册](https://cs144.github.io/assignments/lab2.pdf)中会提到相关概念。

## 实验环境

系统：Ubuntu20.04 LTS

机器：阿里云ECS

实例规格：ecs.t5-lc1m2.small

## 相关概念

### TCP Segment

[TCP数据报](https://cs144.github.io/doc/lab2/class_t_c_p_segment.html)的结构如下：

```markdown
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |       Destination Port        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Data |           |U|A|P|R|S|F|                               |
| Offset| Reserved  |R|C|S|S|Y|I|            Window             |
|       |           |G|K|H|T|N|N|                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Checksum            |         Urgent Pointer        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options                    |    Padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            payload                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### TCP Header

[TCP的报头](https://cs144.github.io/doc/lab2/struct_t_c_p_header.html)指的是上述结构中从`Source Port`至`Urgent Pointer`（至少该实验如此）。其中`SYN`的意思是该数据报中数据的第一个字符（若有）为整个数据流中的第一个字符。而`FIN`的意思是该数据报中数据的最后一个字符（若有）为整个数据流中的最后一个字符。

### TCP Payload

[TCP的数据](https://cs144.github.io/doc/lab2/class_buffer.html)为`Buffer`类，目前不做深究。只需了解需要用数据时，调用`Buffer::copy()`方法即可返回字符串形式的数据。

### 序号

序号总共有三种：`seqno`, `absolute seqno`, `stream index`。以一个数据为“cat”的数据报为例：

|          element |   `SYN`   |     c     |  a   |  t   | `FIN` |
| ---------------: | :-------: | :-------: | :--: | :--: | :---: |
|          `seqno` | 2<sup>32</sup> - 2 | 2<sup>32</sup> - 1 |  0   |  1   |   2   |
| `absolute seqno` |     0     |     1     |  2   |  3   |   4   |
|          `index` |     -     |     0     |  1   |  2   |   -   |

`seqno`：`WrappingInt32`类型，位于TCP报头，用于指明该**数据报的相对位置**。可以通过一些手段恢复成绝对位置，即64位。

`absolute seqno`：`uint64_t`类型，为`seqno`通过一些手段回复出来的结果，用于指示该**数据报的绝对位置**。

`index`：`uint64_t`类型，`StreamReassembler::push_substring`中的`index`参数，与`absolute seqno`类似，用于指示数据报中**数据的绝对位置**。

> 需要注意的是，`seqno`及`absolute seqno`包含`SYN`及`FIN`，也就是说`seqno`及`absolute seqno`可以指向`SYN`和`FIN`，而`Stream Index`不行。三种序号之间的转换关系如下：
>
> - `seqno -> absolute seqno`：需要调用`unwrap(WrappingInt32, WrappingInt32, uint64_t)`（需要自己实现）。在`SYN`包出现之前，无法转换。
> - `absolute seqno -> seqno`：需要调用`wrap(uint64_t, wrappingInt)`（需要自己实现）。在`SYN`包出现之前，无法转换。
> - `absolute seqno -> index`：简单的减1即可。注意，这里的`absolute seqno`必须指向数据，不能指向`SYN`或`FIN`，否则会溢出（`0-1==UINT64_MAX`）。
> - `index -> absolute seqno`：简单的加1即可。

## 注意事项

本次实验中可能会发生编译错误，错误如下：

```shell
$ make -j4
...
CMake Error: The following variables are used in this project, but they are set to NOTFOUND.
Please set them or make sure they are set and tested correctly in the CMake files:
LIBPCAP
    linked by target "tcp_parser" in directory /home/vector/cs144/tests
    linked by target "tcp_parser" in directory /home/vector/cs144/tests

-- Configuring incomplete, errors occurred!
...
```

可以看到，在`Makefile`中的变量`LIBPCAP`找不着了。该错误是由`libpcap`包缺失导致的，输入如下指令安装`libpcap`包。

```shell
$ sudo apt install libpcap-dev
```

## 实验内容

### Translating between 64-bit indexes and 32-bit seqnos

注意，标题所指`index`指的不是`stream index`而是`absolute seqno`。本实验需要实现上文所述的`wrap`及`unwrap`。总体来说，跟着指导手册来做实验还是比较简单的，需要注意的问题集中在`uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint)`中：

1. 手册中说明了要找一个离`checkpoint`“最近”的结果。所谓“最近”，指的是在`checkpoint`的±2<sup>31</sup>的范围内。更具体地，在我的实现中，指的是[`checkpoint`-2<sup>31</sup>, `checkpoint`+2<sup>31</sup>)的范围。
2. 如果checkpoint小于2<sup>31</sup>，则上述范围的左边界会小于0，显然不可能。此时范围应该是[0, `checkpoint` + 2<sup>31</sup>)。但这样范围就不完整了，会有一些数据落在外边，如`unwrap(WrappingInt32(15), WrappingInt32(16), 0)`。显然结果应该为`static_cast<uint64_t>(UINT32_MAX)`，而不能为`-1`。需要处理这种情况。

### Implement the TCP receiver

这个部分手册中说的比较详细，可以跟着手册走。但是需要注意一点：`index`不能指向`SYN`及`FIN`，所以在`ackno()`方法中需要特别注意。`stream_out().byte_written()`的返回值就是`index`，无法指向`FIN`。而`ackno()`的返回值为`WrappingInt32`（可以先将`optional<>`忽略，其介绍见[此处](https://en.cppreference.com/w/cpp/utility/optional)），可以指向`FIN`，其含义为：

> the sequence number of the first byte that the receiver doesn’t already know.

所以会引入一个问题：

- 如果还没有`stream.input_ended()`，即流还没有结束。则`stream_out().byte_written()`对应的`WrappingInt32`位置的`byte`receiver不知道，返回该值。
- 如果`stream.input_ended()`，则`stream_out().byte_written()`对应的`WrappingInt32`位置应该为`FIN`。即该位置的`byte`receiver已知，应该返回下一个值。

## 实验结果

第一部分的结果：

```shell
$ ctest -R wrap
Test project /home/vector/cs144/build
    Start 1: t_wrapping_ints_cmp
1/4 Test #1: t_wrapping_ints_cmp ..............   Passed    0.00 sec
    Start 2: t_wrapping_ints_unwrap
2/4 Test #2: t_wrapping_ints_unwrap ...........   Passed    0.00 sec
    Start 3: t_wrapping_ints_wrap
3/4 Test #3: t_wrapping_ints_wrap .............   Passed    0.00 sec
    Start 4: t_wrapping_ints_roundtrip
4/4 Test #4: t_wrapping_ints_roundtrip ........   Passed    0.23 sec

100% tests passed, 0 tests failed out of 4

Total Test time (real) =   0.25 sec
```

第二部分的结果：

```shell
$ make check_lab2
[100%] Testing the TCP receiver...
Test project /home/vector/cs144/build
      Start  1: t_wrapping_ints_cmp
 1/26 Test  #1: t_wrapping_ints_cmp ..............   Passed    0.00 sec
      Start  2: t_wrapping_ints_unwrap
 2/26 Test  #2: t_wrapping_ints_unwrap ...........   Passed    0.00 sec
      Start  3: t_wrapping_ints_wrap
 3/26 Test  #3: t_wrapping_ints_wrap .............   Passed    0.00 sec
      Start  4: t_wrapping_ints_roundtrip
 4/26 Test  #4: t_wrapping_ints_roundtrip ........   Passed    0.21 sec
      Start  5: t_recv_connect
 5/26 Test  #5: t_recv_connect ...................   Passed    0.00 sec
      Start  6: t_recv_transmit
 6/26 Test  #6: t_recv_transmit ..................   Passed    0.05 sec
      Start  7: t_recv_window
 7/26 Test  #7: t_recv_window ....................   Passed    0.00 sec
      Start  8: t_recv_reorder
 8/26 Test  #8: t_recv_reorder ...................   Passed    0.00 sec
      Start  9: t_recv_close
 9/26 Test  #9: t_recv_close .....................   Passed    0.00 sec
      Start 10: t_recv_special
10/26 Test #10: t_recv_special ...................   Passed    0.00 sec
      Start 17: t_strm_reassem_single
11/26 Test #17: t_strm_reassem_single ............   Passed    0.00 sec
      Start 18: t_strm_reassem_seq
12/26 Test #18: t_strm_reassem_seq ...............   Passed    0.00 sec
      Start 19: t_strm_reassem_dup
13/26 Test #19: t_strm_reassem_dup ...............   Passed    0.01 sec
      Start 20: t_strm_reassem_holes
14/26 Test #20: t_strm_reassem_holes .............   Passed    0.00 sec
      Start 21: t_strm_reassem_many
15/26 Test #21: t_strm_reassem_many ..............   Passed    0.11 sec
      Start 22: t_strm_reassem_overlapping
16/26 Test #22: t_strm_reassem_overlapping .......   Passed    0.01 sec
      Start 23: t_strm_reassem_win
17/26 Test #23: t_strm_reassem_win ...............   Passed    0.09 sec
      Start 24: t_strm_reassem_cap
18/26 Test #24: t_strm_reassem_cap ...............   Passed    0.11 sec
      Start 25: t_byte_stream_construction
19/26 Test #25: t_byte_stream_construction .......   Passed    0.00 sec
      Start 26: t_byte_stream_one_write
20/26 Test #26: t_byte_stream_one_write ..........   Passed    0.00 sec
      Start 27: t_byte_stream_two_writes
21/26 Test #27: t_byte_stream_two_writes .........   Passed    0.00 sec
      Start 28: t_byte_stream_capacity
22/26 Test #28: t_byte_stream_capacity ...........   Passed    0.66 sec
      Start 29: t_byte_stream_many_writes
23/26 Test #29: t_byte_stream_many_writes ........   Passed    0.01 sec
      Start 52: t_address_dt
24/26 Test #52: t_address_dt .....................   Passed    0.01 sec
      Start 53: t_parser_dt
25/26 Test #53: t_parser_dt ......................   Passed    0.00 sec
      Start 54: t_socket_dt
26/26 Test #54: t_socket_dt ......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 26

Total Test time (real) =   1.39 sec
[100%] Built target check_lab2
```


