
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
int main(int argc, char *argv[])
{
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
