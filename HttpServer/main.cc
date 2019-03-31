
#include <signal.h>
#include "EventLoop.h"
//#include "TcpServer.h"
#include "HttpServer.h"
#include "ThreadPool.h"
#include <sstream>

#define MAXTASKSIZE 1000

EventLoop *mlp;
ThreadPool *pThreadPool;
//gprof
static void sighandler1( int sig_no )   
{   
      exit(0);   
}   
static void sighandler2( int sig_no )   
{   
    mlp->quit();// 关闭主线程的EventLoop
}   

int main(int argc, char *argv[])
{
    signal(SIGUSR1, sighandler1);
    signal(SIGUSR2, sighandler2);// 注册信号处理函数
    //signal(SIGINT, sighandler2);
    signal(SIGPIPE, SIG_IGN);  //SIG_IGN,系统函数，忽略信号的处理程序,客户端发送RST包后，服务器还调用write会触发
    int port = 80;
    int ioThreadNum = 4;
    int workerThreadNum = 0;
    if(argc == 4)
    {
        port = atoi(argv[1]);
        ioThreadNum = atoi(argv[2]);
        workerThreadNum = atoi(argv[3]);
    }
    pThreadPool = new ThreadPool(workerThreadNum, MAXTASKSIZE);

    EventLoop loop;
    mlp = &loop;
    HttpServer httpServer(&loop, port, ioThreadNum);

    pThreadPool->start();
    httpServer.start();
    loop.loop();
    delete pThreadPool;
    return 0;
}
