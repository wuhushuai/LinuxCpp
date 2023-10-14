#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <regex>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "locker.h"

#include <stdarg.h>
#include <sys/uio.h>
#define PATH_MAX 100
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
	void close_conn();
	bool read();
    bool write();

    void process();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    //1.������������  2.��������ͷ  3.����������
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_head(char* text);
    HTTP_CODE parse_body(char* text);
    //����һ�У�Ȼ���ٽ�������ĳ������
    LINE_STATUS parse_line();//��ʵ���ǻ�ȡһ�е�����(��û��ʵ�ʻ�ȡ�����ص��ǳɹ�����ʧ�ܡ���Ե����ܲ��ܻ�ȡ���ݡ�)������\r\n��һ�����ݣ�һ����ȡ��һ�оͷ��أ����m_checked_index����һ�еĿ�ʼλ�á��������������Ҫ�Ǹ���m_check_index������00�ַ�����β�Լ��������������
    inline char* get_line();//��ȡһ�����ݣ�����m_start_index����һ��ʼ��0//Ȼ������parse_line������һ�к������һ���ף������ٸ��Ƹ�m_satart_index,��ô�ֿ��Ի�ȡһ��//��ȡһ�п϶��Ǹ���00��β���պ��ٷ�������֮�������00��β���Ϳ��Ի�ȡһ���ˡ�
    HTTP_CODE do_request();//ȥ������Ĵ���

	// ���ڴ�ӳ����ִ��munmap����
    void unmap(); //
public:
    inline bool add_status_line(int status, const char* title);
    bool add_response(const char* format, ...);
    bool add_content_length(int content_len);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
    bool add_headers(int content_len);

    bool add_content(const char* content);
public:
	static int http_epollfd;
	static int http_con_num;
	static const int http_read_max = 2048;
	static const int http_wirte_max = 2048;
public:
	int m_read_idx;    //ָ�򻺳��� m_read_buf ������ĩβ����һ���ֽ�
    int m_write_idx;  // д�������д����͵��ֽ���
	char m_read_buf[http_read_max]; 
	char m_write_buf[http_wirte_max];
    int m_checked_index; //��ǰ���ڷ������ַ��ڶ���������λ��
    int m_start_line;//��ǰ�����е���ʼλ��//m_read_buf + m_start_line���ǽ���λ��

    CHECK_STATE m_check_status;//��״̬����ǰ����״̬
public:
    std::string m_url;                 //HTTPĿ����ʵ��ļ���
    std::string m_Version;            //Э��汾
    std::string m_Host;              //������
    bool m_Linger;                  //HTTP�����Ƿ񱣳�����
    METHOD m_method;               //���󷽷�
    int m_content_length;         //�����峤��
    char m_path[PATH_MAX];       //�ͻ������Ŀ���ļ�������·��

    char* m_file_address; //�ļ����ڴ�ָ��
    struct stat m_file_stat;   // Ŀ���ļ���״̬��ͨ�������ǿ����ж��ļ��Ƿ���ڡ��Ƿ�ΪĿ¼���Ƿ�ɶ�������ȡ�ļ���С����Ϣ
    struct iovec m_iv[2];       // ���ǽ�����writev��ִ��д��������Ҫ���� readv �� writev ϵͳ���á������Զ�������������Ա������m_iv_count��ʾ��д�ڴ���������
	//struct iovec {
		//void* iov_base;  /* ָ��һ���ڴ�����buffer���Ŀ�ʼλ�� */
		//size_t iov_len;   /* �ڴ�����ĳ��� */
	//};
    int m_iv_count;

	int bytes_to_send;              // ��Ҫ���͵����ݵ��ֽ���
	int bytes_have_send;            // �Ѿ����͵��ֽ���
public:
	int m_confd;
	sockaddr_in m_sockaddrin;
};