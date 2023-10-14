#include "http_con.h"


// ����HTTP��Ӧ��һЩ״̬��Ϣ
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";


//��վ��Դ��Ŀ¼
const char* doc_root = "/home/ubuntu/projects/LinuxCpp/Resources";



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
		event.events |= EPOLLET;  //���˼�|������ϣ������ж���������ӹ�������Ҫ�ȵ�������������ֻ����һ�Σ�������������������������ֻ������һ�Σ�����ľͲ������ˡ�
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
//oneshotֻ��epollfd��Ӱ��
void modfd(int epollfd,int fd,int ev) {//ev �ĳɶ�����д
	epoll_event epollev;
	epollev.data.fd = fd;//�����������������⣡����������������������������
	epollev.events = ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;//����ev
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epollev);
}

void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void http_con::process()
{
	HTTP_CODE read_ret = process_read();
	//������Ҫ��������ǲ�����Ϊ����û�������ݣ�������Ҫ��������������ǣ�˵�����Ǵ������ˣ���Ҫ��������Ӧ
	//��������ʶ���������һ�Σ���������һ��
	 if (read_ret == NO_REQUEST) {//û�н��յ�������������������Ҫ�ٴμ�����
		 printf("process() -> NO_REQUEST!\n");
		 modfd(http_epollfd, m_confd, EPOLLIN);
		 return;
	 }
	 // ������Ӧ
	 bool write_ret = process_write(read_ret);
	 if (!write_ret) {
		 close_conn();
	 }
	 modfd(http_epollfd, m_confd, EPOLLOUT);//������д�����ݣ����������д���¼���ֻҪд���������п��еľͻᴥ�����¼�

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
				printf("BAD_REQYEST!\n");
				return BAD_REQUEST;
			}
			else if (ret == GET_REQUEST) {
				printf("GET_REQUEST!\n");
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

	return NO_REQUEST;//�������Ƿ���NO_REQUEST��Ϊ������ֻ������������
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
				m_Linger == true;
			}
			else
			{
				m_Linger == false;
			}
		}
		else if (match[1] == "Content-Length") {
			m_content_length = stoi(match[2]);
		}
		
		printf("Header Name: %s\n", match[1].str().c_str());
		printf("Header Value: %s\n", match[2].str().c_str());
	}
	else {  //���жϵ��ⲽ˵������ƥ�䲻�ϣ���˵���������������
		if (m_method == POST) { //�����post�������Ҫ����������
			m_check_status = CHECK_STATE_CONTENT;
			return NO_REQUEST;//Ӧ�÷���NO_REQUEST��˵������Ҫ����������
		}
		//ֻ�е���������вű�ʾ���GET������������
		return GET_REQUEST;//˵����GET�Ǿ�û����������
		
	}
	return NO_REQUEST;
}

http_con::HTTP_CODE http_con::parse_body(char* text)
{
	printf("����������!\n");
	if (m_read_idx >= (m_content_length + m_checked_index))
	{
		text[m_content_length] = '\0';
		return GET_REQUEST;
	}
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
// ���ݷ���������HTTP����Ľ�����������ظ��ͻ��˵�����
bool http_con::process_write(HTTP_CODE ret)
{
	char* data; int len; int j;
	printf("process_write -> %d\n", ret);
	switch (ret)
	{
	case INTERNAL_ERROR:
		add_status_line(500, error_500_title);
		add_headers(strlen(error_500_form));
		if (!add_content(error_500_form)) {
			return false;
		}
		break;
	case BAD_REQUEST:
		add_status_line(400, error_400_title);
		add_headers(strlen(error_400_form));
		if (!add_content(error_400_form)) {
			return false;
		}
		break;
	case NO_RESOURCE:
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form)) {
			return false;
		}
		break;
	case FORBIDDEN_REQUEST:
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form)) {
			return false;
		}
		break;
	case FILE_REQUEST:
		
		add_status_line(200, ok_200_title);
		add_headers(m_file_stat.st_size);
		m_iv[0].iov_base = m_write_buf;  //
		m_iv[0].iov_len = m_write_idx;
		m_iv[1].iov_base = m_file_address;
		m_iv[1].iov_len = m_file_stat.st_size;
		m_iv_count = 2;
		//��δ���ִ�к����ǾͿ��Խ� m_iv �� m_iv_count ���� writev ����������һ������������ڴ������д�������
		bytes_to_send = m_write_idx + m_file_stat.st_size;
	
		printf("FILE_REQUEST!\n");
		printf("bytes_to_send = %d!\n", bytes_to_send);
