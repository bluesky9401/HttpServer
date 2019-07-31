/* author: chentongjie
 * date:
 * TcpConnection类持有一个事件注册器，读写缓冲区，保存连接信息，同时关联一个事件分发器EventLoop
 * 只有TCP服务器端保存有指向TcpConnection的智能指针，因此要释放TcpConnection的唯一方法是通知服务器
 * 析构该SP_TcpConnection对象。同时，由于Channel中有TcpConnection的裸指针，因此为了确保安全，必须在
 * 释放TcpConnection对象之前，释放Channel对象。
 * TcpConnection连接关闭的情况：
 * 1. 接收到FIN分节(需要分为两种情况：与数据一起到达缓冲区与单独到达缓冲区的情况)
 * 2. TCP连接发生错误
 * 3. 上层Http解析错误，强制关闭连接
 * 4. 连接超时，强制关闭连接
 * 情况1关闭步骤：若read()返回0，表明接收到FIN，连接状态halfClose_置为true，并通知HTTP层状态变为halfClose.
 *                服务端调用handleClose()情况有两种：一种与数据一起到达缓冲区，在HTTP层处理完成后，并且HTTP
 *                层当前工作处理完毕关闭，会通知TCP层HTTP层关闭，然后在发送缓冲区的数据发送完成后关闭连接; 
 *                另一种情况是recvn操作仅接收到一个文件结束符，因此会向HTTP层传递一个空字符串，若当前HTTP层
 *                未关闭并且处理报文为空，即调用handleClose()关闭连接，若当前HTTP层还有剩余工作未处理，由于
 *                设置了halfclose位，在工作完成后，执行关闭操作并通知TCP层，然后在发送缓冲区数据发送完毕时
 *                关闭连接。
 * 情况2关闭步骤：连接发生错误，例如接收到对端发送的RST报文，则调用HTTP层errorCallback回调函数关闭HTTP层会话，
 *                然后直接调用forceClose()关闭连接。
 * 情况3关闭步骤：HTTP层解析错误，关闭HTTP层并通知TCP层，然后在发送完差错报文后关闭连接。
 * 情况4关闭步骤：直接调用forceClose()函数主动关闭。
 * */
#ifndef _TCP_CONNECTION_H_
#define _TCP_CONNECTION_H_

#include <functional>
#include <string>
#include <memory>
#include <netinet/in.h>
#include "Socket.h"

class HttpSession;
class EventLoop;
class Channel;
class Entry;
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    friend HttpSession;
    typedef uint32_t event_t;
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::weak_ptr<Entry> WP_Entry;
    typedef std::function<void()> Callback;
    typedef std::function<void(std::string&)> HandleMessageCallback;// 上层Http业务处理函数
    typedef std::function<void()> TaskCallback;
    typedef std::function<void(std::shared_ptr<TcpConnection>)> IsActiveCallback;
    
    TcpConnection(EventLoop *loop, int fd, struct sockaddr_in clientaddr);
    ~TcpConnection();

    int fd() const
    { 
        return socket_.fd(); 
    }
    bool isConnect() const
    {
        return connected_;
    }
    void addChannelToLoop();
    void notifyHttpClosed()
    {
        httpClosed_ = true;
    }

    // 设置HTTP层回调函数
    void setHandleMessageCallback(const HandleMessageCallback &&cb)
    {
        handleMessageCallback_ = cb;
    }
    void setSendCompleteCallback(const Callback &&cb)
    {
        sendCompleteCallback_ = cb;
    }
    void setCloseCallback(const Callback &&cb)
    {
        closeCallback_ = cb;
    }
    void setHalfCloseCallback(const Callback &&cb)
    {
        halfCloseCallback_ = cb;
    }
    void setErrorCallback(const Callback &&cb)
    {
        errorCallback_ = cb;
    }
    // 连接清理函数(由IO线程推送到主线程所持有的事件分发器任务队列)
    void setConnectionCleanUp(const TaskCallback &&cb)
    {
        connectionCleanUp_ = cb;
    }
    // 更新TCP服务器上的时间轮回调函数
    void setIsActiveCallback(const IsActiveCallback && cb)
    {
        isActiveCallback_ = cb;
    }
    void checkWhetherActive();
    void forceClose();
private:
    // TCP层的处理函数
    void send();// 主动发送操作
    void sendInLoop();
    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();
    int recvn(int fd, std::string &bufferIn);
    int sendn(int fd, std::string &bufferOut);

    EventLoop *loop_;
    SP_Channel spChannel_;
    Socket socket_;
    struct sockaddr_in clientaddr_;
    bool halfClose_; // 半关闭标志位
    bool connected_; // 连接标志位
    bool active_; // 当前连接是否活跃
    bool httpClosed_;
    // 读写缓冲
    std::string bufferIn_;
    std::string bufferOut_;
    
    // HTTP层回调函数
    HandleMessageCallback handleMessageCallback_;
    Callback sendCompleteCallback_;
    Callback closeCallback_;
    Callback halfCloseCallback_;
    Callback errorCallback_;

    // TCP服务器处理函数
    TaskCallback connectionCleanUp_;
    IsActiveCallback isActiveCallback_;
};
#endif 
