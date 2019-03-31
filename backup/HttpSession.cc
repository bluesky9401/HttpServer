
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

// 200：请求被正常处理 
// 204：请求被受理但没有资源可以返回 
// 206：客户端只是请求资源的一部分，服务器只对请求的部分资源执行GET方法，相应报文中通过Content-Range指定范围的资源。

// 301：永久性重定向 
// 302：临时重定向 
// 303：与302状态码有相似功能，只是它希望客户端在请求一个URI的时候，能通过GET方法重定向到另一个URI上 
// 304：发送附带条件的请求时，条件不满足时返回，与重定向无关 
// 307：临时重定向，与302类似，只是强制要求使用POST方法
 
// 400：请求报文语法有误，服务器无法识别
// 401：请求需要认证 
// 403：请求的对应资源禁止被访问 
// 404：服务器无法找到对应资源 

// 500：服务器内部错误 
// 503：服务器正忙

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include "HttpSession.h"
#include "TcpConnection.h"

HttpSession::HttpSession(SP_TcpConnection spTcpConn)
    : setHttpProcessContext_(),
      spCurrPro_(),
      currMethod_(),
      completed_(false),
      remain_(0),
      crlfcrlfPos_(std::string::npos),
      keepAlive_(false),
      connected_(true),
      taskNum_(0),
      spTcpConn_(spTcpConn)
{

}

HttpSession::~HttpSession()
{
    std::cout << "free HttpSession!" << std::endl;
}

/* 处理消息的方式
 * 接收到消息时，HTTP层的处理工作: 主要负责解析HTTP请求报文并做出响应 
 * 注意: 此函数在IO线程中运行 */
void HttpSession::handleMessage(std::string &s)
{
    std::string str;
    // 首先将TcpConnection上的输入缓冲区内容交换到HttpSession的输入缓冲区
    str.swap(s);
    if (recvMsg_.empty())
        recvMsg_.swap(str);
    else
        recvMsg_ += str;

    if (!spCurrPro_) // 若当前没有正在处理的报文
        crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");

    while ((crlfcrlfPos_ != std::string::npos || spCurrPro_)
            && recvMsg_.size() > 0)
    {
        if (parseRcvMsg() != 0)// 分离报文出错
        {
            if(spCurrPro_) 
            {
                crlfcrlfPos_ = std::string::npos;
                addToBuf(spCurrPro_);
               // handleError();// 关闭连接
                break;
            }
        }
        else
        {
           // handleError();
        }

        if (spCurrPro_ && completed_) // 接收到完整报文
        {
            // 推进线程或者在当前线程工作
            if (pThreadPool->getThreadNum() > 0) 
            {
                pThreadPool->addTask(std::bind(&HttpSession::handleMessageTask, this, spCurrPro_));
                ++ taskNum_;
            }
            else
            {
                handleMessageTask(spCurrPro_);
            }
            
            crlfcrlfPos_ = recvMsg_.find("\r\n\r\n");
            spCurrPro_.reset();
            completed_ = false;
        }
        else if (spCurrPro_ && !completed_)// 接收到POST报文，且不完整
        {
            crlfcrlfPos_ = std::string::npos;
            break;
        }
        else
        {
            // 出现错误
            std::cout << "handle message error-- parseRcvMsg_() error" << std::endl;
            return;
        }
    }
}

/* 分解从TCP层传来的数据 
 * 每次最多分离出一个报文 */
