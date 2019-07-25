/* author: chentongjie
 * date: */
//TcpConnection类，表示客户端连接

#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include <memory>
using std::cin;
using std::cout;
using std::endl;
#define BUFSIZE 4096

TcpConnection::TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr)
    : loop_(loop),
	  socket_(fd),
      clientaddr_(clientaddr),
	  halfClose_(false),
	  connected_(true),
      active_(false)
{
	//创建事件注册器，并将事件登记到注册器上
    spChannel_ = std::make_shared<Channel>();
    spChannel_->setFd(fd);
    spChannel_->setEvents(EPOLLIN | EPOLLET);
    spChannel_->setReadHandle(std::bind(&TcpConnection::handleRead, this));
    spChannel_->setWriteHandle(std::bind(&TcpConnection::handleWrite, this));
    spChannel_->setCloseHandle(std::bind(&TcpConnection::handleClose, this));
    spChannel_->setErrorHandle(std::bind(&TcpConnection::handleError, this));    
}

TcpConnection::~TcpConnection()
{
    // 打印日志
    cout << "call TcpConnection destructor!" << endl;
}

void TcpConnection::addChannelToLoop()
{
    // 主线程通过任务队列通知IO线程注册TcpConnection连接
	loop_->addTask(std::bind(&EventLoop::addChannelToEpoller, loop_, spChannel_));
}

void TcpConnection::send()
{
    if (connected_)
		sendInLoop();
    else
        cout << "Send()--The TcpConnection has closed" << endl;
}

/* 在HTTP层处理完数据后发送数据, 若数据还有剩余，则监听该套接字的可读事件 */
void TcpConnection::sendInLoop()
{
    if (!connected_) return;
    int result = sendn(socket_.fd(), bufferOut_);// 将bufferout_中的数据输出
    if(result >= 0)// 成功输出数据
    {
		event_t events = spChannel_->getEvents();// 若当前数据还有剩余，则需要在epoller_重新监听
        if(bufferOut_.size() > 0)
        {
			// 缓冲区满了，数据没发完，就设置EPOLLOUT事件触发			
			spChannel_->setEvents(events | EPOLLOUT);
			loop_->updateChannelToEpoller(spChannel_);
        }
		else
		{
			//数据已发完
			spChannel_->setEvents(events & (~EPOLLOUT));// 不再监听是否可写
			sendCompleteCallback_();
			if (connected_) 
                loop_->updateChannelToEpoller(spChannel_);
		}
    }
    else //发送出错
    {        
		handleError();
    }     
}

void TcpConnection::handleWrite()
{
    if (!connected_) return;
    int result = sendn(socket_.fd(), bufferOut_);
    if(result >= 0)
    {
		event_t events = spChannel_->getEvents();
        if(bufferOut_.size() > 0)
        {
			//缓冲区满了，数据没发完，就设置EPOLLOUT事件触发			
			spChannel_->setEvents(events | EPOLLOUT);
			loop_->updateChannelToEpoller(spChannel_);
        }
		else
		{
			//数据已发完
			spChannel_->setEvents(events & (~EPOLLOUT));
			sendCompleteCallback_();
			if (connected_) 
                loop_->updateChannelToEpoller(spChannel_);
		}
    }
    else
    {        
		handleError();
    }    
}

void TcpConnection::handleRead()
{
    if (!connected_) return;
    // 接收数据，写入bufferIn_
    int result = recvn(socket_.fd(), bufferIn_);

    // 如果有读入数据，则将读入的数据提交给Http层处理
    if(result > 0)
    {
        handleMessageCallback_(bufferIn_);
    }
    else if(result == 0)// 对端连接关闭,有可能为shutdown()或close()
    {
        halfClose_ = true;
        handleMessageCallback_(bufferIn_);
    }
    else
    {
        handleError();
    }
}

void TcpConnection::handleError()
{
	if(!connected_) return; 
	connected_ = false;
    active_ = false;
	loop_->removeChannelFromEpoller(spChannel_);
	errorCallback_();// 调用上层错误回调函数做错误处理工作
    // 通知服务器清理连接
	connectionCleanUp_();
}

