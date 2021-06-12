# Lab 0: networking warmup

本实验为CS144的第一个实验，从实验名中可以看出实验属于热身性质。除了C++之外，不需要前置知识。实验的指导手册可在[此处](https://cs144.github.io/assignments/lab0.pdf)查看。

## 实验环境

系统：Ubuntu20.04 LTS

机器：阿里云ECS

实例规格：ecs.t5-lc1m2.small

## 实验内容

本实验分为三个部分：Networking by hand，Writing a network program using an OS stream socket 以及An in-memory reliable byte stream。

### Networking by hand

这个部分主要是用命令行手动发一些包，让我对Packet有了感性的认识。具体来说跟着指导手册走就好了（第二部分由于没有sunetid，所以作罢）。以第一部分为例，需要注意的是，一个包的结尾需要为一个空行，如：

```shell
$ telnet cs144.keithw.org http
Trying 104.196.238.229...
Connected to cs144.keithw.org.
Escape character is '^]'.
GET /hello HTTP/1.1
Host: cs144.keithw.org
Connection: close

HTTP/1.1 200 OK
Date: Sat, 12 Jun 2021 05:05:17 GMT
Server: Apache
Last-Modified: Thu, 13 Dec 2018 15:45:29 GMT
ETag: "e-57ce93446cb64"
Accept-Ranges: bytes
Content-Length: 14
Connection: close
Content-Type: text/plain

Hello, CS144!
Connection closed by foreign host.
```

上述代码中的第8行为上文所说的空行。如果没有这个空行，包就不完整，然后会收到`HTTP/1.1 408 Request Timeout`。

### Writing a network program using an OS stream socket

先从github上把代码拉下来，默认的master分支即为lab0所需分支。

```shell
$ git clone https://github.com/cs144/sponge ~/cs144
$ cd ~/cs144
```

如果需要查看所有分支，可以输入如下指令：

```shell
$ git branch --all
```

后续实验中`git checkout`到对应分支就可以。然后关注下指导手册中提到的避免成对的操作（`malloc/free`，`new/delete`），以及一些特别注意点。

之后是最后一个准备工作，读[FileDescriptor](https://cs144.github.io/doc/lab0/class_file_descriptor.html), [Socket](https://cs144.github.io/doc/lab0/class_socket.html), [TCPSocket](https://cs144.github.io/doc/lab0/class_t_c_p_socket.html)，以及[Address](https://cs144.github.io/doc/lab0/class_address.html) 类的文档。特别是[TCPSocket](https://cs144.github.io/doc/lab0/class_t_c_p_socket.html)中的Detailed Description部分。

做好了准备工作就可以开始做实验了，这个部分目的就是把上一个部分手动向`cs144.keithw.org`发送数据包的过程用代码实现。实现`../apps/webget.cc`中的`get_URL`函数即可。需要用到`TCPSocket`及`Address`类。跟着指导手册走就可以轻松完成，这部分就不细讲了。

### An in-memory reliable byte stream

这个部分是实现一个容器，容器在初始化时指定大小。这个容器可读可写，写的时候往里边塞，读的时候往外边拿。容器的大小虽然有限，但是可以传输的数据可以是任意有限大小的，只要在动态读写过程中的任意时刻，存放数据的数量不超过容量大小即可。

这里简单介绍下我的实现的high-level overview：

受到6.S081的影响，我的实现有点像硬件io的缓冲区，总体看下来为循环队列。类中维护着以下成员：

```c++
class ByteStream {
  private:
  	std::string buf;
  	size_t _cap;
  	size_t reader;
  	size_t writer;
  	bool	 _eof;
  	bool	 _error;
  // other codes
}
```

其中，`buf`为底层容器，固定大小，为`_cap`。`reader`和`writer`分别为已读/写的字符个数，需要读写时只需要对`_cap`取余即可获得当前待读写的字符下标，`_eof`指示着该Stream是否已经碰到`EOF`了。然后按着循环队列的思路就可以比较轻松地完成。

## 实验结果

所有的测试点全都通过了。

```shell
$ make check_lab0
[100%] Testing Lab 0...
Test project /home/vector/cs144/build
    Start 23: t_byte_stream_construction
1/9 Test #23: t_byte_stream_construction .......   Passed    0.00 sec
    Start 24: t_byte_stream_one_write
2/9 Test #24: t_byte_stream_one_write ..........   Passed    0.00 sec
    Start 25: t_byte_stream_two_writes
3/9 Test #25: t_byte_stream_two_writes .........   Passed    0.00 sec
    Start 26: t_byte_stream_capacity
4/9 Test #26: t_byte_stream_capacity ...........   Passed    0.54 sec
    Start 27: t_byte_stream_many_writes
5/9 Test #27: t_byte_stream_many_writes ........   Passed    0.01 sec
    Start 28: t_webget
6/9 Test #28: t_webget .........................   Passed    1.01 sec
    Start 50: t_address_dt
7/9 Test #50: t_address_dt .....................   Passed    0.01 sec
    Start 51: t_parser_dt
8/9 Test #51: t_parser_dt ......................   Passed    0.00 sec
    Start 52: t_socket_dt
9/9 Test #52: t_socket_dt ......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 9

Total Test time (real) =   1.63 sec
[100%] Built target check_lab0
```

