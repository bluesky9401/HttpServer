/* @author: chen tongjie
 * @date:
 * @description--服务器socket类，封装socket描述符及相关的初始化操作
 * 主要用于封装服务器socket的操作 */
#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class Socket
{
public:
    Socket(/* args */);
    ~Socket();

    int fd() const { return fd_; }    
    void setSocketOption();
    void setReuseAddr();
    void setNonblocking();
    bool bindAddress(int serverport);
    bool listen();
    int accept(struct sockaddr_in &clientaddr);
    bool close();
private:
    /* data */
    int fd_;
    bool closed_;
    
};

#endif
