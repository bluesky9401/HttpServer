/* author: chentongjie
 * date:
 * HttpServer类: 持有TcpServer类对象，并定义在新连接到来时所需进行的操作
 * 新建连接：void handleNewConnection(TcpConnection *pTcpConn);
 * 在新建连接到来时，负责创建一个HttpSession对象，然后向spTcpConn指向的TCP连接对象
 * 登记HTTP层的处理操作。
*/
#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <memory>
#include "EventLoop.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include "HttpSession.h"

class HttpServer {
public:
    typedef std::shared_ptr<HttpSession> SP_HttpSession;
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    HttpServer(EventLoop *loop, int port, int ioThreadNum);
    ~HttpServer();

    void start();

private:
    /* 新建连接时所进行的处理(注册至tcpserver_, 并在tcpserver_中的回调函数中被调用) */
    void handleNewConnection(SP_TcpConnection spTcpConn); 
    
    TcpServer tcpServer_;
    
};


#endif
