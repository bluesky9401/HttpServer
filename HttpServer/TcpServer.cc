/* author: chentongjie
 * date:
 *
 * */
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h> // set nonblocking
#include <cstdlib>
#include <netinet/in.h>
#include <arpa/inet.h>// net_ntoa() 
#include "TcpServer.h"

void setNonblocking(int fd);

TcpServer::TcpServer(EventLoop *loop, int port, int threadNum)
    : serverSocket_(),
    loop_(loop),
    eventLoopThreadPool_(loop, threadNum),
    spServerChannel_(new Channel()),
    connCount_(0)
{
    //serversocket_.SetSocketOption(); 
    serverSocket_.setReuseAddr(); //设置套接字参数  
    serverSocket_.bindAddress(port);
    serverSocket_.listen();
    serverSocket_.setNonblocking();

    spServerChannel_->setFd(serverSocket_.fd());// 注册监听事件至serverChannel
    spServerChannel_->setReadHandle(std::bind(&TcpServer::onNewConnection, this));
    spServerChannel_->setErrorHandle(std::bind(&TcpServer::onConnectionError, this));    
}

TcpServer::~TcpServer()
{

}

void TcpServer::start()
{
    eventLoopThreadPool_.start();// 启动eventLoop线程池(用作IO线程)

    spServerChannel_->setEvents(EPOLLIN | EPOLLET);// 注册serverChannel感兴趣的事件
    loop_->addChannelToEpoller(spServerChannel_);// 将该感兴趣的事件添加到epoller_中
}

// 新TCP连接处理，核心功能，业务功能注册，任务分发
// 注册该事件到主线程所持有的EventLoop对象
void TcpServer::onNewConnection()
{
    //循环调用accept，获取所有的建立好连接的客户端fd
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
        setNonblocking(clientfd);// 将客户连接套接字置为非阻塞

        //选择IO线程loop
        EventLoop *loop = eventLoopThreadPool_.getNextLoop();// 从eventloopthreadpool池中捞出一个线程

        // 创建连接，注册业务函数(此处在主线程创建TcpConnection对象，然后将使用权交给loop所在的IO线程)
        SP_TcpConnection spTcpConnection = std::make_shared<TcpConnection>(loop, clientfd, clientaddr);// TcpConnection用于存放连接信息
        tcpList_[clientfd] = spTcpConnection;

        newConnectionCallback_(spTcpConnection);// 上层HTTP做新建连接准备工作

        // 登记连接清理操作，在连接即将关闭或发生错误时，由IO线程推送连接清理任务至主线程
        spTcpConnection->setConnectionCleanUp(std::bind(&TcpServer::removeConnection, this, clientfd));

        // Bug，应该把事件添加的操作放到最后,否则bug segement fault(在还没登记HTTP层处理回调函数前即读入数据)
        // 总之就是做好一切准备工作再添加事件到epoll！！！
        // 由TcpConnection向IO线程注册事件
        spTcpConnection->addChannelToLoop();// 最后是将TcpConnection中的回调函数注册到epoller_
        ++ connCount_;
    }
}

/* 连接清理(注意: 连接清理由主线程进行)
 * 由IO线程将连接清理事件推送进主线程的任务队列 */
void TcpServer::removeConnection(int fd)
{
    --connCount_;
    tcpList_.erase(fd);
    std::cout << "clean up connection, connount is" << connCount_ << std::endl;   
}

/* 服务器端的等待连接套接字发生错误，会关闭套接字
 * */
void TcpServer::onConnectionError()
{    
    std::cout << "UNKNOWN EVENT" << std::endl;
    serverSocket_.close();
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
