/* @author: chen tongjie
 * @date: 
 * */

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
/* 1xx: 服务器收到请求，需要请求者继续执行操作
   2xx: 代表请求已成功被服务器接收、理解、并接受。
   3xx: 重定向，代表需要客户端采取进一步的操作才能完成请求，
        这些状态码用来重定向，后续的请求地址（重定向目标）在本次响应的 Location 域中指明。
   4xx: 客户端错误，请求包含语法错误或无法完成请求。
   5xx: 服务器错误，服务器在处理请求的过程中发生了错误。

常用状态码：
   200：请求被正常处理 
   204：请求被受理但没有资源可以返回 
   206：客户端只是请求资源的一部分，服务器只对请求的部分资源执行GET方法，相应报文中通过Content-Range指定范围的资源。

   301：永久性重定向 
   302：临时重定向 
   303：与302状态码有相似功能，只是它希望客户端在请求一个URI的时候，能通过GET方法重定向到另一个URI上 
   304：发送附带条件的请求时，条件不满足时返回，与重定向无关 
   307：临时重定向，与302类似，只是强制要求使用POST方法
 
   400：请求报文语法有误，服务器无法识别
   401：请求需要认证 
   403：请求的对应资源禁止被访问 
   404：服务器无法找到对应资源 

   500：服务器内部错误 
   503：服务器正忙
*/
#include <memory>
#include <iostream>
#include <sstream>
using std::stringstream;
#include <string>
using std::string;
#include <unordered_map>
using std::unordered_map;
#include <cstring>
#include <exception>
using std::logic_error;
#include "HttpSession.h"
#include "TcpConnection.h"
#include "EventLoop.h"
using std::cin;
using std::cout;
using std::endl;

HttpSession::HttpSession(SP_TcpConnection spTcpConn, EventLoop *loop)
    : loop_(loop),
      mapHttpProcessContext_(),
      spCurrPro_(),
      prepareContext_(),
      currMethod_(),
      completed_(false),
      remain_(0),
      crlfcrlfPos_(string::npos),
      requestCount_(0),
      sendId_(1),
      keepAlive_(false),
      connected_(true),
      wpTcpConn_(spTcpConn)
{
    // 打印日志
//    cout << "HttpSession construct!" << endl;
}

// 仅在TcpConnection开始销毁，并且线程池或IO线程任务队列中没有指向
// HttpSession的智能指针时，开始调用析构函数销毁对象。 
HttpSession::~HttpSession()
{
    // 打印日志
    // cout << "call HttpSeesion destructor" << endl;
}

/* 处理消息的方式
 * 接收到消息时，HTTP层的处理工作: 主要负责解析HTTP请求报文并做出响应 
 * 注意: 此函数在IO线程中运行 */
