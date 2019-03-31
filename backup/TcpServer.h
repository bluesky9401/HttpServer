/* author:chentongjie
 * date:
 * TcpServer持有一个EventLoopThreadPool线程池、accept套接字、EventLoop对象、serverChannel事件注册器
 * 作用：负责向所持有的EventLoop事件分发器登记等待连接事件，同时在连接到来时新建一个TcpConnection对象，
 *       并从EventLoopThreadPool线程池中捞出一个IO线程与该TcpConnection对象绑定, 然后往IO线程中推送连
 *       接事件注册任务，将该连接登记到IO线程中, 该连接的读写任务均有该IO线程负责，
 *       // 主线程只负责在连接关闭或连接发生错误时释放连接所占有的资源。(目前由IO线程负责释放资源，后续
 *       打算建立对象池)
 * */
#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <functional>
#include <string>
#include <memory>
#include <map>
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"

// 注意，端口限制在0--65535
#define MAXCONNECTION 20000 // 允许的最大连接数

class TcpServer
{
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::function<void(SP_TcpConnection)> Callback;

    TcpServer(EventLoop* loop, int port, int threadNum = 0);
    ~TcpServer();

    void start();

    // 业务函数注册(一般在HttpSever构造函数中注册HttpServer的成员函数)
    void setNewConnCallback(Callback &&cb)
    {
        newConnectionCallback_ = cb; 
    }

private:
    /* data */
    Socket serverSocket_;// TCP服务器的监听套接字C
    EventLoop *loop_;// 所绑定的EventLoop
    EventLoopThreadPool eventLoopThreadPool_;
    SP_Channel spServerChannel_;
    std::map<int, SP_TcpConnection> tcpList_;
    int connCount_;

    // 业务接口函数(上层Http的处理业务回调函数)
    Callback newConnectionCallback_;

    // 传输层对连接处理的函数，业务无关
    void onNewConnection();
    void removeConnection(int fd);
    void onConnectionError();
};

#endif
