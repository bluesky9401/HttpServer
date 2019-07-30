#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include "Socket.h"

// Socket()构造函数创建一个套接字
// Socket(int fd)构造函数封装一个描述符为fd的套接字
Socket::Socket()
{
    closed_ = true;
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == fd_)
    {
        perror("socket create fail!");
        exit(-1);
    }
    closed_ = false;
    std::cout << "Socket create: " << fd_ << std::endl;
}

Socket::~Socket()
{
    if (!closed_) {
        ::close(fd_);
//        std::cout << "Socket: " << fd_ << "close..." << std::endl;
    }
}

void Socket::setSocketOption()
{
    ;
}

void Socket::setReuseAddr()// 将socket设置为地址可复用
{
    int on = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
}

void Socket::setNonblocking()
{
    int opts = fcntl(fd_, F_GETFL);
    if (opts<0)
    {
        perror("fcntl(serverfd_,GETFL)");
        exit(1);
    }
    if (fcntl(fd_, F_SETFL, opts | O_NONBLOCK) < 0)
    {
        perror("fcntl(serverfd_,SETFL,opts)");
        exit(1);
    }
    std::cout << "set socket setnonblocking..." << std::endl;
}

bool Socket::bindAddress(int port)
{
    struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	int resval = bind(fd_, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (resval == -1)
	{
		::close(fd_);
        closed_ = true;
		perror("error bind");
		exit(1);
	}
    std::cout << "server bindaddress..." << std::endl;
    return true;
}

bool Socket::listen()
{    
	if (::listen(fd_, 2048) < 0)
	{
		perror("error listen");
        closed_ = true;
		::close(fd_);
		exit(1);
	}
    std::cout << "server listenning..." << std::endl;
    return true;
}

int Socket::accept(struct sockaddr_in &clientaddr)
{
    socklen_t lengthofsockaddr = sizeof(clientaddr);
    int clientfd = ::accept(fd_, (struct sockaddr*)&clientaddr, &lengthofsockaddr);
    if (clientfd < 0) 
    {
        //perror("error accept");
        //if(errno == EAGAIN)
            //return 0;
		//std::cout << "error accept:there is no new connection accept..." << std::endl;
        return clientfd;
	}
    //std::cout << "server accept,clientfd: " << clientfd << std::endl;
    return clientfd;
}
// 为避免重复关闭, 第一次关闭需设置closed_
bool Socket::close()
{
    if (!closed_) 
    {
        ::close(fd_);
        closed_ = true;
        std::cout << "server close..." << std::endl;
    }
    return true;
}