void HttpSession::handleMessage(std::string &s)
{
    string str;
    str.swap(s);
    // 用于防止在执行该函数期间HttpSession资源块被释放。
    std::shared_ptr<HttpSession> guard = shared_from_this();

    // 若当前Http会话关闭，则将接收到的信息丢弃并返回
    if (!connected_) 
    {
        std::cout << "Http session closed!" << std::endl;
        return;
    }

    if (str.empty()) 
    {   // 下层TCP连接接收到FIN(只从TCP缓冲区中提取单独的文件结束符)
        if (mapHttpProcessContext_.empty()) 
        {
            connected_ = false;// 关闭HTTP层
            SP_TcpConnection spTcpConn = wpTcpConn_.lock();
            if (spTcpConn) 
            {
                // HTTP层工作已完成，TCP层可在发送完数据后关闭。
                spTcpConn->notifyHttpClosed();
                spTcpConn->handleClose();
            }
            else
            {
                cout << "handleMessage: TcpConnection has closed!" << endl;
            }
        }
        return;
    }
    
    // 把数据加入到HTTP层的缓冲区
    if (recvMsg_.empty())
        recvMsg_.swap(str);
    else
        recvMsg_.append(str);
        
    if (!spCurrPro_) 
        crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");

    // 注意：若当前spCurrPro_非空并且crlfcrlfPos_==std::string::npos
    //       则将recvMsg_中至多remain_个字节的数据附加到spCurrPro指
    //       向的HttpProcessContext
    while (connected_ && recvMsg_.size() > 0 && (crlfcrlfPos_ != string::npos || spCurrPro_))
    {
        if (parseRcvMsg() != 0)// 分离报文出错
        {
            std::cout << "pareRcvMsg error!" << std::endl; 
            if(spCurrPro_) 
            {
                // 将差错响应报文压入准备响应队列
                prepareContext_.push(spCurrPro_->id);
                addToBuf();// 向客户端发送报文 
                crlfcrlfPos_ = string::npos;
                spCurrPro_.reset();// 释放资源
            }
            else
            {
                ;
            }
            break;
        }

        if (spCurrPro_ && completed_) // 接收到完整报文
        {
            // 推进线程或者在当前线程工作
            if (pThreadPool->getThreadNum() > 0) 
            {
                // 若线程池任务数满，则在当前IO线程进行处理
                if (-1 == pThreadPool->addTask(std::bind(&HttpSession::handleMessageTask, shared_from_this(), spCurrPro_)))
                {
                    cout << "the threadpool task is full!" << endl;
                    handleMessageTask(spCurrPro_);
                }
            }
            else
            {
                handleMessageTask(spCurrPro_);
            }
            crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");// 重置
            spCurrPro_.reset();
            completed_ = false;
        }
        else if (spCurrPro_ && !completed_)// 接收到POST报文，且不完整
        {
            crlfcrlfPos_ = string::npos;
        }
        else
        {
            // 出现错误(不可能执行到此处)
            cout << "handle message error-- parseRcvMsg_() error" << endl;
            recvMsg_.clear();
            return;
        }
    }
}

/* 分离从TCP层传来的数据：每次最多分离出一个报文
 * 只有在当前接收缓冲区recvMsg_中找到“\r\n\r\n”或当前正在处理
 * 不完整的POST报文才有可能调用parseRcvMsg()
 * */
int HttpSession::parseRcvMsg()
{
    if (!spCurrPro_)// 新报文
    {
        // 新建一个处理报文对象，当前crlfcrlfPos_ != std::string::npos
        ++requestCount_;
        SP_HttpProcessContext spProcessContext = std::make_shared<HttpProcessContext>();
        mapHttpProcessContext_[requestCount_] = spProcessContext;
        spCurrPro_ = spProcessContext;
        spCurrPro_->id = requestCount_;

        // 分离命令
        size_type methodPos = recvMsg_.find(string(" "));
        std::string method = recvMsg_.substr(0, methodPos);

        if (method == "GET")
            currMethod_ = GET;
        else if (method == "POST")
            currMethod_ = POST;
        else
            currMethod_ = OTHER;

        // 目前只支持GET和POST请求
        if (currMethod_ == OTHER)
        {
            std::cout << "HttpSession::parseRcvMsg--don't support method " 
                      << method << std::endl;
            httpError(501, "Method Not Implemented", spCurrPro_);
            return -1;
        }
    
        // 若请求报文为GET(GET请求报文肯定完整)
        if (currMethod_ == GET)
        {
            if (crlfcrlfPos_+4 == recvMsg_.size()) // 若缓冲区刚好是一个请求报文
            {
                spCurrPro_->requestContext.swap(recvMsg_);
                completed_ = true;
            } 
            else // 当前缓冲区有多于一个报文
            {
                string str = recvMsg_.substr(0, crlfcrlfPos_ + 4);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4);
                spCurrPro_->requestContext.swap(str);
                completed_ = true;
            }
            return 0;
        }

        // 当前请求报文为POST类型
        if (currMethod_ == POST)
        {
            size_type conLen_pos= recvMsg_.find(string("Content-Length"));
            if (conLen_pos == std::string::npos)
            {
                cout << "HttpSession::parseRecvMsg--POST request context without Content-Length" 
                     << endl;
                httpError(400, "Bad request!", spCurrPro_);
                return -1;
            }

            // 查找Content-Length的结束尾部
            size_type crlf_pos = recvMsg_.find(string("\r\n"), conLen_pos);// 从conLen_pos位置起寻找第一个“\r\n”
            string bodyLengthStr = recvMsg_.substr(conLen_pos+16, crlf_pos-conLen_pos-16);
            // 将bodyLength转换为数值
            int bodyLength = 0;
            try 
            {
                bodyLength = stoi(bodyLengthStr);
            } 
            catch (logic_error &e) 
            {
                // 数值解析错误
                cout << "The value of Content-Length is wrong!" << endl;
                httpError(400, "Bad request: The format of content length is wrong!", spCurrPro_);
                return -1;
            }

            auto resDataNum = recvMsg_.size() - (crlfcrlfPos_+4);

            if (bodyLength <= resDataNum)// 若缓冲区中携带完整POST报文
            {
                string str = recvMsg_.substr(0, crlfcrlfPos_ + 4 + bodyLength);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4 + bodyLength);
                spCurrPro_->requestContext.swap(str);
                remain_ = 0;
                completed_ = true;
            } 
            else// 请求报文携带数据不完整
            {
                (spCurrPro_->requestContext).swap(recvMsg_);
                remain_ = bodyLength - resDataNum;
                completed_ = false;
            }
            return 0;
        }
    } 
    else// 当前有正在处理的报文，即上一次报文收取不完整(POST)
    {
        if (remain_ <= recvMsg_.size()) 
        {
            string str = recvMsg_.substr(0, remain_);
            (spCurrPro_->requestContext).append(str);
            recvMsg_ = recvMsg_.substr(remain_);
            completed_ = true;
            remain_ = 0;
        } 
        else
        { 
            (spCurrPro_->requestContext).append(recvMsg_);
            remain_-=recvMsg_.size();
            recvMsg_.clear();
        }
        return 0;
    }
}

