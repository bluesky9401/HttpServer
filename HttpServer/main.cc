/* author: chen tongjie
 * last updated: 2019-7-21
 * HttpServer */
#include <iostream>
#include <signal.h>
#include <sys/prctl.h>
#include "EventLoop.h"
#include "HttpServer.h"
#include "ThreadPool.h"
#include "CurrentThread.h"

using std::cout;
using std::endl;
EventLoop *mlp;
ThreadPool *pThreadPool;

int main(int argc, char *argv[])
{
    CurrentThread::t_threadName = "Main Thread";
    prctl(PR_SET_NAME, CurrentThread::t_threadName);// 给当前线程命名
    cout << "main thread " << CurrentThread::tid() << " start..." << endl;
    // 设置SIG_IGN信号的默认操作为忽略该信号，服务器接收到客户端发送
    // 的RST分节后，TCP连接释放。若服务器再继续对套接字进行读写操作，
    // 会触发SIGPIPE信号。
    signal(SIGPIPE, SIG_IGN);
    /* port: 监听套接字的端口号
     * ioThreadNum: IO线程数目
     * workerThreadNum: 工作线程数目
     * idleSeconds: 空闲连接超时时间。
     * */
    int port = 80;
    int ioThreadNum = 4;
    int workerThreadNum = 0;
    int idleSeconds = 0;
    if(argc == 5)
    {
        port = atoi(argv[1]);
        ioThreadNum = atoi(argv[2]);
        workerThreadNum = atoi(argv[3]);
        idleSeconds = atoi(argv[4]);
    }
    pThreadPool = new ThreadPool(workerThreadNum);
    mlp = new EventLoop();
    HttpServer httpServer(mlp, port, ioThreadNum, idleSeconds);
    // 启动线程池以及http服务器
    pThreadPool->start();
    httpServer.start();
    mlp->loop();
    delete pThreadPool;
    delete mlp;
    return 0;
}
