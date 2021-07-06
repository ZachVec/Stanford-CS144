# Lab5: the network interface

本实验为CS144的第六个实验，也是倒数第二个实验。实验前需要看完[实验指导手册](https://cs144.github.io/assignments/lab5.pdf)及其中推荐阅读的相关文档。实验难度相比上一个实验，难度果然是低了很多，怪不得上一个实验手册的小标题是「the summit」，而且overview的第一行单独一行且加粗说：

> **You have reached the summit.**

本实验主要是在TCP/IP之下，深入到了链路层。在链路层这个水平上，将`IPv4 datagram`包装成`EthernetFrame`，然后发出。除此之外，还有接收`EthernetFrame`，根据其种类，方法会有不同行为。还有最后一个`tick`，用来表示时间的流逝，进而维护内部数据结构。

之前的实验中，我们实现了 *TCP* 协议。但是 *TCP segments* 是如何被运输到其它计算机的 *TCP* 中的呢？总共有如下几种可能：

- TCP-in-UDP-in-IP：*TCP Segment* 可以放在 *user datagram* 中。
  当在正常情况下（用户态）时，这是最容易实现的方式——Linux提供了一个接口（`UDPSocket`），可以让应用只提供 *user datagram* 的 *payload* 和 *target address*。剩下的就由内核处理——构造*UDP*的*header*、*IP*的*header*及*Ethernet*的*header*，然后把这个包发送到正确的下一跳。内核会确保这样的 *socket* 有唯一的本地-远端地址和端口号的组合。并且由于是内核来写 *UDP*、*IP* 的头部，所以它可以保证不同应用之间的隔离。
- TCP-in-IP：一般情况下，*TCP segment* 几乎总是以这种方式发送——直接放到 *IP datagram* 里。
  所谓「*TCP/IP*」指代的就是这种东西。这种方式实现起来会麻烦一点。Linux提供了一个叫做`TUN`设备的接口，让应用提供整个*IP datagram*，剩下的交给内核来处理（写 *Ethernet header* 然后从物理网卡真正地发出去）。但是现在应用要自己构造完整地 *IP header*，而不仅仅是 *payload*。
- TCP-in-IP-in-Ethernet：在上面的方式中，我们还是要依靠Linux内核。
  每当代码写了一个 *IP datagram* 到`TUN`设备的时候，Linux就得构造相应的链路层数据帧，再把 *IP datagram* 作为 *payload*。这意味着Linux还是得根据给出的下一跳IP，搞清楚下一跳的Ethernet地址。如果它还没有这个映射，Linux就会进行广播，问“谁是这个地址？你的以太网地址是什么？”然后等待应答。
  这些功能都由`network interface`提供，它可以把发出的 *IP datagram* 转换成链路层（如以太网）数据帧，反之亦然。在一个真实的系统中，`network interface`常常有着诸如`eth0`、`eth1`、`wlan0`等等的名字。在本周的实验中，你要实现`network interface`，然后把它放在你的TCP/IP 栈的底下。你的代码会产出未处理的以太网数据帧。然后Linux会通过一个类似于`TUN`、但更底层的、叫`TAP`设备的接口处理这些数据帧，因为是Linux来交换链路层数据帧而不是 *IP datagram*.

本实验大部分任务是给每一个下一跳的IP找 *Ethernet address*。用来干这事的协议叫做 **Address Resolution Protocol**，或 **ARP**。

## 实验环境

系统：Ubuntu20.04 LTS

机器：阿里云ECS

实例规格：ecs.t5-lc1m2.small

## Part I. The Address Resolution Protocol

这部分比较简单，只要跟着实验报告走就好了。不过注意一点：在发出ARP时（`ARPMessage::OPCODE_REQUEST`），`EthernetHeader::dst`得是`ETHERNET_BROADCAST`，这一点报告里没有提。很轻松就可以全过：

```shell
$ make -j4
$ ctest -V -R "^arp"
UpdateCTestConfiguration  from :/home/vector/cs144/build/DartConfiguration.tcl
UpdateCTestConfiguration  from :/home/vector/cs144/build/DartConfiguration.tcl
Test project /home/vector/cs144/build
Constructing a list of tests
Done constructing a list of tests
Updating test list for fixtures
Added 0 tests to meet fixture requirements
Checking test dependency graph...
Checking test dependency graph end
test 32
    Start 32: arp_network_interface

32: Test command: /home/vector/cs144/build/tests/net_interface
32: Test timeout computed to be: 10000000
32: DEBUG: Network interface has Ethernet address 4a:a1:a4:5b:95:d8 and IP address 4.3.2.1
32: DEBUG: Network interface has Ethernet address 56:81:a5:6c:04:f1 and IP address 5.5.5.5
32: DEBUG: Network interface has Ethernet address 12:ff:ea:22:d7:75 and IP address 5.5.5.5
32: DEBUG: Network interface has Ethernet address c6:b4:d8:7e:56:a2 and IP address 1.2.3.4
32: DEBUG: Network interface has Ethernet address c2:f9:0b:cf:d9:55 and IP address 4.3.2.1
32: DEBUG: Network interface has Ethernet address 5e:e0:d3:4f:24:d0 and IP address 10.0.0.1
1/1 Test #32: arp_network_interface ............   Passed    0.01 sec

The following tests passed:
        arp_network_interface

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.02 sec
$ make check_lab5
[100%] Testing Lab 5...
Test project /home/vector/cs144/build
    Start 31: t_webget
1/2 Test #31: t_webget .........................   Passed    1.17 sec
    Start 32: arp_network_interface
2/2 Test #32: arp_network_interface ............   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   1.19 sec
[100%] Built target check_lab5
```

## Part II. webget revisited

在`./apps/webget.cc`中把`CS144TCPSocket`类型替换为`FullStackSocket`，编译，重跑`make check_lab5`：

```shell
$ make -j4
$ make check_lab5
[100%] Testing Lab 5...
Test project /home/vector/cs144/build
    Start 31: t_webget
1/2 Test #31: t_webget .........................   Passed    1.22 sec
    Start 32: arp_network_interface
2/2 Test #32: arp_network_interface ............   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   1.23 sec
[100%] Built target check_lab5
```

没啥问题，本次实验算是圆满完成。由于 Lab7 好像是没有代码的，所以实际上实验只剩下最后一个，即 Lab6。
