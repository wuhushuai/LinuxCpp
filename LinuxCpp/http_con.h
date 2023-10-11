#pragma once
#include <sys/epoll.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "locker.h"

#include <regex>
class http_con {
public:
    // HTTP请求方法，这里只支持GET
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };

    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
public:
	http_con() {};
	~http_con() {};
	void init(int confd ,const sockaddr_in& sock);
    void init();
	void close();
	bool read();
    bool write();

    void process();
    HTTP_CODE process_read();
    //1.解析请求首行  2.解析请求头  3.解析请求体
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_head(char* text);
    HTTP_CODE parse_body(char* text);
    //解析一行，然后再交给上面某个函数
    LINE_STATUS parse_line();//其实就是获取一行的数据(并没有实际获取，返回的是成功或者失败【针对的是能不能获取数据】)，根据\r\n找一行数据，一旦获取到一行就返回，最后m_checked_index会在一行的开始位置【所以这个函数主要是更新m_check_index和设置00字符串结尾以及分析报错情况】
    inline char* get_line();//获取一行内容，根据m_start_index来，一开始是0//然后由于parse_line解析完一行后会在下一行首，所以再复制给m_satart_index,那么又可以获取一行//获取一行肯定是根据00结尾，刚好再分析完了之后会添加00结尾。就可以获取一行了。
    HTTP_CODE do_request();//去做具体的处理

    
public:
	static int http_epollfd;
	static int http_con_num;
	static const int http_read_max = 2048;
	static const int http_wirte_max = 2048;

	int m_read_idx; //指向缓冲区 m_read_buf 的数据末尾的下一个字节
	char m_read_buf[http_read_max]; 
	char m_write_buf[http_wirte_max];
    int m_checked_index; //当前正在分析的字符在读缓冲区的位置
    int m_start_line;//当前解析行的起始位置//m_read_buf + m_start_line就是解析位置

    CHECK_STATE m_check_status;//主状态机当前所处状态
public:
    std::string m_url;         //目标访问的文件名
    std::string m_Version;    //协议版本
    std::string m_Host;      //主机名
    bool m_Linger;          //HTTP请求是否保持连接
    METHOD m_method;       //请求方法
public:
	int m_confd;
	sockaddr_in m_sockaddrin;
};
