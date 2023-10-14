#include "http_con.h"


// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";


//网站资源根目录
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
	event.events = EPOLLIN | EPOLLRDHUP;//同时监听读事件和对端关闭事件 HUP:挂起
	//所以就不用判断recv的返回值来知道他是否关闭，而是监听某个事件
	//不用EPOLLRDHUP的话： 之前服务端需要通过调用recv函数且返回为0进行判断，当返回值为0说明对端已经关闭了socket，此时我们直接调用close关闭本端的socket即可。
	if (flag) {
		event.events |= EPOLLET;  //忘了加|，我们希望如果有多个请求连接过来，需要等到被处理，而不是只处理一次：【比如连接来了两个，但是只处理了一次，后面的就不处理了】
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
//oneshot只对epollfd有影响
void modfd(int epollfd,int fd,int ev) {//ev 改成读还是写
	epoll_event epollev;
	epollev.data.fd = fd;//大概率是这里除了问题！！！！！！！！！！！！！！！
	epollev.events = ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;//少了ev
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
	//这里主要处理的是是不是因为错误没读入数据，我们需要继续读。如果不是，说明我们处理完了，需要对她做响应
	//浏览器访问都是先请求一次，发送数据一次
	 if (read_ret == NO_REQUEST) {//没有接收到他的完整请求，我们需要再次监听他
		 printf("process() -> NO_REQUEST!\n");
		 modfd(http_epollfd, m_confd, EPOLLIN);
		 return;
	 }
	 // 生成响应
	 bool write_ret = process_write(read_ret);
	 if (!write_ret) {
		 close_conn();
	 }
	 modfd(http_epollfd, m_confd, EPOLLOUT);//当我们写好数据，就申请监听写入事件，只要写缓冲区是有空闲的就会触发该事件

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

	return NO_REQUEST;//正常就是返回NO_REQUEST因为你这里只解析了请求行
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
	else {  //能判断到这步说明正则匹配不上，那说明到了请求空行了
		if (m_method == POST) { //如果是post请求才需要解析请求体
			m_check_status = CHECK_STATE_CONTENT;
			return NO_REQUEST;//应该返回NO_REQUEST，说明还需要解析请求体
		}
		//只有到了请求空行才表示获得GET的完整数据了
		return GET_REQUEST;//说明是GET那就没有请求体了
		
	}
	return NO_REQUEST;
}

http_con::HTTP_CODE http_con::parse_body(char* text)
{
	printf("解析请求体!\n");
	if (m_read_idx >= (m_content_length + m_checked_index))
	{
		text[m_content_length] = '\0';
		return GET_REQUEST;
	}
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
// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
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
		//这段代码执行后，我们就可以将 m_iv 和 m_iv_count 传给 writev 函数，让它一次性完成两块内存区域的写入操作。
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
	//如果执行break;就会执行下面的代码：也就是错误消息，下面只添加了响应状态行，响应头，响应空行
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
	//我把这里改了，如果
// 	if (m_url != "/index.html" || m_url != "/XXX.jpg") {
// 		m_url = "/NotFound.html";
// 	}
// 	else if(m_url == "/")
// 	{
// 		m_url = "/index.html";
// 	}
	//应该先组合了，顺序不小心搞错了
	strcpy(m_path, doc_root);
	strcpy(m_path + strlen(doc_root), m_url.c_str());
	printf("%s!\n", m_url.c_str());
	// 获取m_real_file文件的相关的状态信息，-1失败，0成功
	if (stat(m_path, &m_file_stat) < 0) {
		printf("NO_RESOURCE!\n");
		return NO_RESOURCE;
	}//stat 函数来获取文件的详细信息
	if (!(m_file_stat.st_mode & S_IROTH)) {
		printf("FORBIDDEN_REQUEST!\n");
		return FORBIDDEN_REQUEST;
	}
	// 判断是否是目录
	if (S_ISDIR(m_file_stat.st_mode)) {
		return BAD_REQUEST;
	}
	printf("url = %s\n", m_url.c_str());

	printf("path : %s\n", m_path);

	int fd = open(m_path, O_RDONLY);
	if (fd == -1) {
		// 文件不存在，返回404错误
		printf("BUG to [open]\n");
		return NO_RESOURCE;
	}
	// 将文件映射到内存
	m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	//NULL：指定映射的起始地址为NULL，由系统自动选择合适的地址
	//file_size：指定映射的大小，即要映射的文件的大小
	//PROT_READ：指定映射区域的保护方式，这里是只读
	//MAP_PRIVATE：指定映射的方式，这里是私有映射，对映射区域的修改不会写回到文件中
	//fd：指定要映射的文件的文件描述符
	//0：指定映射的偏移量，这里是从文件的起始位置开始映射
	//确保在使用mmap函数之后，检查返回的指针是否为MAP_FAILED，以及在不再使用时使用munmap函数解除内存映射
	if (m_file_address == MAP_FAILED) {
		// 内存映射失败，返回500错误
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

// 往写缓冲中写入待发送的数据
bool http_con::add_response(const char* format, ...) {
	if (m_write_idx >= http_wirte_max) {  //不能超过写缓冲区的大小
		return false;
	}
	//va_list是一个用于存储可变参数的类型
	va_list arg_list;
	va_start(arg_list, format);// 它的作用是初始化一个va_list类型的变量，使其指向可变参数列表(format参数)的第一个参数。
	//vsnprintf函数用于将可变参数按照指定的格式写入到缓冲区中
	//len = 函数返回实际写入的字符数
	//此时arg_list 就存储%s %d %s 对应的值，然后再进行拼接到"%s %d %s\r\n"
	//拼接玩了就传入缓冲区中
	int len = vsnprintf(m_write_buf + m_write_idx, http_wirte_max - 1 - m_write_idx, format, arg_list);
	//如果实际传入的长度大于了后续的内存就代表失败
	if (len >= (http_wirte_max - 1 - m_write_idx)) {
		return false;
	}
	//更新为实际写入的字符数
	m_write_idx += len;
	//清理并释放与可变参数列表相关的资源。
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
	
	//应该不需要端口复用了，我是这么觉得的

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
	printf("write!\n");
	int temp = 0;

	if (bytes_to_send == 0) {
		// 将要发送的字节为0，这一次响应结束。
		printf("bytes_to_send == 0\n");
		modfd(http_epollfd, m_confd, EPOLLIN);
		init();
		return true;
	}
	//注意这里是循环
	while (1) {
		// 分散写//writev 吧不同的内存块合到一起
		//writev 函数会按照 iovec 数组中元素的顺序，依次将每个元素所指向的内存区域的内容写入到 fd 所指向的文件中。
		temp = writev(m_confd, m_iv, m_iv_count);
		if (temp <= -1) {
			// 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
			// 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
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
		bytes_have_send += temp;		//这里是实际的数据
		//bytes_to_send = m_write_idx + m_file_stat.st_size;在前面代表缓冲区数据+文件数据
		bytes_to_send -= temp;		//这里就是我们准备好的数据有多大

		//即m_write_buf写缓冲区待发送的数据是否已经全部发送完毕
		if (bytes_have_send >= m_iv[0].iov_len)	//m_write_buf写缓冲区待发送的数据[存储请求体以外的数据]
		{
			//前面就是m_iv[0].iov_len = m_write_idx;
			m_iv[0].iov_len = 0;
			//m_write_idx在add_response函数中,m_write_idx += len;//更新为实际写入的字符数()
			//m_iv[0]用于存放响应首行、响应头和响应空行的数据，即缓冲区数据
			//m_iv[1]用于存放文件数据

			//m_iv[1].iov_base = m_file_address;
			//为什么要修改因为bytes_have_send >= m_iv[0].iov_len其实是两种情况，一个等于是刚好发送完了缓冲区数据，一个是大于，说明文件数据也发送了一些，所以我们需要重新定位
			m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
			m_iv[1].iov_len = bytes_to_send;
		}
		else//这里的意思是缓冲区数据没发完，需要重新定位
		{
			m_iv[0].iov_base = m_write_buf + bytes_have_send;
			m_iv[0].iov_len = m_iv[0].iov_len - temp;
		}

		if (bytes_to_send <= 0)
		{
			// 没有数据要发送了
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
