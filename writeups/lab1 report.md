# Lab1: stitching substrings into a byte stream

本实验为CS144的第二个实验。本实验除了C++的OOP之外，不需要前置知识。实验的指导手册可[在此](https://cs144.github.io/assignments/lab1.pdf)查看。

## 实验环境

系统：Ubuntu20.04 LTS

机器：阿里云ECS

实力规格：ecs.t5-lc1m2.small

## 实验内容

本实验主要是模拟TCP接受到数据包后，将数据包重组的过程。由于IP只会尽可能将数据包送达，而不会确保①数据包的顺序与发出顺序一致②数据包不会丢失③数据包不会重复，故需要接收端将数据包重组，恢复出原字符串。

确保收到的顺序与发出的顺序一致，有两种方法，详见[此处](https://afteracademy.com/blog/what-is-flow-control-in-networking)。本实验采用滑动窗口(Sliding Window Protocol)的方法。

实验注意事项：

1. 你可以把源数据当成一个大的字符串，其中每一个字符都有且仅有一个唯一的索引（index）
2. 仔细阅读[实验手册](https://cs144.github.io/assignments/lab1.pdf)3.1节，理解capacity以及源字符串的四个部分等相关概念。
3. 整个流（stream）的第一个字节的索引为0
4. 实现要有效率：每个测试点不要超过差不多半秒
5. 假设不会有不一致的字符（同一格位置不同的字符）出现。
6. 最好使用至少一个数据结构。
7. 越早写入字节越好，只要能写入，就写入。不能写入只有一种情况，就是该字符之前有字符还没有被“push”进来。
8. 输入给`push_substring()`方法的字符串会重叠。
9. 得给`StreamReassembler`加一些私有成员，用来记住子字符串，直到它们被写入。
10. `StreamReassembler`不能存储字符串的重叠部分。
11. 更多FAQ：如果想看更多，可以查看https://cs144.github.io/lab_faq.html

实现细节：我增加了如下成员变量

```c++
class StreamReassembler {
private:
  ...
  size_t stream_len;
  size_t _unassembled;
  std::map<size_t, std::string> unassembled;
  size_t first_unacceptable();
  size_t first_unassembled();
  bool validate_substr(std::string &data, size_t &index);
  void push_substring(std::string data, size_t index);
  void push_available(const std::string &data, const size_t index);
  void pop_substring();
public:
  ...
};
```

其中，`stream_len`维护的是`Stream`的总长度，初始化为`-1`（即最大），等有`eof`时，计算出总长度，到达总长度时不接受更多输入。`_unassembled`维护未组装的字符串的总长度。`unassembled`维护未组装的字符串的`index`及对应的字符串。

`first_unacceptable()`返回不可接受区域的第一个字符的索引。`first_unassembled()`返回未组装区域的第一个字符。`validate_substr()`返回输入字符串是否为有效字符串，即不是所有字符都被写入过，或是不全部在不可接受区。如果为有效字符串，但是字符串被部分写入过，或是部分在不可接受区，那么就修剪字符串，使之完全成为有效字符串。

`push_substring()`为`public`的`push_substring()`的重载，主要为修剪字符串，使之不与任何`unassembled`中的字符串重叠。若有完全覆盖，则在修剪完后会被分成两段，将前一段直接存入，后一段继续与之后的进行修剪。

`push_available()`与`pop_substring()`是一对真正操作`unassembled`的方法，`push_available()`将可用（不与任何已有、已读的字符串重叠）字符串存入`unassembled`中，并让`_unassembled`变量增加相应的值。而`pop_substring()`则会弹出第一个键值对，用以进行写入`_output`。并让`_unassembled`减少相应的值。

## 实验结果：

所有的测试点全部通过了。

```shell
$ make check_lab1
[100%] Testing the stream reassembler...
Test project /home/vector/cs144/build
      Start 15: t_strm_reassem_single
 1/16 Test #15: t_strm_reassem_single ............   Passed    0.00 sec
      Start 16: t_strm_reassem_seq
 2/16 Test #16: t_strm_reassem_seq ...............   Passed    0.00 sec
      Start 17: t_strm_reassem_dup
 3/16 Test #17: t_strm_reassem_dup ...............   Passed    0.01 sec
      Start 18: t_strm_reassem_holes
 4/16 Test #18: t_strm_reassem_holes .............   Passed    0.00 sec
      Start 19: t_strm_reassem_many
 5/16 Test #19: t_strm_reassem_many ..............   Passed    0.06 sec
      Start 20: t_strm_reassem_overlapping
 6/16 Test #20: t_strm_reassem_overlapping .......   Passed    0.00 sec
      Start 21: t_strm_reassem_win
 7/16 Test #21: t_strm_reassem_win ...............   Passed    0.06 sec
      Start 22: t_strm_reassem_cap
 8/16 Test #22: t_strm_reassem_cap ...............   Passed    0.06 sec
      Start 23: t_byte_stream_construction
 9/16 Test #23: t_byte_stream_construction .......   Passed    0.00 sec
      Start 24: t_byte_stream_one_write
10/16 Test #24: t_byte_stream_one_write ..........   Passed    0.00 sec
      Start 25: t_byte_stream_two_writes
11/16 Test #25: t_byte_stream_two_writes .........   Passed    0.00 sec
      Start 26: t_byte_stream_capacity
12/16 Test #26: t_byte_stream_capacity ...........   Passed    0.34 sec
      Start 27: t_byte_stream_many_writes
13/16 Test #27: t_byte_stream_many_writes ........   Passed    0.00 sec
      Start 50: t_address_dt
14/16 Test #50: t_address_dt .....................   Passed    0.01 sec
      Start 51: t_parser_dt
15/16 Test #51: t_parser_dt ......................   Passed    0.00 sec
      Start 52: t_socket_dt
16/16 Test #52: t_socket_dt ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 16

Total Test time (real) =   0.58 sec
[100%] Built target check_lab1
```


