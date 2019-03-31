/* author: chentongjie
 * date:
 * */

#include <iostream>
#include <functional>
#include "MutexLock.h"
#include "HttpServer.h"

HttpServer::HttpServer(EventLoop *loop, int port, int ioThreadNum)
    : tcpServer_(loop, port, ioThreadNum)
{
    /* 将HTTP层新建连接处理函数注册到tcpServer_对象 
     * 使用std::bind封装函数的接口，同时将处理函数与该HttpServer对象绑定 */
    tcpServer_.setNewConnCallback(std::bind(&HttpServer::handleNewConnection, this, std::placeholders::_1));
}

/* HttpServer的生命期为整个程序运行期间 */
HttpServer::~HttpServer()
{

}

/* 连接到来时上层所需处理的业务 
 * 注意：此函数调用发生只会发生在主线程 
 * 关联HttpSession与TcpConnection 
 * 注意：仅需HttpSession持有指向TcpConnection的指针
 * */
void HttpServer::handleNewConnection(SP_TcpConnection spTcpConn)
{
    SP_HttpSession spHttpSession = std::make_shared<HttpSession>(spTcpConn);// 创建新的会话

    // 在此将spHttpSession指向对象中的处理函数登记到spTcpConn指向的对象
    // 注意：此处将spHttpSession指向对象的生命期交由spTcpConn控制
    spTcpConn->setHandleMessageCallback(std::bind(&HttpSession::handleMessage, spHttpSession, std::placeholders::_1));
    spTcpConn->setSendCompleteCallback(std::bind(&HttpSession::handleSendComplete, spHttpSession));
    spTcpConn->setCloseCallback(std::bind(&HttpSession::handleClose, spHttpSession));
    spTcpConn->setErrorCallback(std::bind(&HttpSession::handleError, spHttpSession));
}

/* 启动HTTP网络服务器 */
void HttpServer::start()
{
    tcpServer_.start();
}