int HttpSession::parseRcvMsg()
{
    if (!spCurrPro_)// 新报文
    {
        // 新建一个请求--响应报文映射
        SP_HttpProcessContext spProcessContext = std::make_shared<HttpProcessContext>();
        setHttpProcessContext_.insert(spProcessContext);

        spCurrPro_ = spProcessContext;
        
        // 接收到的缓存数据中存在"\r\n\r\n"字符串
        size_t methodPos = recvMsg_.find(" ");
        std::string method = recvMsg_.substr(0, methodPos);
        currMethod_ = method;
        
        // 目前只支持GET和POST请求
        if (method != std::string("GET") && method != std::string("POST"))
        {
            std::cout << "HttpSession::handleMessage" << std::endl;
            httpError(501, "Method Not Implemented", spCurrPro_);
            return -1;
        }

        // 若请求报文为GET(GET请求报文肯定完整)
        if (method == std::string("GET"))
        {
            if (crlfcrlfPos_+4 == recvMsg_.size()) // 若缓冲区刚好是一个请求报文
            {
                spCurrPro_->requestContext.swap(recvMsg_);
                completed_ = true;
                return 0;
            } 
            else // 当前缓冲区有多于一个报文
            {
                std::string str = recvMsg_.substr(0, crlfcrlfPos_ + 4);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4);
                spCurrPro_->requestContext.swap(str);
                completed_ = true;
                return 0;
            }
        }

        // 当前请求报文为POST类型
        if (method == std::string("POST"))
        {
            size_t conLen_pos= recvMsg_.find("Content-Length");
            if ( conLen_pos == std::string::npos)
            {
                std::cout << "HttpSession::parseRecvMsg" << std::endl;
                httpError(501, "Don't support chunked mode", spCurrPro_);
                return -1;
            }
            // 查找Content-Length的结束尾部
            size_t crlf_pos = recvMsg_.find("\r\n", conLen_pos);
            std::string bodyLengthStr = recvMsg_.substr(conLen_pos+15, crlf_pos-conLen_pos-15);
            // 将bodyLength转换为数值
            auto bodyLength = stoll(bodyLengthStr);

            auto resDataNum = recvMsg_.size() - (crlfcrlfPos_+4);

            if (bodyLength <= resDataNum)// 若请求报文携带数据完整
            {
                std::string str = recvMsg_.substr(0, crlfcrlfPos_ + 4 + bodyLength);
                recvMsg_ = recvMsg_.substr(crlfcrlfPos_ + 4 + bodyLength);
                spCurrPro_->requestContext.swap(str);
                completed_ = true;
                return 0;
            } 
            else// 请求报文携带数据不完整
            {
                spCurrPro_->requestContext.swap(recvMsg_);
                remain_ = bodyLength - resDataNum;
                completed_ = false;
                return 0;
            }
        }
    } 
    else// 当前有正在处理的报文，即上一次报文收取不完整(POST)
    {
        if (remain_ <= recvMsg_.size()) 
        {
            std::string str = recvMsg_.substr(0, remain_);
            spCurrPro_->requestContext += str;
            recvMsg_ = recvMsg_.substr(remain_);
            completed_ = true;
            remain_ = 0;
        } 
        else
        { 
            spCurrPro_->requestContext += recvMsg_;
            recvMsg_.clear();
            remain_-=recvMsg_.size();
        }
        return 0;
    }
}

void HttpSession::handleMessageTask(SP_HttpProcessContext spProContext)
{
    if(spProContext->requestContext.empty()) 
        return;
    parseHttpRequest(spProContext);
    httpProcess(spProContext);    
    spTcpConn_->loop_->addTask(std::bind(&HttpSession::addToBuf, this, spProContext));
    
  //  if(!keepAlive())
  //  {
        //短连接，可以告诉框架层数据发完就可以关掉TCP连接，不过这里注释掉，还是交给客户端主动关闭吧
        //sptcpConn_->handleClose();
  //  }        
}


/* 发送完成时HTTP层的处理工作
 * 打印日志的操作交由主线程进行 */
void HttpSession::handleSendComplete()
{
    // 打印发送完成日志
}

/* 连接关闭时HTTP层的处理工作 
 * HTTP层连接关闭时主要进行资源释放操作
 * */
void HttpSession::handleClose()
{
    if (taskNum_ == 0 && connected_) 
    {
        spTcpConn_.reset(); // 为释放Tcp连接做准备
    }
    connected_ = false;
}

/* 连接发生错误时，HTTP层处理工作 
 * 与连接关闭时的处理工作一致，进行资源释放 */
