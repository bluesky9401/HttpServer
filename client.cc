#include <iostream>
#include <vector>
#include <cstring>
#include <functional>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <pthread.h>
#include "Thread.h"
using std::cout;
using std::endl;
using std::vector;
using std::function;
void threadFunc(const char *servip, int port)
{
    int sockfd;
    struct sockaddr_in servaddr;
    cout << "servip: " << servip << "port: " << port;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, servip, &servaddr.sin_addr);
    connect(sockfd, (sockaddr *) &servaddr, sizeof(servaddr));
    sleep(20);
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    int threadNum = atoi(argv[1]);
    if (threadNum < 1) return 0;
    const char *servip = argv[2];
    int servport = atoi(argv[3]);
    vector<Thread> threads(threadNum, Thread (std::bind(threadFunc, servip, servport)));
    for (int i = 0; i < threadNum; ++i) 
    {
        threads[i].start();
    }
    for (auto &t : threads) t.join();
    return 0;
}

