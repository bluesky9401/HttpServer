/* author: chentongjie
 * last updated: 7-24, 2019
 * TcpServer持有一个EventLoopThreadPool线程池、accept套接字、EventLoop对象、serverChannel事件注册器
 * 作用：负责向所持有的EventLoop事件分发器登记等待连接事件，同时在连接到来时新建一个TcpConnection对象，
 *       并从EventLoopThreadPool线程池中捞出一个IO线程与该TcpConnection对象绑定, 然后往IO线程中推送连
 *       接事件注册任务，将该连接登记到IO线程中, 该连接的读写任务均由该IO线程负责，
 *       // 主线程只负责在连接关闭或连接发生错误时释放连接所占有的资源。(目前由IO线程负责释放资源，后续
 *       打算建立对象池)
 * */
#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <iostream>
#include <functional>
#include <memory>
#include <map>
#include "Socket.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "TimeWheel.h"

#define MAXCONNECTION 10000 // 允许的最大连接数

// 前向声明
class Channel;
class EventLoop;
  
class TcpServer
{
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::function<void(SP_TcpConnection, EventLoop*)> Callback;

    TcpServer(EventLoop* loop, int port, int threadNum = 0, int idleSeconds = 0);
    ~TcpServer();
    // 启动服务器
    void start();
    // 业务函数注册(一般在HttpSever构造函数中注册HttpServer的成员函数)
    void setNewConnCallback(Callback &&cb)
    {
        newConnectionCallback_ = cb; 
    }
private:
    // 连接清理工作
    void connectionCleanUp(int fd);
    // 监听套接字发生错误处理工作
    void onConnectionError();
    // 连接到来处理工作
    void onNewConnection();
    // data
    Socket serverSocket_;// TCP服务器的监听套接字
    EventLoop *loop_;// 所绑定的EventLoop
    EventLoopThreadPool eventLoopThreadPool_;
    SP_Channel spServerChannel_;// 监听连接事件注册器
    std::map<int, SP_TcpConnection> tcpList_;// 记录TCP server上的当前连接，并控制TcpConnection资源块的释放
    int connCount_;
    // 业务接口函数(上层Http的处理业务回调函数)
    Callback newConnectionCallback_;

    TimeWheel timeWheel_;
    bool removeIdleConnection_;
     
    void onTime();
    // TCP连接发送信息或者接收数据时，更新时间轮中的数据
    void isActive(SP_TcpConnection spTcpConn);
};

#endif