// 		printf("m_iv[0].iov_len = %d!\n", m_write_idx);
// 				printf("m_iv[1].iov_len = %d!\n", m_file_stat.st_size);
// 				
// 				data = (char*)m_iv[1].iov_base;
// 				len = m_iv[1].iov_len;
// 				for ( j = 0; j < len; j++) {
// 					printf("%02X ", (unsigned char)data[j]);
// 					if ((j + 1) % 16 == 0) {
// 						printf("\n");
// 					}
// 				}
// 				printf("\n");

		return true;
	default:
		return false;
	}
	//���ִ��break;�ͻ�ִ������Ĵ��룺Ҳ���Ǵ�����Ϣ������ֻ�������Ӧ״̬�У���Ӧͷ����Ӧ����
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	bytes_to_send = m_write_idx;

	return true;
}

inline char* http_con::get_line()
{
	return m_read_buf + m_start_line;
}

http_con::HTTP_CODE http_con::do_request()
{
	//home/ubuntu/projects/LinuxCpp/Resources
	//�Ұ�������ˣ����
// 	if (m_url != "/index.html" || m_url != "/XXX.jpg") {
// 		m_url = "/NotFound.html";
// 	}
// 	else if(m_url == "/")
// 	{
// 		m_url = "/index.html";
// 	}
	//Ӧ��������ˣ�˳��С�ĸ����
	strcpy(m_path, doc_root);
	strcpy(m_path + strlen(doc_root), m_url.c_str());
	printf("%s!\n", m_url.c_str());
	// ��ȡm_real_file�ļ�����ص�״̬��Ϣ��-1ʧ�ܣ�0�ɹ�
	if (stat(m_path, &m_file_stat) < 0) {
		printf("NO_RESOURCE!\n");
		return NO_RESOURCE;
	}//stat ��������ȡ�ļ�����ϸ��Ϣ
	if (!(m_file_stat.st_mode & S_IROTH)) {
		printf("FORBIDDEN_REQUEST!\n");
		return FORBIDDEN_REQUEST;
	}
	// �ж��Ƿ���Ŀ¼
	if (S_ISDIR(m_file_stat.st_mode)) {
		return BAD_REQUEST;
	}
	printf("url = %s\n", m_url.c_str());

	printf("path : %s\n", m_path);

	int fd = open(m_path, O_RDONLY);
	if (fd == -1) {
		// �ļ������ڣ�����404����
		printf("BUG to [open]\n");
		return NO_RESOURCE;
	}
	// ���ļ�ӳ�䵽�ڴ�
	m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	//NULL��ָ��ӳ�����ʼ��ַΪNULL����ϵͳ�Զ�ѡ����ʵĵ�ַ
	//file_size��ָ��ӳ��Ĵ�С����Ҫӳ����ļ��Ĵ�С
	//PROT_READ��ָ��ӳ������ı�����ʽ��������ֻ��
	//MAP_PRIVATE��ָ��ӳ��ķ�ʽ��������˽��ӳ�䣬��ӳ��������޸Ĳ���д�ص��ļ���
	//fd��ָ��Ҫӳ����ļ����ļ�������
	//0��ָ��ӳ���ƫ�����������Ǵ��ļ�����ʼλ�ÿ�ʼӳ��
	//ȷ����ʹ��mmap����֮�󣬼�鷵�ص�ָ���Ƿ�ΪMAP_FAILED���Լ��ڲ���ʹ��ʱʹ��munmap��������ڴ�ӳ��
	if (m_file_address == MAP_FAILED) {
		// �ڴ�ӳ��ʧ�ܣ�����500����
		printf("HTTP/1.1 500 Internal Server Error\r\n\r\n");
		close(fd);
		return INTERNAL_ERROR;
	}
	close(fd);
	return FILE_REQUEST;
}