// 对端关闭连接,有两种，一种close，另一种是shutdown(写半关闭)。
// 一般来说，两种关闭方式都会在把发送缓冲区中的内容发送出去后发送FIN报文，
// 故无法确定是哪种关闭方式。若为close关闭方式，服务器发送的数据会被客户端接收缓冲区丢弃
// 但服务器并不清楚是哪一种，只能按照最保险的方式来，即发完数据再close
void TcpConnection::handleClose()
{
	if (!connected_) { return; }
	connected_ = false;
    active_ = false;
    loop_->removeChannelFromEpoller(spChannel_);// 从epoller中清除注册信息
	closeCallback_();// 上层处理连接关闭工作
	connectionCleanUp_();
}
void TcpConnection::checkWhetherActive()
{
    if (loop_->isInLoopThread()) 
    {
        // 当前连接处于活跃状态
        if (active_) 
        {
            isActiveCallback_(shared_from_this());// 更新时间轮信息
            active_ = false;
        }
        else
        {
            // 否则，强制关闭连接
            forceClose();
        }
    }
    else
    {
        loop_->addTask(std::bind(&TcpConnection::checkWhetherActive, shared_from_this()));
    }
}

void TcpConnection::forceClose()
{
    if (loop_->isInLoopThread()) 
    {
        cout << "forceClose: close connection!" << endl;
        if (!connected_) { return; }
        connected_ = false;
        loop_->removeChannelFromEpoller(spChannel_);
        closeCallback_();
        connectionCleanUp_();
    } 
    else
    {
        cout << "it is not in the IO thread!" << endl;
        loop_->addTask(std::bind(&TcpConnection::forceClose, shared_from_this()));
    }
}
int TcpConnection::recvn(int fd, std::string &bufferIn)
{
    active_ = true;
    int nbyte = 0;
    int readSum = 0;
    char buffer[BUFSIZE];// 每次最多只读BUFSIZE个字节
    for(;;)
    {
        //nbyte = recv(fd, buffer, BUFSIZE, 0);
		nbyte = read(fd, buffer, BUFSIZE);
    	if (nbyte > 0)
		{
            bufferIn.append(buffer, nbyte);//效率较低，2次拷贝
            readSum += nbyte;
			continue;// 为了避免漏读文件结尾，需要read到出现EAGAIN错误
		}
		else if (nbyte < 0)//异常
		{
			if (errno == EAGAIN)//系统缓冲区未有数据，非阻塞返回
			{
				//std::cout << "EAGAIN,系统缓冲区未有数据，非阻塞返回" << std::endl;
				return readSum;
			}
			else if (errno == EINTR)// 被中断则重读
			{
				std::cout << "errno == EINTR" << std::endl;
				continue;
			}
			else
			{
				// 可能是RST
				perror("recv error");
				std::cout << "recv error" << std::endl;
				return -1;
			}
		}
		else// 返回0，客户端关闭socket，FIN
		{
//			std::cout << "client close the Socket" << std::endl;
            halfClose_ = true;
			return readSum;
		}
    }
}

int TcpConnection::sendn(int fd, std::string &bufferOut)
{
    active_ = true;
	ssize_t nbyte = 0;
	size_t length = 0;
    // 获取当前bufferOut的数值
	length = bufferOut.size();

    //nbyte = send(fd, buffer, length, 0);
	//nbyte = send(fd, bufferout.c_str(), length, 0);
    for (; ;) 
    {
	    nbyte = write(fd, bufferOut.c_str(), length);
	    if (nbyte > 0)
    	{
		    bufferOut.erase(0, nbyte);
	        return nbyte;
	    } 
        else if (nbyte < 0)//异常
	    {
		    if (errno == EAGAIN)//系统缓冲区满，非阻塞返回
		    {
			    std::cout << "write errno == EAGAIN,not finish!" << std::endl;
			    return 0;
		    }
		    else if (errno == EINTR)
		    {
			    std::cout << "write errno == EINTR" << std::endl;
                continue;
		    }
		    else if (errno == EPIPE)
		    {
			    //客户端已经close，并发了RST，继续write会报EPIPE，返回0，表示close
			    perror("write error");
			    std::cout << "write errno == client send RST" << std::endl;
			    return -1;
		    }
		    else
		    {
			    perror("write error");// Connection reset by peer
			    std::cout << "write error, unknow error" << std::endl;
			    return -1;
		    }
	    }
	    else//返回0
	    {
		    //应该不会返回0
		    //std::cout << "client close the Socket!" << std::endl;
		    return 0;
	    }
    }
}