void HttpSession::handleError()
{
    if (taskNum_ == 0) 
    {
        spTcpConn_.reset(); // 为释放Tcp连接做准备
    }
    connected_ = false;
}

void HttpSession::parseHttpRequest(SP_HttpProcessContext spProContext)
{
    std::string msg;// 由于parseHttpRequest有可能在工作线程中执行，因此
    msg.swap(spProContext->requestContext);
	std::string crlf("\r\n"), crlfcrlf("\r\n\r\n");
	size_t prev = 0, next = 0, posColon;
	std::string key, value;

    // 以下解析可以改成状态机，解决一次收Http报文不完整问题
	// parse http request line
	if ((next = msg.find(crlf, prev)) != std::string::npos) // std::string::npos用来表示一个不存在的位置，
    {                                                       // 用于判断字符串中不存在查找子串
		std::string firstLine(msg.substr(prev, next - prev));
		prev = next;
		std::stringstream sstream(firstLine);
		sstream >> (spProContext -> method);
		sstream >> (spProContext -> url);
		sstream >> (spProContext -> version);
	}
	else
	{
        std::cout << "msg" << msg << std::endl;
		std::cout << "Error in httpParser: http_request_line isn't complete!" << std::endl;
        //可以临时存起来，凑齐了再解析
	}

    // parse http request header
	size_t pos_crlfcrlf = 0;
	if (( pos_crlfcrlf = msg.find(crlfcrlf, prev)) != std::string::npos)
	{
		while (prev != pos_crlfcrlf)
        {
            next = msg.find(crlf, prev + 2);// prev表示/r/n的/r的位置
            posColon = msg.find(":", prev + 2); 
            key = msg.substr(prev + 2, posColon - prev-2);// 提取Content_length:size中的Content_length
            value = msg.substr(posColon + 2, next-posColon-2);
            prev = next;
            spProContext->header.insert(std::pair<std::string, std::string>(key, value));
        }
	}
    else
    {
        std::cout << "Error in httpParser: http_request_header isn't complete!" << std::endl;
    }

    // parse http request body
	spProContext->body = msg.substr(pos_crlfcrlf + 4);
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
        keepAlive_ = (iter->second == "Keep-Alive");
    }
    else
    {
        if(spProContext->version == "HTTP/1.1")
        {
            keepAlive_ = true;//HTTP/1.1默认长连接
        }
        else
        {
            keepAlive_ = false;//HTTP/1.0默认短连接
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
        responseContext += responseBody;
        return;
    }
    else
    {
        ;//此处表示请求的资源不存在
    }

    //std::string responsebody;    
    path.insert(0,".");
    FILE* fp = NULL;
    // 此处调用string的成员函数获取string的C风格字符串指针以适应C函数接口
    // “rb”表示以打开一个二进制文件，并以二进制的方式读取
    if((fp = fopen(path.c_str(), "rb")) == NULL)
    {
        //perror("error fopen");
        //404 NOT FOUND
        /* 此处要改为推进任务队列 */
        // httpError(404, "Not Found");
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
    return;
}

void HttpSession::addToBuf(SP_HttpProcessContext spProContext)
{
    // std::cout << spProContext->requestContext << std::endl;
    if (connected_)
    {
        --taskNum_;
        spTcpConn_->bufferOut_ += spProContext->responseContext;
        setHttpProcessContext_.erase(spProContext);// 删除该报文
        spTcpConn_->send();// 发送处理好的请求报文
    }
    else
    {
        --taskNum_;
        setHttpProcessContext_.erase(spProContext);
        if (taskNum_ == 0 && setHttpProcessContext_.empty())
            spTcpConn_.reset();// 开始释放连接
    }
}

// 报文处理出错所调用的函数
void HttpSession::httpError(int err_num, std::string short_msg, SP_HttpProcessContext spProContext)
{
    //这里string创建销毁应该会耗时间
    //std::string body_buff;
    //这里string创建销毁应该会耗时间
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
    
    connected_ = false;// 标记HTTP层的连接关闭，不再处理任务
}
