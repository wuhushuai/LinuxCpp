#include "http_con.h"

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);// File Control
	fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
	return old_option;
}

void addfd(int epollfd, int fd, bool one_shot,bool flag = true )
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLRDHUP;//ͬʱ�������¼��ͶԶ˹ر��¼� HUP:����
	//���ԾͲ����ж�recv�ķ���ֵ��֪�����Ƿ�رգ����Ǽ���ĳ���¼�
	//����EPOLLRDHUP�Ļ��� ֮ǰ�������Ҫͨ������recv�����ҷ���Ϊ0�����жϣ�������ֵΪ0˵���Զ��Ѿ��ر���socket����ʱ����ֱ�ӵ���close�رձ��˵�socket���ɡ�
	if (flag) {
		event.events |= EPOLLET;  //���˼�|
	}
	if (one_shot)
	{
		// ��ֹͬһ��ͨ�ű���ͬ���̴߳��� Oneshot
		event.events |= EPOLLONESHOT;
		//����һ���߳����ڶ�ȡĳ��socket�ϵ����ݣ����ڴ����ʱ���������µ����ݿɶ��ͻᱻ��һ���߳� �������Ǿ����������̲߳���һ��socket�ĳ���
		//EPOLLONESHOT����ʱһ��socketֻ�ܱ�һ���̴߳���
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	// �����ļ�������������
	setnonblocking(fd);
}

void modfd(int epollfd,int fd,int ev) {//ev �ĳɶ�����д
	epoll_event epollev;
	epollev.events = ev | EPOLLRDHUP | EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epollev);
}

void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void http_con::process()
{
	 process_read();



	printf("process!\n");
}