void http_con::unmap()
{
	if (m_file_address)
	{
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}

// ��д������д������͵�����
bool http_con::add_response(const char* format, ...) {
	if (m_write_idx >= http_wirte_max) {  //���ܳ���д�������Ĵ�С
		return false;
	}
	//va_list��һ�����ڴ洢�ɱ����������
	va_list arg_list;
	va_start(arg_list, format);// ���������ǳ�ʼ��һ��va_list���͵ı�����ʹ��ָ��ɱ�����б�(format����)�ĵ�һ��������
	//vsnprintf�������ڽ��ɱ��������ָ���ĸ�ʽд�뵽��������
	//len = ��������ʵ��д����ַ���
	//��ʱarg_list �ʹ洢%s %d %s ��Ӧ��ֵ��Ȼ���ٽ���ƴ�ӵ�"%s %d %s\r\n"
	//ƴ�����˾ʹ��뻺������
	int len = vsnprintf(m_write_buf + m_write_idx, http_wirte_max - 1 - m_write_idx, format, arg_list);
	//���ʵ�ʴ���ĳ��ȴ����˺������ڴ�ʹ���ʧ��
	if (len >= (http_wirte_max - 1 - m_write_idx)) {
		return false;
	}
	//����Ϊʵ��д����ַ���
	m_write_idx += len;
	//�����ͷ���ɱ�����б���ص���Դ��
	va_end(arg_list);
	return true;
}

bool http_con::add_content_length(int content_len)
{
	return add_response("Content-Length: %d\r\n", content_len);
}

bool http_con::add_content(const char* content)
{
	return add_response("%s", content);
}

bool http_con::add_content_type()
{
	return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_con::add_linger()
{
	return add_response("Connection: %s\r\n", (m_Linger == true) ? "keep-alive" : "close");
}

bool http_con::add_blank_line()
{
	return add_response("%s", "\r\n");
}

bool http_con::add_headers(int content_len)
{
	add_content_length(content_len);
	add_content_type();
	add_linger();
	add_blank_line();
	return false;
}

inline bool http_con::add_status_line(int status, const char* title) {
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
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
	bytes_to_send = 0;
	bytes_have_send = 0;

	m_check_status = CHECK_STATE_REQUESTLINE;
	m_checked_index = 0;
	m_start_line = 0;
	m_read_idx = 0;
	m_write_idx = 0;

	m_Host = {};
	m_url = {};
	m_Version = {};
	m_method = GET;
	m_Linger = false;
	bzero(m_read_buf, http_read_max);
	bzero(m_write_buf, http_wirte_max);
	bzero(m_path, PATH_MAX);
	m_content_length = 0;
}

void http_con::close_conn()
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
	printf("write!\n");
	int temp = 0;

	if (bytes_to_send == 0) {
		// ��Ҫ���͵��ֽ�Ϊ0����һ����Ӧ������
		printf("bytes_to_send == 0\n");
		modfd(http_epollfd, m_confd, EPOLLIN);
		init();
		return true;
	}
	//ע��������ѭ��
	while (1) {
		// ��ɢд//writev �ɲ�ͬ���ڴ��ϵ�һ��
		//writev �����ᰴ�� iovec ������Ԫ�ص�˳�����ν�ÿ��Ԫ����ָ����ڴ����������д�뵽 fd ��ָ����ļ��С�
		temp = writev(m_confd, m_iv, m_iv_count);
		if (temp <= -1) {
			// ���TCPд����û�пռ䣬��ȴ���һ��EPOLLOUT�¼�����Ȼ�ڴ��ڼ䣬
			// �������޷��������յ�ͬһ�ͻ�����һ�����󣬵����Ա�֤���ӵ������ԡ�
			if (errno == EAGAIN) {
				modfd(http_epollfd, m_confd, EPOLLOUT);
				return true;
			}
			printf("writev return -1\n");
			unmap();
			return false;
		}
		char* str = (char*)&m_iv[0];
		//printf("%s\n", str);
		bytes_have_send += temp;		//������ʵ�ʵ�����
		//bytes_to_send = m_write_idx + m_file_stat.st_size;��ǰ�������������+�ļ�����
		bytes_to_send -= temp;		//�����������׼���õ������ж��

		//��m_write_bufд�����������͵������Ƿ��Ѿ�ȫ���������
		if (bytes_have_send >= m_iv[0].iov_len)	//m_write_bufд�����������͵�����[�洢���������������]
		{
			//ǰ�����m_iv[0].iov_len = m_write_idx;
			m_iv[0].iov_len = 0;
			//m_write_idx��add_response������,m_write_idx += len;//����Ϊʵ��д����ַ���()
			//m_iv[0]���ڴ����Ӧ���С���Ӧͷ����Ӧ���е����ݣ�������������
			//m_iv[1]���ڴ���ļ�����

			//m_iv[1].iov_base = m_file_address;
			//ΪʲôҪ�޸���Ϊbytes_have_send >= m_iv[0].iov_len��ʵ�����������һ�������Ǹպ÷������˻��������ݣ�һ���Ǵ��ڣ�˵���ļ�����Ҳ������һЩ������������Ҫ���¶�λ
			m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
			m_iv[1].iov_len = bytes_to_send;
		}
		else//�������˼�ǻ���������û���꣬��Ҫ���¶�λ
		{
			m_iv[0].iov_base = m_write_buf + bytes_have_send;
			m_iv[0].iov_len = m_iv[0].iov_len - temp;
		}

		if (bytes_to_send <= 0)
		{
			// û������Ҫ������
			unmap();
			modfd(http_epollfd, m_confd, EPOLLIN);

			if (m_Linger)
			{
				init();
				return true;
			}
			else
			{
				return false;
			}
		}

	}
	return true;
}
