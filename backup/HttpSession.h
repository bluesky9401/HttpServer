
// GET /register.do?p={%22username%22:%20%2213917043329%22,%20%22nickname%22:%20%22balloon%22,%20%22password%22:%20%22123%22} HTTP/1.1\r\n
// GET / HTTP/1.1
// Host: bigquant.com
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8
// Accept-Encoding: gzip, deflate, br
// Accept-Language: zh-CN,zh;q=0.9,en;q=0.8
// Cookie: _bdid_=059a16ee3bef488b9d5212c81e2b688d; Hm_lvt_c58f67ca105d070ca7563b4b14210980=1550223017; _ga=GA1.2.265126182.1550223018; _gid=GA1.2.1797252688.1550223018; Hm_lpvt_c58f67ca105d070ca7563b4b14210980=1550223213; _gat_gtag_UA_124915922_1=1

// HTTP/1.1 200 OK
// Server: nginx/1.13.12
// Date: Fri, 15 Feb 2019 09:57:21 GMT
// Content-Type: text/html; charset=utf-8
// Transfer-Encoding: chunked
// Connection: keep-alive
// Vary: Accept-Encoding
// Vary: Cookie
// X-Frame-Options: SAMEORIGIN
// Set-Cookie: __bqusername=""; Domain=.bigquant.com; expires=Thu, 01-Jan-1970 00:00:00 GMT; Max-Age=0; Path=/
// Access-Control-Allow-Origin: *
// Content-Encoding: gzip

#ifndef _HTTP_SESSION_H_
#define _HTTP_SESSION_H_

#include <string>
#include <set>
#include <memory>
#include "ThreadPool.h"
#include "TcpConnection.h"

extern ThreadPool *pThreadPool;

struct HttpProcessContext {
    std::string requestContext;// 待解析的请求报文
    std::string responseContext;// 待发送响应报文
	std::string method;
	std::string url;
	std::string version;
	std::map<std::string, std::string> header;
	std::string body;
};// 存放HTTP请求信息

class HttpSession
{
public:
    typedef std::shared_ptr<TcpConnection> SP_TcpConnection;
    typedef std::shared_ptr<HttpProcessContext> SP_HttpProcessContext;

    HttpSession(SP_TcpConnection spTcpConn);
    ~HttpSession();

    /* HTTP层数据处理、读写事件处理与会话关闭或发生错误处理 */
    void handleMessage(std::string &s);
    void handleSendComplete();
    void handleClose();
    void handleError();

    void handleMessageTask(SP_HttpProcessContext spProContext);

private:
    /* 分离接收缓冲区的报文 */
    int parseRcvMsg();

    /* 解析HTTP请求报文，将其存放至httpRequestContext_对象中 */
    void parseHttpRequest(SP_HttpProcessContext spProContext);

    /* 根据httpRequestContext_中的信息进行处理，并且填充响应报文 */
    void httpProcess(SP_HttpProcessContext spProContext);

    /* 将响应报文的内容转移到发送缓冲区 */
    void addToBuf(SP_HttpProcessContext spProContext);

    void httpError(int err_num, std::string short_msg, SP_HttpProcessContext spProContext);

    bool keepAlive()
    { return keepAlive_;}
    
    std::string recvMsg_;// 从缓冲区接收的报文
    
    // 此为当前待处理报文
    std::set<SP_HttpProcessContext> setHttpProcessContext_;
    SP_HttpProcessContext spCurrPro_; // 指向当前正在处理的报文
    std::string currMethod_; // 当前正在处理请求报文的类型
    bool completed_ = false;// 用于标记当前报文是否完整
    size_t remain_ = 0;// 用于记录POST请求还剩余多少数据未读
    size_t crlfcrlfPos_ = std::string::npos;// 记录“\r\n\r\n”的接收位置

    bool keepAlive_;
    bool connected_ = true;
    int taskNum_ = 0;// 存放当前HttpSession在工作线程的任务数
    SP_TcpConnection spTcpConn_;// 此处持有SP_TcpConnection指针,注意在连接关闭时释放
};
#endif

