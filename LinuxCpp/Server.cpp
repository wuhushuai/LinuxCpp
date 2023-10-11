#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "threadpool.h"
#include "http_con.h"
//fcntl  F_GETFL  F_SETFL O_NONBLOCK

int http_con::http_epollfd = -1;
int http_con::http_con_num = 0;

#define DEFAULT_BUFLEN 256
#define CLIENT_MAX 1024
#define Connect_MAX 65535

extern int setnonblocking(int fd);
extern void addfd(int epollfd, int fd, bool one_shot,bool flag = 1);
extern void removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int)) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}



int main(int argc , char * argv[]) {
	printf("%s  %s  %s\n",argv[0],argv[1],argv[2]);
	
	if (argc != 3) {
		printf("you should input Server.exe ip port\n");
		return -1;
	}
	
	addsig(SIGPIPE, SIG_IGN);
	

	threadpool< http_con >* pool = NULL;
	try {
		pool = new threadpool<http_con>;
	}
	catch (...) {
		return 1;
	}
	http_con* http = new http_con[Connect_MAX];	//连接类

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in sockaddr_;
	sockaddr_.sin_addr.s_addr = INADDR_ANY;
	sockaddr_.sin_family = AF_INET;
	sockaddr_.sin_port = htons(atoi(argv[2]));

	// 端口复用   要在绑定之前
	//也称为socket地址复用，是指在同一主机上允许多个套接字绑定到同一个端口号上，只要它们的 IP 地址不同就可以
	int reuse = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	//opt：opt表示"option"，即选项。在网络编程中，我们可以通过设置不同的选项来控制套接字的行为。
	//SOL_SOCKET：SOL_SOCKET是socket选项的级别。它表示我们要设置的选项是属于套接字级别的，而不是其他级别的选项。
	//SO_REUSEADDR：SO_REUSEADDR是一个socket选项，用于启用端口复用。它允许多个套接字绑定到同一个端口。
	//optval：optval是一个整数值，用于设置socket选项的值。在这个例子中，optval的值为1，表示启用SO_REUSEADDR选项。
	bind(listenfd, (sockaddr*)&sockaddr_, sizeof(sockaddr));
	listen(listenfd, 8);

	// 创建epoll对象，和事件数组，添加
	int epollfd = epoll_create(5); //1
	epoll_event eventInfo[CLIENT_MAX];			//信息结构体
	http_con::http_epollfd = epollfd;
	addfd(epollfd, listenfd, false , false);

	while (true) {
		int number = epoll_wait(epollfd, eventInfo, CLIENT_MAX, -1);//等待事件的超时时间 -1无限等待 0：立即返回
		//错误：返回-1并正确设置errno
		if ((number < 0) && (errno != EINTR)) {//出现错误且不是被信号打断的情况
			printf("epoll failure\n");
			break;
		}
		for (int i = 0; i < number; i++) {
			if (i == 0) {
				printf("0\n");
			}
			int confd = eventInfo[i].data.fd;
			if (confd == listenfd) {
				sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				confd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
				if (confd == -1) {
					// accept错误处理
					perror("accept\n");
					continue;
				}
				if (http_con::http_con_num >= Connect_MAX) {
					close(confd);
					perror("too Connect_MAX\n");
					continue;
				}
				http[confd].init(confd, client_address);
			}
			else if (eventInfo[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
					//出现断开，或者错误就close
				printf("断开!\n");
				http[confd].close();
			}
			else if (eventInfo[i].events & EPOLLIN) {
				printf("get EPOLLIN\n");
				if (http[confd].read()) {
					pool->append(&http[confd]);
				}
				else //如果read完了就会返回true，那边断开或者出错就会返回false
				{
					http[confd].close();
				}
				
				
			}
			else if (eventInfo[i].events & EPOLLOUT) {

				if (!http[confd].write()) {
					http[confd].close();
				}

			}
		}

	}
	close(epollfd);//epollfd也要
	close(listenfd);
	delete[] http;
	delete pool;
	return 0;
}

