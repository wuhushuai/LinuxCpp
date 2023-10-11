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
    // HTTP���󷽷�������ֻ֧��GET
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

    /*
        �����ͻ�������ʱ����״̬����״̬
        CHECK_STATE_REQUESTLINE:��ǰ���ڷ���������
        CHECK_STATE_HEADER:��ǰ���ڷ���ͷ���ֶ�
        CHECK_STATE_CONTENT:��ǰ���ڽ���������
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };

    /*
        ����������HTTP����Ŀ��ܽ�������Ľ����Ľ��
        NO_REQUEST          :   ������������Ҫ������ȡ�ͻ�����
        GET_REQUEST         :   ��ʾ�����һ����ɵĿͻ�����
        BAD_REQUEST         :   ��ʾ�ͻ������﷨����
        NO_RESOURCE         :   ��ʾ������û����Դ
        FORBIDDEN_REQUEST   :   ��ʾ�ͻ�����Դû���㹻�ķ���Ȩ��
        FILE_REQUEST        :   �ļ�����,��ȡ�ļ��ɹ�
        INTERNAL_ERROR      :   ��ʾ�������ڲ�����
        CLOSED_CONNECTION   :   ��ʾ�ͻ����Ѿ��ر�������
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

    // ��״̬�������ֿ���״̬�����еĶ�ȡ״̬���ֱ��ʾ
    // 1.��ȡ��һ���������� 2.�г��� 3.���������Ҳ�����
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
    //1.������������  2.��������ͷ  3.����������
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_head(char* text);
    HTTP_CODE parse_body(char* text);
    //����һ�У�Ȼ���ٽ�������ĳ������
    LINE_STATUS parse_line();//��ʵ���ǻ�ȡһ�е�����(��û��ʵ�ʻ�ȡ�����ص��ǳɹ�����ʧ�ܡ���Ե����ܲ��ܻ�ȡ���ݡ�)������\r\n��һ�����ݣ�һ����ȡ��һ�оͷ��أ����m_checked_index����һ�еĿ�ʼλ�á��������������Ҫ�Ǹ���m_check_index������00�ַ�����β�Լ��������������
    inline char* get_line();//��ȡһ�����ݣ�����m_start_index����һ��ʼ��0//Ȼ������parse_line������һ�к������һ���ף������ٸ��Ƹ�m_satart_index,��ô�ֿ��Ի�ȡһ��//��ȡһ�п϶��Ǹ���00��β���պ��ٷ�������֮������00��β���Ϳ��Ի�ȡһ���ˡ�
    HTTP_CODE do_request();//ȥ������Ĵ���

    
public:
	static int http_epollfd;
	static int http_con_num;
	static const int http_read_max = 2048;
	static const int http_wirte_max = 2048;

	int m_read_idx; //ָ�򻺳��� m_read_buf ������ĩβ����һ���ֽ�
	char m_read_buf[http_read_max]; 
	char m_write_buf[http_wirte_max];
    int m_checked_index; //��ǰ���ڷ������ַ��ڶ���������λ��
    int m_start_line;//��ǰ�����е���ʼλ��//m_read_buf + m_start_line���ǽ���λ��

    CHECK_STATE m_check_status;//��״̬����ǰ����״̬
public:
    std::string m_url;         //Ŀ����ʵ��ļ���
    std::string m_Version;    //Э��汾
    std::string m_Host;      //������
    bool m_Linger;          //HTTP�����Ƿ񱣳�����
    METHOD m_method;       //���󷽷�
public:
	int m_confd;
	sockaddr_in m_sockaddrin;
};
