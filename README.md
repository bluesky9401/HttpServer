# webserver_chen

A C++ High Performance HttpServer

[![license](https://img.shields.io/github/license/mashape/apistatus.svg)](https://opensource.org/licenses/MIT)

## Introduction  

本项目为C++11编写的基于epoll的多线程HTTP网络服务器，实现了HTTP的解析和Get方法请求，目前支持静态资源访问，支持HTTP长连接以及管线化请求；

* 项目目的：学习C++知识、部分C++11的语法和编码规范、学习巩固网络编程、网络IO模型、多线程、git使用、Linux命令、性能分析、TCP/IP、HTTP协议等知识

## Envoirment  
* OS: Ubuntu 17.04 (# cat /etc/issue)
* kernel: 4.10.0-42-generic.x86_64 (# uname -a)
* Complier: 6.3.0

## Build

	$ make
	$ make clean

## Run
	$ ./httpserver [port] [iothreadnum] [workerthreadnum]
	
	例：$ ./httpserver 80 4 2
	表示开启80端口，采用4个IO线程、2个工作线程的方式 
	一般情况下，业务处理简单的话，工作线程数设为0即可
    
## Tech
 * 基于epoll的IO复用机制实现Reactor模式，采用边缘触发（ET）模式，和IO非阻塞模式
 * 由于采用ET模式，read、write和accept的时候必须采用循环的方式，直到error==EAGAIN为止，防止漏读等清况.
 * 线程模型将划分为主线程、IO线程和worker线程，主线程主要负责监听连接到来事件、分配连接所需要的资源以及在连接关闭时进行一些处理工作，并通过Round-Robin策略分发给IO线程；IO线程负责监听连接的读写事件，并且进行数据的接收以及发送工作；工作线程负责对接收到的Http报文进行处理以及构建响应报文，为了支持Http管线化请求，IO线程只有在报文收取完整的情况下才把报文提交到工作线程中处理。
 * 采用智能指针管理多线程下的对象资源
 * 支持HTTP长连接
 * 支持优雅关闭连接
   * 通常情况下，由客户端主动发起FIN关闭连接
   * 客户端发送FIN关闭连接后，服务器把数据发完才close，而不是直接暴力close
   * 如果连接出错，则服务器可以直接close


## Performance Test
 * 本项目采用了两款开源的HTTP压力测试工具“wrk”和“WebBench”进行测试，其中使用了林亚改写后的[WebBench](https://github.com/linyacool/WebBench)
 * 测试方法
   * 模拟1000条TCP连接，持续时间60s
   * 测试长连接情况
   * 考虑到磁盘IO的影响，分别对有/无磁盘IO影响两种情况做测试，测试时服务器的响应报文分别为:
     保存到内存中的HTTP报文（无磁盘IO影响）、HTTP报头+读取磁盘的index.html网页（有磁盘IO影响）
  

 * 测试环境（虚拟机环境下测试，性能可能较物理机低）
   * CPU: Intel(R) Core(TM) i5-4440 CPU @ 3.10GHz
   * Memory: 2G
   * VirtualBox 5.2.20
   * OS: CentOS Linux release 7.0.1406
   * kernel: 3.10.0-123.el7.x86_64
  
### 单线程测试 （执行命令：./httpserver 80 0 0）
* wrk测试结果：9万+QPS、4万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_hello.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_html.png)

* WebBench测试结果
  * 内存中的HTTP报文（无磁盘IO影响）
 ![WebBench](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/WebBench_hello.png)

  * index.html网页（有磁盘IO影响）
 ![WebBench](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/WebBench_html.png)
 
### 多线程测试1 （4个IO线程 执行命令：./httpserver 80 4 0）
* wrk测试结果：21万+QPS、5万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_hello_4_iothread.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_html_4_iothread.png)
 
 ### 多线程测试2 （4个IO线程 2个工作线程 执行命令：./httpserver 80 4 2）
* wrk测试结果：19万+QPS、11万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_hello_4_iothread_2_workerthread.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chenshuaihao/NetServer/blob/master/docs/images/wrk_html_4_iothread_2_workerthread.png)
 
## License
See [LICENSE](https://github.com/chenshuaihao/NetServer/blob/master/LICENSE)

## Roadmap
日志系统、内存池等

## Develop and Fix List
* 2019-02-21 Dev: 实现IO线程池，性能比单线程提升30+%，由EventLoopThreadPool类对IO线程进行管理，主线程accept客户端连接，并通过Round-Robin策略分发给IO线程，IO线程负责事件监听、读写操作和业务计算
* 2019-02-21 Fix: 修复多线程下HttpServer::HandleMessage函数中phttpsession可能为NULL，导致出现SegmentFault的情况。因为新连接事件过早的添加到epoll中，而HttpSession还没new，如果这时候有数据来时，会出现phttpsession==NULL，无法处理数据，段错误。
* 2019-02-24 Dev: 实现worker线程池，响应index.html网页的性能比单线程提升100+%；实现了跨线程唤醒
* 2019-03-17 Dev: 基于时间轮实现定时器功能，定时剔除不活跃连接，时间轮的插入、删除复杂度为O(1)，执行复杂度取决于每个桶上的链表长度
* 2019-03-27 Dev&Fix: 把部分关键的原始指针改成智能指针，解决多线程下资源访问引起的内存问题；修复一些内存问题（段错误、double free等）；添加注释
* 2019-03-28 Fix: 头文件包含次序调整；部分函数参数加const修饰和改为引用传递，提高程序效率

## Others
本项目将一直进行开发和维护，也非常欢迎各位小伙伴提出建议，共同学习，共同进步！

Enjoy!