// 报文处理任务(可在IO线程或工作线程中运行)
void HttpSession::handleMessageTask(SP_HttpProcessContext spProContext)
{
    if (-1 != parseHttpRequest(spProContext));// 分解报文
        httpProcess(spProContext);// 处理报文并构建响应报文

    if (loop_->isInLoopThread())// 判断当前线程是在IO线程还是工作线程
    {
        // 报文处理任务在IO线程完成
        prepareContext_.push(spProContext->id);
        keepAlive_ = spProContext->keepAlive;
        addToBuf();
    } 
    else
    {
        // 报文处理任务在工作线程完成，需通知IO线程发送数据
         loop_->addTask(std::bind(&HttpSession::notifyIoThreadDataPrepare, shared_from_this(), spProContext));
    }    
}

// 解析请求行
int HttpSession::parseRequestLine(SP_HttpProcessContext spProContext)
{
    string requestLine;
    string &request = spProContext->requestContext;
    size_type crlf_pos = request.find("\r\n");
    if (crlf_pos == string::npos)
    {
        httpError(400, "Wrong request!", spProContext);
        return -1;
    }
    requestLine = request.substr(0, crlf_pos);
    request = request.substr(crlf_pos+2);
    stringstream sstream(requestLine);
    ParseRequestLineState state = PARSE_METHOD;
    string tmp;
    while (sstream >> tmp) 
    {
	    switch (state) 
        {
            case PARSE_METHOD:
            {
                spProContext->method = tmp;
                state = PARSE_URI;
                break;
            }
            case PARSE_URI:
            {
	            spProContext->url = tmp;
                state = PARSE_VERSION;
                break;
            }
            case PARSE_VERSION:
            {
                spProContext->version = tmp;
                state = PARSE_SUCCESS;
                break;
            }
            case PARSE_SUCCESS:
            {
                state = PARSE_ERROR;
                httpError(400, "Wrong request!", spProContext);
                return -1;
            }
        }
    }
    if (state != PARSE_SUCCESS) 
    {
        httpError(400, "Wrong request!", spProContext);
        return -1;
    }
    else
        return 0;
}

