#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_BUFLEN 256
#define CLIENT_MAX 1024

int main() {
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addrsock;
	addrsock.sin_addr.s_addr = INADDR_ANY;
	addrsock.sin_family = AF_INET;
	addrsock.sin_port = htons(8080);
	int ret = bind(lfd, (sockaddr*)&addrsock, sizeof(sockaddr_in));
	if (ret == -1) {
		perror("bind");
		exit(-1);
	}
	listen(lfd, 8);

	int epfd = epoll_create(800);
	epoll_event epollet;
	epollet.data.fd = lfd;
	epollet.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD,lfd, &epollet);//虽然传入了两个文件描述符，参数3的描述符意思是对谁操作
												//参数属于传入传出参数，会当epoll_wait的时候会返回变化
	//  如果成功，函数返回0。如果失败，函数返回-1，并设置errno为相应的错误代码。
	epoll_event epollets[CLIENT_MAX];

	while (1) {
		//第二个参数就是输出参数，预计处理1024个连接，返回到这个结构体数组
		int ret = epoll_wait(epfd, epollets, CLIENT_MAX, -1);
		//第二个参数events [OUT]：指向epoll_event结构体数组的指针，用来接收就绪的事件
		if (ret > 0 ) {
			for (int i =0; i < ret ;i++) // 成功，返回发送变化的文件描述符的个数
			{
				int curfd = epollets[i].data.fd;
				//如果是新客户端
				if (curfd == lfd) {
					sockaddr_in outsockAddr;
					memset(&outsockAddr, 0, sizeof(sockaddr_in));
					socklen_t len;
					int cfd = accept(curfd, (sockaddr*)&outsockAddr, &len);
					epollet.data.fd = cfd;
					epollet.events = EPOLLIN;
					epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &epollet);
					printf("cfd = %d is come on!\n", cfd);
				}
				else
				{
					char recvBuf[DEFAULT_BUFLEN] = { 0 };
					int recvRetn = recv(curfd, recvBuf, DEFAULT_BUFLEN, 0);
					if (recvRetn > 0) {
						printf("client cfd = %d get msg = %s\n", curfd, recvBuf);
						//char sendBuf[DEFAULT_BUFLEN] = { 0 };
						//fgets(sendBuf, DEFAULT_BUFLEN, stdin);
						//send(curfd, sendBuf, strlen(sendBuf), 0);
					}
					else if(recvRetn < 0)
					{
						perror("recv");
						continue;
					}
					else
					{
						printf("client cfd = %d is closed .....\n", curfd);
						epoll_ctl(epfd, EPOLL_CTL_ADD, curfd, NULL); //用不到传入，因为是删除
						close(curfd);
					}
				}



			}


		}
		else
		{
			perror("epoll_wait");
			exit(-1);
		}


	}
	close(lfd);
	close(epfd);
	return 0;
}