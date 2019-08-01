# HttpServer

A C++ High Performance WebServer


## Introduction  

本项目为应用C++11编写的基于Reactor事件分发模式以及Epoll边沿触发IO复用方式的多线程HTTP网络服务器，在本项目中，目前主要完成的工作有：
* 实现了一个基于Reactor模式的事件分发器。
* 实现了一个基本的工作线程池，用于解析处理报文。
* 应用状态机实现HTTP的解析，目前支持GET方法、HEAD方法的请求，支持对静态资源的访问，支持HTTP长连接以及管线化请求。
* 应用智能指针对所分配的资源块进行管理。
* 实现了时间轮，用于剔除服务器上的空闲连接。

## The purpose of this project
项目的目的主要有以下几个方面：
* 增加C++的编程经验
* 学习多线程下的网络编程，巩固TCP/IP、HTTP协议等知识。

## Envoirment  
* 操作系统: Ubuntu 17.04
* 内核: 4.10.0-42-generic
* 编辑工具：Vim
* 编译工具: g++ 6.3.0
* 排查工具：gdb

## Build

	$ make
	$ make clean

## Run
	$ ./httpServer [port] [iothreadnum] [workerthreadnum] [idleSeconds]
	
	例：$ ./httpServer 80 4 2 60
	表示开启80端口，采用4个IO线程、2个工作线程、空闲连接超时时间为60s的方式 
	一般情况下，业务处理简单的话，工作线程数设为0即可
    
## Tech
 * 基于epoll的IO复用机制实现Reactor模式，采用边缘触发（ET）模式，和非阻塞模式。
 * 由于采用ET模式，read、write和accept的时候必须采用循环的方式，直到error==EAGAIN为止，防止漏读等清况。
 * Version-0.1.0基于单线程实现，Version-0.2.0利用线程池实现多IO线程，Version-0.3.0实现通用worker线程池，基于one loop per thread的IO模式
 * 线程模型将划分为主线程、IO线程和worker线程，主线程接收客户端连接（accept），并通过Round-Robin策略分发给IO线程，IO线程负责连接管理（即事件监听和读写操作），worker线程负责业务计算任务（即对数据进行处理，应用层处理复杂的时候可以开启）
 * 基于时间轮实现定时器功能，定时剔除不活跃连接，时间轮的插入、删除复杂度为O(1)，执行复杂度取决于每个桶上的链表长度
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

