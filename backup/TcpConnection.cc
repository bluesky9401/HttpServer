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

#define BUFSIZE 4096

/* 底层IO操作(从套接字读取数据) */
int recvn(int fd, std::string &bufferIn);
int sendn(int fd, std::string &bufferOut);

TcpConnection::TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr)
    : loop_(loop),
	fd_(fd),
    clientaddr_(clientaddr),
	halfClose_(false),
	disconnected_(false)
{
	//创建事件注册器，并将事件登记到注册器上
    spChannel_ = std::make_shared<Channel>();
    spChannel_->setFd(fd_);
    spChannel_->setEvents(EPOLLIN | EPOLLET);
    spChannel_->setReadHandle(std::bind(&TcpConnection::handleRead, this));
    spChannel_->setWriteHandle(std::bind(&TcpConnection::handleWrite, this));
    spChannel_->setCloseHandle(std::bind(&TcpConnection::handleClose, this));
    spChannel_->setErrorHandle(std::bind(&TcpConnection::handleError, this));    
}

TcpConnection::~TcpConnection()
{
	// 移除事件
	loop_->removeChannelToEpoller(spChannel_);// 从epoller中清除注册信息
	close(fd_);
    std::cout << "free TcpConnection " << std::endl;
}

void TcpConnection::addChannelToLoop()
{
    // 主线程通过任务队列通知EventLoopThreadPool中的线程注册该Connection感兴趣的事件
	loop_->addTask(std::bind(&EventLoop::addChannelToEpoller, loop_, spChannel_));
}

/* 由于有可能在工作线程池中调用send(), 故需判断当前线程是否为IO线程 */
void TcpConnection::send()
{
	 // 判断当前IO线程是否为处理该connection的线程
	 if(loop_->isInLoopThread())
	 {
		sendInLoop();
	 }
	 else
	{
		//不是，则是跨线程调用,加入IO线程的任务队列，唤醒
		loop_->addTask(std::bind(&TcpConnection::sendInLoop, this));
	}
}

/* 在HTTP层处理完数据后发送数据, 若数据还有剩余，则监听该套接字的可读事件 */
void TcpConnection::sendInLoop()
{
    int result = sendn(fd_, bufferOut_);// 将bufferout_中的数据输出
    if(result > 0)// 成功输出数据
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
			sendCompleteCallback_();// 调用发送完成回调函数(目前不做工作)

			if(halfClose_)
				handleClose();
		}
    }
    else if(result < 0) //发送出错
    {        
		handleError();
    }
	else 
	{
		handleClose();
	}     
}

void TcpConnection::handleRead()
{
    // 接收数据，写入缓冲区
    int result = recvn(fd_, bufferIn_);

    // 业务回调,可以利用工作线程池处理，投递任务
    if(result > 0)
    {
        handleMessageCallback_(bufferIn_);
    }
    else if(result == 0)
    {
        handleClose();
    }
    else
    {
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    int result = sendn(fd_, bufferOut_);
    if(result > 0)
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
			//发送完毕，如果是半关闭状态，则可以close了
			if(halfClose_)
				handleClose();
		}
    }
    else if(result < 0)
    {        
		handleError();
    }
	else
	{
		handleClose();
	}     
}

void TcpConnection::handleError()
{
	if(disconnected_) { return; }
	errorCallback_();// 调用上层错误回调函数做错误处理工作
	//loop_->RemoveChannelToPoller(pchannel_);
	//连接标记为清理
	//task添加
	disconnected_ = true;
	loop_->addTask(connectionCleanUp_);
}

//对端关闭连接,有两种，一种close，另一种是shutdown(半关闭)，但服务器并不清楚是哪一种，只能按照最保险的方式来，即发完数据再close
void TcpConnection::handleClose()
{
	//移除事件
	//loop_->RemoveChannelToPoller(pchannel_);
	//连接标记为清理
	//task添加
	//loop_->AddTask(connectioncleanup_);
	//closecallback_(this);

	//20190217 优雅关闭，发完数据再关闭
	if(disconnected_) { return; }
	if(bufferOut_.size() > 0 || bufferIn_.size() > 0)
	{
		//如果还有数据待发送，则先发完,设置半关闭标志位
		halfClose_ = true;
		//还有数据刚刚才收到，但同时又收到FIN
		if(bufferIn_.size() > 0)
		{
			handleMessageCallback_(bufferIn_);
		}
	}
	else
	{
		disconnected_ = true;
		closeCallback_();// 上层处理连接关闭工作
		loop_->addTask(connectionCleanUp_);
	}
}

int recvn(int fd, std::string &bufferIn)
{
    int nbyte = 0;
    int readSum = 0;
    char buffer[BUFSIZE];
    for(;;)
    {
        //nbyte = recv(fd, buffer, BUFSIZE, 0);
		nbyte = read(fd, buffer, BUFSIZE);
		
    	if (nbyte > 0)
		{
            bufferIn.append(buffer, nbyte);//效率较低，2次拷贝
            readSum += nbyte;
			if(nbyte < BUFSIZE)// 读取已完成
				return readSum;// 读优化，减小一次读调用，因为一次调用耗时10+us
			else
				continue;
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
			std::cout << "client close the Socket" << std::endl;
			return 0;
		}
    }
}

int sendn(int fd, std::string &bufferOut)
{
	ssize_t nbyte = 0;
    int sendSum = 0;
	size_t length = 0;
    // 获取当前bufferOut的数值
	length = bufferOut.size();

    // 注意：一次sendInLoop最多只发送BUFSIZE个字节的数据
    // 若length大于BUFSIZE, 则剩余数据在下次缓冲区可写时发送
	 if(length >= BUFSIZE)
	 {
	    length = BUFSIZE;// 仅发送BUFSIZE大小的数据
	 }

	for (;;)
	{
		//nbyte = send(fd, buffer, length, 0);
		//nbyte = send(fd, bufferout.c_str(), length, 0);
		nbyte = write(fd, bufferOut.c_str(), length);
		if (nbyte > 0)
		{
            sendSum += nbyte;
			bufferOut.erase(0, nbyte);
            return sendSum;
			length = bufferOut.size();// 更新bufferOut的length值
            
			 if(length >= BUFSIZE)
			 {
				length = BUFSIZE;
			 }

			 if (length == 0 )
			 {
				return sendSum;
			}
		}
		else if (nbyte < 0)//异常
		{
			if (errno == EAGAIN)//系统缓冲区满，非阻塞返回
			{
				std::cout << "write errno == EAGAIN,not finish!" << std::endl;
				return sendSum;
			}
			else if (errno == EINTR)
			{
				std::cout << "write errno == EINTR" << std::endl;
				continue;
			}
			else if (errno == EPIPE)
			{
				//客户端已经close，并发了RST，继续wirte会报EPIPE，返回0，表示close
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
