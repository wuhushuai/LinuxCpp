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
	event.events = EPOLLIN | EPOLLRDHUP;//同时监听读事件和对端关闭事件 HUP:挂起
	//所以就不用判断recv的返回值来知道他是否关闭，而是监听某个事件
	//不用EPOLLRDHUP的话： 之前服务端需要通过调用recv函数且返回为0进行判断，当返回值为0说明对端已经关闭了socket，此时我们直接调用close关闭本端的socket即可。
	if (flag) {
		event.events |= EPOLLET;  //忘了加|
	}
	if (one_shot)
	{
		// 防止同一个通信被不同的线程处理 Oneshot
		event.events |= EPOLLONESHOT;
		//比如一个线程正在读取某个socket上的数据，正在处理的时候又来了新的数据可读就会被另一个线程 捕获，于是就有了两个线程操作一个socket的场面
		//EPOLLONESHOT可以时一个socket只能被一个线程处理
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	// 设置文件描述符非阻塞
	setnonblocking(fd);
}

void modfd(int epollfd,int fd,int ev) {//ev 改成读还是写
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
	//初始化一下
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;
	
	while (((m_check_status == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
		|| ((line_status = parse_line()) == LINE_OK)) {
		// 需要获取一行数据
		
		text = get_line(); //

		m_start_line = m_checked_index;
		//printf("got 1 http line: %s\n", text);

		//获取一行数据line_status = parse_line()，根据主状态机的状态做不同的处理
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

//解析HTPP请求，文件名，HTTP版本
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

		m_check_status = CHECK_STATE_HEADER;//解析完了请求行开始解析请求头

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
			//return GET_REQUEST;  没分析完呢，return啥
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
	else {  //能判断到这步说明正则匹配不上，那说明到了请求空行了
		if (m_method == POST) { //如果是post请求才需要解析请求体
			m_check_status = CHECK_STATE_CONTENT;
			return NO_REQUEST;//应该返回NO_REQUEST，说明还需要解析请求体
		}
		return GET_REQUEST;//说明是GET那就没有请求体了
		
	}
	return NO_REQUEST;
}

http_con::HTTP_CODE http_con::parse_body(char* text)
{
	printf("解析请求体!\n");
	return NO_REQUEST;
}

http_con::LINE_STATUS http_con::parse_line(){

	char temp;
	//m_checked_index是正在解析的位置
	//m_read_idx是调用read时候的index，记录的是m_read_buf原始buf的最后一个字节+1
	//为什么是最后一个字节加1，这个Index本来就是为了一次一次接受数据确保数据连续的index
	//printf("m_checked_index = %d\n", m_checked_index);
	//printf("m_read_idx = %d\n", m_read_idx);
	for (; m_checked_index < m_read_idx; ++m_checked_index) {
		temp = m_read_buf[m_checked_index];//当前分析字符
		if (temp == '\r') {
			if ((m_checked_index + 1) == m_read_idx) {//正常应该是被for循环结束，刚好到read_buf的尾部，但是这里发现+1就到尾部了，说明即当前行的结束标志\r\n还没有完全出现在缓冲区，\r的下一行应该是\n
				return LINE_OPEN;//行数据不完整
			}
			else if (m_read_buf[m_checked_index + 1] == '\n') {//吧\r\n替换成\0 \0
				m_read_buf[m_checked_index++] = '\0';
				m_read_buf[m_checked_index++] = '\0';//\0是表示字符串结束的空字符
				return LINE_OK;
			}
			return LINE_BAD;//如果是其他情况，则行出错
		}
		else if (temp == '\n') {//当前分析字符如果是\n
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
// 			// 更新搜索位置
// 			pos = match.suffix().first;
// 		}
// 		const char* body_start = strstr(m_read_buf, "\n\n");
// 		if (body_start != nullptr) {
// 			std::string body(body_start + 2); // 2表示两个换行符的长度
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
	
	//应该不需要端口复用了，我是这么觉得的

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
		//在非阻塞IO模式下，当调用recv函数时，如果TCP接收缓冲区中没有数据或者数据的长度小于要求接收的长度，那么recv函数就会立即返回，而不会阻塞等待，此时返回的长度就会小于要求接收的长度。这就是为什么一次read调用可能无法读取完所有数据的原因。
		//在你的代码中，虽然使用了while(1)循环来反复调用recv，但是当recv返回 - 1并且errno被设置为EAGAIN或EWOULDBLOCK时，你的代码会跳出while循环。这是因为在非阻塞IO模式下，EAGAIN或EWOULDBLOCK表示此时没有数据可读，需要稍后再试。因此，你的代码在这种情况下会停止读取数据，并等待下一次触发EPOLLIN事件时再继续读取。
		if (bytes_read == -1) {	//是否是没有数据可读或者发送缓冲区已满
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;//不break就是死循环。
			}
			return false;//出错
		}
		else if (bytes_read == 0) { //断开连接
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
	printf("处理数据!\n");
	return true;
}