// 解析请求首部
int HttpSession::parseRequestHeader(SP_HttpProcessContext spProContext)
{
    string requestHeader;
    string &request = spProContext->requestContext;
    unordered_map<string, string> &header = spProContext->header;
    size_type start = 0;
    size_type pos;
    string key;
    string value;
    ParseRequestHeaderState state = H_KEY;
    while (state != H_END) 
    {
        switch (state) 
        {
            case H_KEY:
            {
                pos = request.find(":", start);
                if (pos != string::npos) 
                {
                    key = request.substr(start, pos-start);
                    ++pos;
                    while (pos < request.size() && request[pos] == ' ') ++pos;
                    start = pos;
                    state = H_VALUE;
                }
                else
                {
                    state = H_ERROR;
                }
                break;
            }
            case H_VALUE:
            {
                pos = request.find("\r\n", start);
                if (pos != string::npos) 
                {
                    value = request.substr(start, pos-start);
                    header[key] = value;
                    start = pos+2;
                    state = H_SUCCESS;
                }
                else
                {
                    state = H_ERROR;
                }
                break;
            }
            case H_SUCCESS:
            {
                // 剩余字符不足组成"\r\n"
                if (start > request.size()-2) 
                {
                    state = H_ERROR;
                }
                else
                {
                    request[start] == '\r' && request[start+1] == '\n' ? state = H_END : state = H_KEY;
                    start = start+2;
                }
                break;
            }
            case H_ERROR:
            {
                httpError(400, "Wrong request!", spProContext);
                return -1;
            }
        }
    }
    request = request.substr(start);
    return 0;
}

// 解析请求体
int HttpSession::parseRequestBody(SP_HttpProcessContext spProContext)
{
   (spProContext->body).swap(spProContext->requestContext);
   return 0;
}

int HttpSession::parseHttpRequest(SP_HttpProcessContext spProContext)
{
    if (-1 == parseRequestLine(spProContext))
    {
        // 打印出错日志
        return -1;
    }
    if (-1 == parseRequestHeader(spProContext)) 
    {
        // 打印出错日志
        return -1;
    }
    if (-1 == parseRequestBody(spProContext)) 
    {
        // 打印出错日志
        return -1;
    }
    // 解析成功
    return 0;
}

/* 根据分解后的HTTP请求报文，进行处理(构建HTTP响应报文) */
void HttpSession::httpProcess(SP_HttpProcessContext spProContext)
{
    std::string path;
    std::string responseBody;
    std::string queryString;
    std::string &responseContext = spProContext->responseContext;
    size_t pos = spProContext->url.find("?");
    /* 将url中的路径与参数分离 */
    if(pos != std::string::npos)
    {
        path = spProContext->url.substr(0, pos);
        queryString = spProContext->url.substr(pos+1);
    }
    else
    {
        path = spProContext->url;
    }
    
    //keepalive判断处理
    // std::map<std::string, std::string>::const_iterator iter = httpRequestContext_.header.find("Connection");
    auto iter = spProContext->header.find("Connection");
    if(iter != spProContext->header.end())
    {
        spProContext->keepAlive = (iter->second == "Keep-Alive");
    }
    else
    {
        if(spProContext->version == "HTTP/1.1")
        {
            spProContext->keepAlive = true;//HTTP/1.1默认长连接
        }
        else
        {
            spProContext->keepAlive = false;//HTTP/1.0默认短连接
        }            
    }

    if("/" == path)// 路径为根目录，返回主页
    {        
        path = "/index.html";
    }
    else if("/hello" == path)
    {
        //Wenbbench 测试用
        std::string fileType("text/html");
        responseBody = ("hello world");
        responseContext += spProContext->version + " 200 OK\r\n";// 响应报文的响应行
        responseContext += "Server: Chen TongJie's HttpServer/0.1\r\n";// 首部
        responseContext += "Content-Type: " + fileType + "; charset=utf-8\r\n";
        if(iter != spProContext->header.end())// 若请求报文中存在Connection选项
        {
            responseContext += "Connection: " + iter->second + "\r\n";
        }
        responseContext += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";// 存放响应报文的大小
        responseContext += "\r\n";// 首部结束，下面是响应报文的主体
        responseContext.append(responseBody);
        spProContext -> success = true;
        return;
    }
    else
    {
        ;//此处表示请求的资源不存在
    }

    path.insert(0,".");
    FILE* fp = NULL;
    // 此处调用string的成员函数获取string的C风格字符串指针以适应C函数接口
    // “rb”表示以打开一个二进制文件，并以二进制的方式读取
    if((fp = fopen(path.c_str(), "rb")) == NULL)
    {
        //404 NOT FOUND
        httpError(404, "Not Found", spProContext);
        spProContext->success = false;
        return;
    }
    else
    {   
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        // 读取文件中的信息至缓冲区,每次至多读取sizeof(buffer)个字节
        while(fread(buffer, sizeof(buffer), 1, fp) == 1)
        {
            responseBody.append(buffer);
            memset(buffer, 0, sizeof(buffer));
        }
        if(feof(fp))// 若已读到文件尾
        {
            responseBody.append(buffer);
        }        
        else
        {
            std::cout << "error fread" << std::endl;
            httpError(404, "Not Found", spProContext);
            spProContext->success = false;
            fclose(fp);
            return;
        }        	
        fclose(fp);
    }

    std::string fileType("text/html"); //暂时固定为html
    responseContext += spProContext->version + " 200 OK\r\n";
    responseContext += "Server: Chen tongjie's NetServer/0.1\r\n";
    responseContext += "Content-Type: " + fileType + "; charset=utf-8\r\n";

    if(iter != spProContext->header.end())
    {
        responseContext += "Connection: " + iter->second + "\r\n";
    }
    responseContext += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    responseContext += "\r\n";
    responseContext += responseBody;
    // 处理报文成功
    spProContext->success = true;
    return;
}

