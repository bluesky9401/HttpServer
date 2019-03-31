/* author: chentongjie
 * date:
 * TcpConnection类持有一个事件注册器，读写缓冲区，保存连接信息，同时关联一个事件分发器EventLoop
 *
 * */
#ifndef _TCP_CONNECTION_H_
#define _TCP_CONNECTION_H_

#include <functional>
#include <string>
#include <memory>
// #include <sys/types.h>
// #include <sys/socket.h>
#include <netinet/in.h>
// #include <arpa/inet.h>
#include "Channel.h"
#include "EventLoop.h"
class HttpSession;
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    friend HttpSession;
    typedef uint32_t event_t;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::function<void()> Callback;
    typedef std::function<void(std::string&)> HandleMessageCallback;// 上层Http业务处理函数
    typedef std::function<void()> TaskCallback;
    
    TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr);
    ~TcpConnection();

    int fd()
    { return fd_; }
    //void FillBuffout(std::string &s);

    void addChannelToLoop();

    // 当前IO线程发送函数(TCP层的处理回调函数)
    void send();
    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();
    
    // 设置HTTP层回调函数
    void setHandleMessageCallback(HandleMessageCallback &&cb)
    {
        handleMessageCallback_ = cb;
    }
    void setSendCompleteCallback(Callback &&cb)
    {
        sendCompleteCallback_ = cb;
    }
    void setCloseCallback(Callback &&cb)
    {
        closeCallback_ = cb;
    }
    void setErrorCallback(Callback &&cb)
    {
        errorCallback_ = cb;
    }

    // 连接清理函数(由IO线程推送到主线程所持有的事件分发器任务队列)
    void setConnectionCleanUp(const TaskCallback &cb)
    {
        connectionCleanUp_ = cb;
    }
    
private:
    void sendInLoop();

    EventLoop *loop_;
    SP_Channel spChannel_;
    int fd_;
    struct sockaddr_in clientaddr_;
    bool halfClose_; //半关闭标志位
    bool disconnected_; //已关闭标志位

    // 读写缓冲
    std::string bufferIn_;
    std::string bufferOut_;
    
    // HTTP层处理函数
    HandleMessageCallback handleMessageCallback_;
    Callback sendCompleteCallback_;
    Callback closeCallback_;
    Callback errorCallback_;

    TaskCallback connectionCleanUp_;
};
#endif 
