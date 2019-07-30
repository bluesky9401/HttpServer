/* author: chentongjie
 * last updated: 7-22, 2019
 * */

#include <iostream>
#include "HttpSession.h"
#include "HttpServer.h"
#include "TcpConnection.h"
using namespace std::placeholders;
using std::cin;
using std::cout;
using std::endl;

HttpServer::HttpServer(EventLoop *loop, int port, int ioThreadNum, int idleSeconds)
    : tcpServer_(loop, port, ioThreadNum, idleSeconds)
{
    /* 将HTTP层新建连接处理函数注册到tcpServer_对象 
     * 使用std::bind封装函数的接口，同时将处理函数与该HttpServer对象绑定 */
    tcpServer_.setNewConnCallback(std::bind(&HttpServer::handleNewConnection, this, _1, _2));
}

/* HttpServer的生命期为整个程序运行期间 */
HttpServer::~HttpServer()
{
    cout << "call HttpServer destructor!" << endl;
}

/* 连接到来时上层所需处理的业务 
 * 注意：此函数调用只会发生在主线程,关联HttpSession与TcpConnection 
 * 注意：仅需HttpSession持有指向TcpConnection的指针
 * */
void HttpServer::handleNewConnection(SP_TcpConnection spTcpConn, EventLoop *loop)
{
    SP_HttpSession spHttpSession = std::make_shared<HttpSession>(spTcpConn, loop);// 创建新的会话

    // 在此将spHttpSession指向对象中的处理函数登记到spTcpConn指向的对象
    // 注意：此处将spHttpSession指向对象的生命期交由spTcpConn控制
    spTcpConn->setHandleMessageCallback(std::bind(&HttpSession::handleMessage, spHttpSession, _1));
    spTcpConn->setSendCompleteCallback(std::bind(&HttpSession::handleSendComplete, spHttpSession));
    spTcpConn->setCloseCallback(std::bind(&HttpSession::handleClose, spHttpSession));
    spTcpConn->setHalfCloseCallback(std::bind(&HttpSession::handleHalfClose, spHttpSession));
    spTcpConn->setErrorCallback(std::bind(&HttpSession::handleError, spHttpSession));
}

/* 启动HTTP网络服务器 */
void HttpServer::start()
{
    tcpServer_.start();
}