void HttpSession::notifyIoThreadDataPrepare(SP_HttpProcessContext spProContext)
{
    prepareContext_.push(spProContext->id);
    keepAlive_ = spProContext->keepAlive;
    addToBuf();
}

// 由于只会在IO线程中调用，因此可直接调用TcpConnectino的send操作
void HttpSession::addToBuf()
{
    SP_HttpProcessContext sendContext;
    SP_TcpConnection spTcpConn = wpTcpConn_.lock();
    std::string tmp;
    while (prepareContext_.top() == sendId_) 
    {
        if (connected_) 
        {
            sendContext = mapHttpProcessContext_[sendId_];
            tmp.append(sendContext->responseContext);
            // http 1.0报文或者HTTP层解析出错会导致HTTP层关闭
            if (!keepAlive() || !sendContext->success) 
            {
                halfClose_ = true;
                connected_ = false;
            }
        }
        mapHttpProcessContext_.erase(sendId_);
        prepareContext_.pop();
        ++sendId_;
    }

    // 若当前连接为半关闭，并且处理待处理报文为空，则可关闭连接
    if (halfClose_ && mapHttpProcessContext_.empty())
        connected_ = false;

    if (!tmp.empty() && spTcpConn) 
    {
        if (!connected_) 
            spTcpConn->notifyHttpClosed();
        spTcpConn->bufferOut_.append(sendContext->responseContext);
        spTcpConn->send();
    }
}

/* 发送完成时HTTP层的处理工作
 * 打印日志的操作交由主线程进行 */
void HttpSession::handleSendComplete()
{
    // 打印发送完成日志
}

/* 发生在TCP服务器强制关闭连接
 * HTTP层连接关闭时主要进行资源释放操作
 * */
void HttpSession::handleClose()
{
    connected_ = false;
}

// 发生在TCP连接接收到FIN分节
void HttpSession::handleHalfClose()
{
    halfClose_ = true;
}

/* 底层TCP连接发生错误时，HTTP层处理工作 
 * 关闭当前HTTP层会话 */
void HttpSession::handleError()
{
    connected_ = false;
}

// 报文处理出错所调用的函数
void HttpSession::httpError(int err_num, std::string short_msg, SP_HttpProcessContext spProContext)
{
    //这里string创建销毁应该会耗时间
    //std::string body_buff;
    //这里string创建销毁应该会耗时间
    spProContext->success = false;
    std::string responseBody;
    std::string &responseContext = spProContext->responseContext;

    responseBody += "<html><title>出错了</title>";
    responseBody += "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head>";
    responseBody += "<style>body{background-color:#f;font-size:14px;}h1{font-size:60px;color:#eeetext-align:center;padding-top:30px;font-weight:normal;}</style>";
    responseBody += "<body bgcolor=\"ffffff\"><h1>";
    responseBody += std::to_string(err_num) + " " + short_msg;
    responseBody += "</h1><hr><em> Chen tongjie's NetServer</em>\n</body></html>";

    responseContext += spProContext->version + " " + std::to_string(err_num) + " " + short_msg + "\r\n";
    responseContext += "Server: Chen tongjie's NetServer/0.1\r\n";
    responseContext += "Content-Type: text/html\r\n";
    responseContext += "Connection: Keep-Alive\r\n";
    responseContext += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    responseContext += "\r\n";
    responseContext += responseBody;
}
