/* author: chentongjie
 * last edited: 2019-7-28
 * 时间轮设计：采用boost库中的circular_buffer作为时间轮容器，采用timerfd作为定时器，
 * 每过1s，则将时间轮旋转1格。若TcpServer开启了剔除空闲连接，则当新建连接时，向时间
 * 轮中插入一个与TcpConnection绑定的Entry的智能指针。原先设计参考muduo库，每当TcpCo-
 * -nnection上有数据收发时，即向主线程任务队列中插入一个指向Entry的智能指针，若连接在
 * TcpServer设定的空闲时间内均没有数据收发，则时间轮中指向Entry的智能指针全部析构，从
 * 而触发Entry的析构动作，然后强行关闭TCP连接。然而，由于每次读写数据都需要向主线程任
 * 务队列插入一个更新时间轮的回调函数。一般来说，数据的收发是一个高频动作(压测时)，会
 * 使得更新时间轮的操作大大占据了系统CPU时间，经过测试，该法会导致吞吐率下降1/3左右。
 * 因此，修正时间轮设计，改为在TcpServer新建连接时，向时间轮插入一个指向Entry的智能指针，
 * 然后在空闲时间到达时，智能指针释放触发Entry的析构动作，然后查询Tcp连接在空闲事件内是否
 * 发送或接收过数据。若连接处于活跃状态，则重新向时间轮插入与该Tcp连接绑定的Entry的智能指
 * 针，否则关闭连接。与没有启动空闲连接剔除的情况相比，修正后的时间轮设计仅导致吞吐率略微
 * 下降。同时，任意时刻时间轮只有一个指向Entry的智能指针，内存使用较之前设计也大大降低。
 * 假设空闲时间设定为idles，由于采用的是判断在[k*idles,(k+1)*idles]是否有数据收发来确定是否
 * 强制关闭连接。因此，在极端情况下，一个空闲连接需要等待2*idles的时间才关闭。
 * */

#ifndef _TIME_WHEEL_H_
#define _TIME_WHEEL_H_
#include <memory>
#include <unordered_set>
#include <boost/circular_buffer.hpp>
#include "TcpConnection.h"

// 控制空闲连接是否关闭的条目,将指向其的shared_ptr指针插入时间轮
// 在其析构时，若当前连接还没有释放，则强制关闭连接。
struct Entry
{
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::weak_ptr<TcpConnection> WP_TcpConnection;

    explicit Entry(const WP_TcpConnection& wpConn)
            : wpConn_(wpConn)
    { }
    ~Entry()
    {
        SP_TcpConnection spConn = wpConn_.lock();
        if (spConn)
        {
            // 注意：此处将任务推至IO线程中运行
            // 查询IO线程中该连接在[t0, t0+intervals]是否发送或接收过数据
            spConn->checkWhetherActive();
        }
    }
    WP_TcpConnection wpConn_;
};
class TimeWheel {
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::weak_ptr<TcpConnection> WP_TcpConnection;
    typedef std::shared_ptr<Entry> SP_Entry;
    typedef std::weak_ptr<Entry> WP_Entry;
    typedef std::unordered_set<SP_Entry> Bucket;
    typedef boost::circular_buffer<Bucket> WeakConnectionList;

    TimeWheel(int timeout_) 
        : connectionBuckets_(timeout_)
    { }
    // 时间轮向下转一格
    void rotateTimeWheel()
    {
        connectionBuckets_.push_back(Bucket());
    }
    // 向时间轮添加条目(即向其中加入连接)
    void addConnection(SP_TcpConnection spTcpConn)
    {
        SP_Entry spEntry(new Entry(spTcpConn));
        connectionBuckets_.back().insert(spEntry);
    } 
private:
    WeakConnectionList connectionBuckets_;
};
#endif
