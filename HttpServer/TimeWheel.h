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