http_con::HTTP_CODE http_con::process_read()
{
	//��ʼ��һ��
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;
	
	while (((m_check_status == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
		|| ((line_status = parse_line()) == LINE_OK)) {
		// ��Ҫ��ȡһ������
		
		text = get_line(); //

		m_start_line = m_checked_index;
		//printf("got 1 http line: %s\n", text);

		//��ȡһ������line_status = parse_line()��������״̬����״̬����ͬ�Ĵ���
		switch (m_check_status) {
		case CHECK_STATE_REQUESTLINE: {
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER: {
			ret = parse_head(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			else if (ret == GET_REQUEST) {
				return do_request();
			}
			break;
		}
		case CHECK_STATE_CONTENT: {
			ret = parse_body(text);
			if (ret == GET_REQUEST) {
				return do_request();
			}
			line_status = LINE_OPEN;
			break;
		}
		default: {
			return INTERNAL_ERROR;
		}
		}
	}
	return NO_REQUEST;
}

//����HTPP�����ļ�����HTTP�汾
http_con::HTTP_CODE http_con::parse_request_line(char* text)
{
	std::regex request_line_regex(R"(^([^ ]*) ([^ ]*) HTTP/([^ ]*)$)");
	std::cmatch match;
	if (std::regex_search(m_read_buf, match, request_line_regex)) {
		std::map<std::string, METHOD> strToMethod = {
				{"GET", GET},
				{"POST", POST},
				{"HEAD", HEAD},
				{"PUT", PUT},
				{"DELETE", DELETE},
				{"TRACE", TRACE},
				{"OPTIONS", OPTIONS},
				{"CONNECT", CONNECT}
		};
		auto it = strToMethod.find(match[1]);
		if (it != strToMethod.end()) {
			m_method = it->second;
		}
		else {
			return NO_REQUEST;
		}
		
		m_url = match[2];
		m_Version = match[3];
		printf("Method: %s\n", it->first.c_str());
		printf("Path: %s\n", m_url.c_str());
		printf("HTTP Version: %s\n", m_Version.c_str());
		if (m_method != GET) {
			return BAD_REQUEST;
		}
		else if (m_Version != "1.1") {
			return BAD_REQUEST;
		}
		else if (m_url[0] != '/') {
			return BAD_REQUEST;
		}

		m_check_status = CHECK_STATE_HEADER;//�������������п�ʼ��������ͷ

	}

	return NO_REQUEST;
}

http_con::HTTP_CODE http_con::parse_head(char* text)
{
	std::regex header_regex(R"(([A-Z]+[a-z]+.*): (.*))");
	std::cmatch match;
	if (std::regex_search(text, match, header_regex)) {
		if (match[1] == "Host") {
			m_Host = match[2].str().substr(0, match[2].str().find(':'));
			printf("Header Name: %s\n", "Host");
			printf("Header Value: %s\n", m_Host.c_str());
			//return GET_REQUEST;  û�������أ�returnɶ
		}
		else if (match[1] == "Connection") {
			if (match[2] == "keep-alive") {
				m_Linger == 1;
			}
			else
			{
				m_Linger == 0;
			}
		}
		
		printf("Header Name: %s\n", match[1].str().c_str());
		printf("Header Value: %s\n", match[2].str().c_str());
	}
	else {  //���жϵ��ⲽ˵������ƥ�䲻�ϣ���˵���������������
		if (m_method == POST) { //�����post�������Ҫ����������
			m_check_status = CHECK_STATE_CONTENT;
			return NO_REQUEST;//Ӧ�÷���NO_REQUEST��˵������Ҫ����������
		}
		return GET_REQUEST;//˵����GET�Ǿ�û����������
		
	}
	return NO_REQUEST;
}

http_con::HTTP_CODE http_con::parse_body(char* text)
{
	printf("����������!\n");
	return NO_REQUEST;
}

http_con::LINE_STATUS http_con::parse_line(){

	char temp;
	//m_checked_index�����ڽ�����λ��
	//m_read_idx�ǵ���readʱ���index����¼����m_read_bufԭʼbuf�����һ���ֽ�+1
	//Ϊʲô�����һ���ֽڼ�1�����Index��������Ϊ��һ��һ�ν�������ȷ������������index
	//printf("m_checked_index = %d\n", m_checked_index);
	//printf("m_read_idx = %d\n", m_read_idx);
	for (; m_checked_index < m_read_idx; ++m_checked_index) {
		temp = m_read_buf[m_checked_index];//��ǰ�����ַ�
		if (temp == '\r') {
			if ((m_checked_index + 1) == m_read_idx) {//����Ӧ���Ǳ�forѭ���������պõ�read_buf��β�����������﷢��+1�͵�β���ˣ�˵������ǰ�еĽ�����־\r\n��û����ȫ�����ڻ�������\r����һ��Ӧ����\n
				return LINE_OPEN;//�����ݲ�����
			}
			else if (m_read_buf[m_checked_index + 1] == '\n') {//��\r\n�滻��\0 \0
				m_read_buf[m_checked_index++] = '\0';
				m_read_buf[m_checked_index++] = '\0';//\0�Ǳ�ʾ�ַ��������Ŀ��ַ�
				return LINE_OK;
			}
			return LINE_BAD;//�����������������г���
		}
		else if (temp == '\n') {//��ǰ�����ַ������\n
			if ((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r')) {
				m_read_buf[m_checked_index - 1] = '\0';
				m_read_buf[m_checked_index++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
// 	bool flag = true;
// 	if (m_read_buf != NULL) {
// 		
// 		std::regex request_line_regex(R"(^([^ ]*) ([^ ]*) HTTP/([^ ]*)$)");
// 		std::regex header_regex(R"(([A-Z]+[a-z]+.*): (.*))");
// 		std::cmatch match;
// 		if (std::regex_search(m_read_buf, match, request_line_regex)) {
// 			std::string method = match[1];
// 			std::string path = match[2];
// 			std::string http_version = match[3];
// 
// 			printf("Method: %s\n", method);
// 			printf("Path: %s\n", path);
// 			printf("HTTP Version: %s\n", http_version);
// 
// 		}
// 		else
// 		{
// 			flag = false;
// 		}
// 		const char* pos = m_read_buf;
// 		while (std::regex_search(m_read_buf, match, header_regex)) {
// 			std::string header_name = match[1];
// 			std::string header_value = match[2];
// 
// 			printf("Header Name: %s\n", header_name);
// 			printf("Header Value: %s\n", header_value);
// 
// 
// 			// ��������λ��
// 			pos = match.suffix().first;
// 		}
// 		const char* body_start = strstr(m_read_buf, "\n\n");
// 		if (body_start != nullptr) {
// 			std::string body(body_start + 2); // 2��ʾ�������з��ĳ���
// 			printf("Body: %s\n", body);
// 		}
// 
// 	}
// 
// 
// 
// 	return flag? LINE_OK:LINE_BAD;
}

inline char* http_con::get_line()
{
	return m_read_buf + m_start_line;
}

http_con::HTTP_CODE http_con::do_request()
{
	return GET_REQUEST;
}

void http_con::init(int confd, const sockaddr_in& sock)
{
	m_confd = confd;
	m_sockaddrin = sock;
	
	//Ӧ�ò���Ҫ�˿ڸ����ˣ�������ô���õ�

	addfd(http_epollfd, confd, true);
	http_con_num++;
	init();
}

void http_con::init()
{
	m_check_status = CHECK_STATE_REQUESTLINE;
	m_checked_index = 0;
	m_start_line = 0;
	m_read_idx = 0;

	m_url = {};
	m_Version = {};
	m_method = GET;
	m_Linger = false;
	bzero(m_read_buf, http_read_max);
}

void http_con::close()
{
	if (m_confd != -1) {
		removefd(http_epollfd, m_confd);
		http_con_num--;
		m_confd = -1;
	}

}

bool http_con::read()
{
	if (m_read_idx >= http_read_max) {
		return false;
	}
	int bytes_read = 0;
	while (1) {
		bytes_read = recv(m_confd, m_read_buf + m_read_idx, http_read_max - m_read_idx, 0);
		//�ڷ�����IOģʽ�£�������recv����ʱ�����TCP���ջ�������û�����ݻ������ݵĳ���С��Ҫ����յĳ��ȣ���ôrecv�����ͻ��������أ������������ȴ�����ʱ���صĳ��Ⱦͻ�С��Ҫ����յĳ��ȡ������Ϊʲôһ��read���ÿ����޷���ȡ���������ݵ�ԭ��
		//����Ĵ����У���Ȼʹ����while(1)ѭ������������recv�����ǵ�recv���� - 1����errno������ΪEAGAIN��EWOULDBLOCKʱ����Ĵ��������whileѭ����������Ϊ�ڷ�����IOģʽ�£�EAGAIN��EWOULDBLOCK��ʾ��ʱû�����ݿɶ�����Ҫ�Ժ����ԡ���ˣ���Ĵ�������������»�ֹͣ��ȡ���ݣ����ȴ���һ�δ���EPOLLIN�¼�ʱ�ټ�����ȡ��
		if (bytes_read == -1) {	//�Ƿ���û�����ݿɶ����߷��ͻ���������
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;//��break������ѭ����
			}
			return false;//����
		}
		else if (bytes_read == 0) { //�Ͽ�����
			return false;
		}
		printf("Message : \n%s", m_read_buf);
		m_read_idx += bytes_read;
		
	}
	printf("Read over!\n");
	return true;
}

bool http_con::write()
{
	printf("��������!\n");
	return true;
}
