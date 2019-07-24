/* author: chentongjie
 * last updated: 7-23, 2019
 *
 * */
#include <iostream>
#include <unistd.h>
#include <fcntl.h> // set nonblocking
#include <cstdlib>
#include <netinet/in.h>
#include "TcpServer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "TcpConnection.h"
using std::cin;
using std::cout;
using std::endl;

void setNonblocking(int fd);

TcpServer::TcpServer(EventLoop *loop, int port, int threadNum, int idleSeconds)
    : serverSocket_(),
      loop_(loop),
      eventLoopThreadPool_(loop, threadNum),
      connectionBuckets_(),
      spServerChannel_(new Channel()),
      connCount_(0)
{
    serverSocket_.setReuseAddr();// 设置地址复用  
    serverSocket_.bindAddress(port);// 绑定端口
    serverSocket_.listen();// 将套接字转为监听套接字
    serverSocket_.setNonblocking();// 设置非阻塞模式

    // 设置监听事件注册器上的回调函数以及监听套接字的描述符
    spServerChannel_->setFd(serverSocket_.fd());// 注册监听事件至serverChannel
    spServerChannel_->setReadHandle(std::bind(&TcpServer::onNewConnection, this));
    spServerChannel_->setErrorHandle(std::bind(&TcpServer::onConnectionError, this)); 
    if (idleSeconds > 0) 
        connectionBuckets_.resize(idleSeconds);
}

TcpServer::~TcpServer()
{
    cout << "Call TcpServer destructor!" << endl;
}

void TcpServer::start()
{
    eventLoopThreadPool_.start();// 启动eventLoop线程池(用作IO线程)
    spServerChannel_->setEvents(EPOLLIN | EPOLLET);// 设置监听套接字可读边沿触发
    loop_->addChannelToEpoller(spServerChannel_);// 将该感兴趣的事件添加到epoller_中

    if (!connectionBuckets_.empty())
        loop_->setOnTimeCallback(1, std::bind(&TcpServer::onTime, this));
}

/* 新TCP连接处理，核心功能，业务功能注册，任务分发
 * 注册该事件到主线程所持有的EventLoop对象
 */
void TcpServer::onNewConnection()
{
    // 循环调用accept，获取所有的建立好连接的客户端fd
    struct sockaddr_in clientaddr;
    int clientfd;
    while( (clientfd = serverSocket_.accept(clientaddr)) > 0) 
    {
        //std::cout << "New client from IP:" << inet_ntoa(clientaddr.sin_addr) 
        //    << ":" << ntohs(clientaddr.sin_port) << std::endl;
        
        if(connCount_+1 > MAXCONNECTION)// 若已创建连接大于所允许的最大连接数
        {
            close(clientfd);
            continue;
        }
        /* 上层连接建立步骤：
         * 1. 设置客户连接套接字为非阻塞
         * 2. 从IO线程池中选择合适的IO线程作为该连接的读写处理线程
         * 3. 分配TcpConnection资源块，并将该资源块与clientfd以及loop绑定
         * 4. 创建Entry条目，用于控制空闲连接的释放
         * 5. 在TcpConnection上设置更新时间轮的回调函数
         * 6. 调用上层新建连接处理函数(分配HttpSession资源块并绑定)
         * 7. 向TcpConnection登记TcpServer的连接清理回调函数
         * 8. 最终，将该连接登记到IO线程的Epoll上
         */
        setNonblocking(clientfd);
        EventLoop *loop = eventLoopThreadPool_.getNextLoop();
        SP_TcpConnection spTcpConn = std::make_shared<TcpConnection>(loop, clientfd, clientaddr);// TcpConnection用于存放连接信息
        if (!connectionBuckets_.empty())
        {
            SP_Entry spEntry(new Entry(spTcpConn));
            connectionBuckets_.back().insert(spEntry);
            WP_Entry wpEntry(spEntry);
            spTcpConn->setIsActiveCallback(std::bind(&TcpServer::isActive, this, spTcpConn, wpEntry));
        }
    
        tcpList_[clientfd] = spTcpConn;
        newConnectionCallback_(spTcpConn, loop);// HTTP层分配HttpSession块，并且将其与TcpConnection绑定
        // 登记连接清理操作，在连接即将关闭或发生错误时，由IO线程推送连接清理任务至主线程
        spTcpConn->setConnectionCleanUp(std::bind(&TcpServer::connectionCleanUp, this, clientfd));
        // Bug，应该把事件添加的操作放到最后,否则bug segement fault(在还没登记HTTP层处理回调函数前即读入数据)
        // 总之就是做好一切准备工作再添加事件到epoll！！！
        // 由TcpConnection向IO线程注册事件
        spTcpConn->addChannelToLoop();// 最后是将TcpConnection中的回调函数注册到epoller_
        ++ connCount_;
        cout << "Connections' number = " << connCount_ << endl;
    }
}

void TcpServer::connectionCleanUp(int fd) 
{
    if (loop_->isInLoopThread()) 
    {
        --connCount_;
        tcpList_.erase(fd);
    }
    else
    {  
        loop_->addTask(std::bind(&TcpServer::connectionCleanUp, this, fd));
    }
}

// 每秒计时会更新时间轮
void TcpServer::onTime()
{
    loop_->assertInLoopThread();
    connectionBuckets_.push_back(Bucket());
}
/* 服务器端的等待连接套接字发生错误，会关闭套接字
 * */
void TcpServer::onConnectionError()
{    
    std::cout << "UNKNOWN EVENT" << std::endl;
    serverSocket_.close();
}
// 若Tcp连接处于活跃状态，在计时到达的时候调用重新更新TCP连接信息到时间轮
void TcpServer::isActive(WP_TcpConnection wpTcpConn, WP_Entry wpEntry) 
{
    if (loop_->isInLoopThread()) 
    {
        SP_TcpConnection spTcpConn = wpTcpConn.lock();
        if (spTcpConn)
        {
            SP_Entry spEntry(new Entry(spTcpConn));
            connectionBuckets_.back().insert(spEntry);
        }
    } 
    else 
    {
        loop_->addTask(std::bind(&TcpServer::isActive, this, wpTcpConn, wpEntry));
    }
}
/* 设置套接字为非阻塞 */
void setNonblocking(int fd)
{
    int opts = fcntl(fd, F_GETFL);// 获取当前文件描述符的信息
    if (opts < 0)
    {
        perror("fcntl(fd,GETFL)");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, opts | O_NONBLOCK) < 0)// 设置当前文件描述符为非阻塞 
    {
        perror("fcntl(fd,SETFL,opts)");
        exit(1);
    }
}
