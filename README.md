# HttpServer

A C++ High Performance WebServer


## Introduction  

本项目为应用C++11编写的基于Reactor事件分发模式以及epoll边沿触发IO复用方式的多线程HTTP网络服务器，在本项目中，目前主要完成的工作有：
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
    
## Technical points
 * 采用Reactor事件分发模式，采用epoll边缘触发(ET)IO复用方式。由于采用边缘触发方式，为以防漏读，套接字需要设置为非阻塞模式，并且在read的时候需要    读到出现error==EAGAIN为止。
 * 参考muduo库，在每个事件分发器EventLoop上维护有一个任务队列，用于在线程间传递任务。这可以保证访问特定资源的操作只能在特定线程完成，从而大大降低    多线程的编程难度。
 * 应用eventfd实现线程间通信，主要是用于唤醒线程及时处理任务队列中的事件。
 * 基于one loop per thread的模式，线程模型将划分为主线程、IO线程以及工作线程，其中主线程与IO线程均持有一个独立的事件分发器。各线程的任务如下：
   * 主线程负责接收客户端连接，分配连接所需的资源块，通过Round-Robin策略将连接分发给IO线程进行处理，并在连接关闭的时候控制资源块的释放。
   * IO线程负责连接管理，即负责读写以及关闭，在工作线程池没开启或者繁忙的情况下，还兼任HTTP报文的解析处理工作。
   * 工作线程负责业务计算任务（即对数据进行处理，应用层处理复杂的时候可以开启）
 * 基于时间轮实现定时器功能，定时查看TCP连接的状态。若TCP连接当前为空闲状态，则剔除该连接。否则，重新开始该TCP连接的计时。这种方式一方面实现惰性剔    除，一个空闲TCP连接释放前的空闲时间在[idleSeconds,2\*idleSeconds]之间。另外，在本项目中，采用timer_fd实现计时功能。
 * 采用智能指针管理所分配的资源。
 * 应用状态机解析HTTP报文，支持HTTP长连接，管线化请求。
 * 支持优雅关闭连接
   * 通常情况下，由客户端主动发起FIN关闭连接(若服务器没有开启剔除空闲连接)
   * 客户端发送FIN关闭连接后，服务器需要等待当前HTTP层报文全部处理完成以及TCP缓冲区中的数据发送完毕才close套接字，而不是直接暴力close。
   * 在上层HTTP解析出错后，服务器会在发送完差错报文后直接调用close关闭连接。
   * 如果连接出错或连接超时，则服务器可以直接调用forceClose操作关闭连接。


## Performance Test
 * 本项目采用了两款开源的HTTP压力测试工具“wrk”和“WebBench”进行测试，其中使用了林亚改写后的[WebBench](https://github.com/linyacool/WebBench)
 * 测试方法
   * 模拟1000条TCP连接，持续时间60s
   * 测试长连接情况
   * 考虑到磁盘IO的影响，分别对有/无磁盘IO影响两种情况做测试，测试时服务器的响应报文分别为:
     直接构建的HTTP响应报文（无磁盘IO影响）、HTTP报头+读取磁盘的index.html网页（有磁盘IO影响）
  

 * 测试环境（虚拟机环境下测试）
   * CPU: Intel(R) Core(TM) i5-4590 CPU @ 3.30GHz
   * 内存: 3G
   * 虚拟机：VMware-Workstation
   * 操作系统: Ubuntu 17.04
   * 内核: 4.10.0-42-generic
  
### 单线程测试 （执行命令：./httpServer 80 0 0 0）
* wrk测试结果：11.2万+QPS、6.6万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_0_0_0_hello.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_0_0_0_index.png)

对比上述数据可以看出，与无磁盘IO影响相比，有磁盘IO导致吞吐率下降50%。究其原因，在单线程时，由于磁盘IO会阻塞线程从而导致吞吐率下降。
* WebBench测试结果
  * 内存中的HTTP报文（无磁盘IO影响）
 ![WebBench](https://github.com/chentongjie94/webserver_chen/blob/master/data/webbench/webbench_0_0_0_hello.png)

  * index.html网页（有磁盘IO影响）
 ![WebBench](https://github.com/chentongjie94/webserver_chen/blob/master/data/webbench/webbench_0_0_0_hello.png)
 
### 多线程测试1 （4个IO线程 执行命令：./httpServer 80 4 0 0）
* wrk测试结果：16.2万+QPS、15.2万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_0_0_hello.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_0_0_index.png)
 
 将使用4个IO线程与上述单线程相比，无磁盘IO下吞吐率上升44%，有磁盘IO吞吐率上升230%。可以明显看出，相较于单线程，多线程下服务器性能有明显的提升。同时，多线程下服务器的性能受磁盘IO的影响远远小于单线程。
 ### 多线程测试2 （4个IO线程 2个工作线程 执行命令：./httpServer 80 4 2 0）
* wrk测试结果：12.8万+QPS、9.8万+QPS
  * 内存中的HTTP报文（无磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_2_0_hello.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_2_0_index.png)
 
 与前面仅使用4个IO线程相比，吞吐率有所下降。
 ### 多线程测试3(开启提出空闲连接) （4个IO线程 执行命令：./httpServer 80 4 0 15）
  ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_0_15_hello.png)

  * index.html网页（有磁盘IO影响）
 ![wrk](https://github.com/chentongjie94/webserver_chen/blob/master/data/wrk/wrk_4_0_15_index.png)

