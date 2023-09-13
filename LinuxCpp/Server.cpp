#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#define DEFAULT_BUFLEN 256
#define CLIENT_MAX 1024

int main() {
	//epoll的ET（Edge Triggered）模式并不会直接使recv不阻塞，它只是一种事件通知方式
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
	//获取lfd文件描述符的属性，在TA属性上添加不阻塞的信息再填回lfd

	//想让recv不阻塞，你需要将socket套接字设置为非阻塞模式，下面不用提前设置，因为循环里就会设置
	//int flags = fcntl(lfd, F_GETFL, 0);
	//fcntl(lfd, F_SETFL, flags | O_NONBLOCK);

	int epfd = epoll_create(800);//800是随便填的，内核会自动判断要多少的值
	epoll_event epollet;
	epollet.data.fd = lfd;
	epollet.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &epollet);//虽然传入了两个文件描述符，参数3的描述符意思是对谁操作
												//参数属于传入传出参数，会当epoll_wait的时候会返回变化
	//  如果成功，函数返回0。如果失败，函数返回-1，并设置errno为相应的错误代码。
	epoll_event epollets[CLIENT_MAX];

	while (1) {
		//第二个参数就是输出参数，预计处理1024个连接，返回到这个结构体数组
		int ret = epoll_wait(epfd, epollets, CLIENT_MAX, -1);
		//第二个参数events [OUT]：指向epoll_event结构体数组的指针，用来接收就绪的事件
		if (ret > 0) {
			for (int i = 0; i < ret; i++) // 成功，返回发送变化的文件描述符的个数
			{
				int curfd = epollets[i].data.fd;
				//如果是新客户端
				if (curfd == lfd) {
					sockaddr_in outsockAddr;
					memset(&outsockAddr, 0, sizeof(sockaddr_in));
					socklen_t len;
					int cfd = accept(curfd, (sockaddr*)&outsockAddr, &len);
					if (cfd == -1) {
						perror("accept");
						continue;
					}
					//吧cfd设置为不阻塞模式，使他接受数据不会阻塞
					int flags = fcntl(cfd, F_GETFL, 0);
					fcntl(cfd, F_SETFL, flags | O_NONBLOCK);//文件描述符的属性就会是不阻塞
					//当你调用recv函数时，如果没有数据可以读取，recv就会立即返回，而不是等待数据到来。这样就实现了非阻塞。

					//设置epoll接受事件的结构体epoll_event为非阻塞模式
					epollet.data.fd = cfd;
					epollet.events = EPOLLIN | EPOLLET;//epoll会对这个文件描述符启动ET模式
					epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &epollet);
					printf("cfd = %d is come on!\n", cfd);
				}
				else
				{
					char recvBuf[DEFAULT_BUFLEN] = { 0 };
					int recvRetn = -1;
					//while的循环不要吧下面 < 0这些判断包括进去了//我说怎么收不到断开连接
					while (recvRetn = recv(curfd, recvBuf, DEFAULT_BUFLEN, 0)) {
						if (recvRetn > 0) {
							printf("client cfd = %d get msg = %s\n", curfd, recvBuf);
							//char sendBuf[DEFAULT_BUFLEN] = { 0 };
							//fgets(sendBuf, DEFAULT_BUFLEN, stdin);
							//send(curfd, sendBuf, strlen(sendBuf), 0);

							memset(recvBuf, 0, DEFAULT_BUFLEN);//清空接收缓冲区，以确保不会处理旧数据
						}
					}
					if (recvRetn < 0)//当recv函数返回-1时，表示发生了错误。错误的类型可以通过检查errno变量来确定
					{
						//errno的值是EWOULDBLOCK或EAGAIN，表示当前没有更多的数据可读取，这不是算是一个错误
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							// 数据已经全部读完

						}
						else
						{
							perror("recv");
							epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
							close(curfd);

						}

					}
					else if (recvRetn == 0)
					{
						printf("client cfd = %d is closed .....\n", curfd);
						epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL); //用不到传入，因为是删除
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