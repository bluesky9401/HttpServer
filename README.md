# NetServer

A C++ High Performance NetServer (version 0.4.1)

[![license](https://img.shields.io/github/license/mashape/apistatus.svg)](https://opensource.org/licenses/MIT)

## Introduction  

本项目为C++11编写的基于epoll的多线程网络服务器框架，应用层实现了简单的HTTP服务器HttpServer和一个回显服务器EchoServer，其中HTTP服务器实现了HTTP的解析和Get方法请求，目前支持静态资源访问，支持HTTP长连接；该框架不限于这两类服务器，用户可根据需要编写应用层服务。

## Origin and purpose of the project
* 项目起源：大四的时候实现了一个简单基于epoll的多线程服务器，支持HTTP的GET方法和JSON解析（见[forumNet](https://github.com/chenshuaihao/forumNet/tree/master/forumNet)），后来看了陈硕的书，决定重写一个网络服务器。在项目过程中参阅了网上很多优秀的博客和开源项目，也参考了陈硕和林亚的代码，在此向他们表示感谢！
* 项目目的：学习C++知识、部分C++11的语法和编码规范、学习巩固网络编程、网络IO模型、多线程、git使用、Linux命令、性能分析、TCP/IP、HTTP协议等知识

## Envoirment  
* OS: CentOS Linux release 7.0.1406 (# cat /etc/redhat-release)
* kernel: 3.10.0-123.el7.x86_64 (# uname -a)
* Complier: 4.8.5

## Build

	$ make
	$ make clean

## Run
	$ ./httpserver [port] [iothreadnum] [workerthreadnum]
	
	例：$ ./httpserver 80 4 2
	表示开启80端口，采用4个IO线程、2个工作线程的方式 
	一般情况下，业务处理简单的话，工作线程数设为0即可
    
## Tech
 * 基于epoll的IO复用机制实现Reactor模式，采用边缘触发（ET）模式，和非阻塞模式
 * 由于采用ET模式，read、write和accept的时候必须采用循环的方式，直到error==EAGAIN为止，防止漏读等清况，这样的效率会比LT模式高很多，减少了触发次数
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
