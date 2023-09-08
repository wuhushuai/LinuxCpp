#ifdef __INTELLISENSE__ 
using __float128 = long double; // or some fake 128 bit floating point type
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>//assert
#include <unistd.h>
#include <netinet/in.h>//目前没地方用到
#include <arpa/inet.h>//=#include <sys/types.h> + #include <sys/socket.h>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#define DEFAULT_BUFLEN 256
#define Threads_Max 128

int echo_clinet(int sockfd,  sockaddr_in outSocketadrr) {
	char recvbuf[DEFAULT_BUFLEN] = "";
	while (int clientRecv = recv(sockfd, recvbuf, sizeof(recvbuf), 0)) {
		if (clientRecv == 0 || recvbuf[0] == 0x0A) {//
			perror("have client closed ...");
			exit(-1);
		}
		else if (clientRecv < 0) {
			perror("recv"+__LINE__);
			exit(-1);
		}
		//
		int currentIndex = 0;
		int len = strlen(recvbuf);
		while (len--) {
			for (int i = 0; i < 10; i++) {
	
				printf("%02X ", (unsigned char)recvbuf[currentIndex]);
				currentIndex++;
			}
			printf("\n");
		}
		//
		char* clientIp = inet_ntoa(outSocketadrr.sin_addr);
		int clientPort = ntohs(outSocketadrr.sin_port);
		printf("来自: ip %s port %d , 接收到的信息：%s\n", clientIp, clientPort, recvbuf);

		char sendbuf[] = "你好，刚刚接受到了你的消息!";
		int senlen = strlen(sendbuf) + strlen(recvbuf) + 1;
		memcpy(sendbuf + strlen(sendbuf), recvbuf, senlen);
		send(sockfd, sendbuf, senlen, 0);
	}
	close(sockfd);
	//exit(0);



}

	
	

//线程函数
struct  sockinfo {
	int fd;
	struct sockaddr_in clientaddr;
	pthread_t tid;
	sockinfo() {
		fd = -1;
		bzero(&clientaddr, sizeof(sockaddr_in));
		tid = -1;
	}
};
void* thread_func(void* arg) {
	sockinfo* clientInfo = (sockinfo*)arg;
	printf("soure tip: %d 's Client message", clientInfo->tid);
	echo_clinet(clientInfo->fd, clientInfo->clientaddr);
	return NULL;
}
//线程池
sockinfo threadsSock[Threads_Max];

int main(int argc, char* argv[])
{
	if (argc <=2) {
		printf("参数必须包含ip 和 port");
		return 0;
	}
	//
		//../
	//
	
	char * ip = argv[1];
	int port = atoi(argv[2]);// ascii to intege
	// ipv4  tcp  0
	int socketServer = socket(AF_INET, SOCK_STREAM, 0);//这里最好变量名是listenfd 确实是用来监听的
	if (socketServer == -1) {
		perror("socket");
		exit(-1);
	}
	struct sockaddr_in socketadrr;
	//TO 1.也可以不用memset填空
	socketadrr.sin_family = AF_INET;
	socketadrr.sin_port = htons(port);  //host to n s
	inet_pton(AF_INET, ip, &socketadrr.sin_addr.s_addr);
	//绑定 socket  ip,port  sizeof
	  //先给程序员使用再强制转化成操作系统用的
	//1.这里可以不用(struct sockaddr*)
	//监听
	int ret = bind(socketServer, (sockaddr*)&socketadrr, sizeof(sockaddr_in));
	if (ret ==-1) {
		perror("bind");
		exit(-1);
	}
	int retListen = listen(socketServer, 10);
	if (retListen <  0) {
		perror("Listen");
		exit(-1);
	}
	int acceptSocket{};
	while (1) {
	
	struct sockaddr_in outSocketadrr;
	socklen_t outSocketaddrlen = sizeof(sockaddr_in); //socklen =
	//socket 接受的 ip,port sizeof的地址;感觉用不到确实可以设置为NULL
	acceptSocket = accept(socketServer, (struct sockaddr*)&outSocketadrr,&outSocketaddrlen);
	if (acceptSocket == -1) {
		if (errno == EINTR) {
			perror("acceptSocket");
			continue;
		}
		else
		{
			perror("acceptSocket" + __LINE__);//printf error
			exit(-1);
		}
	}
	//------------------------------------------

	//从线程池拿可用线程
	struct sockinfo* pinfo;
	for (int i = 0; i < Threads_Max; i++) {
		// 从这个数组中找到一个可以用的sockInfo元素
		if (threadsSock[i].fd == -1) {//如果找到子线程的情况
			pinfo = &threadsSock[i]; //
			break;
		}
		// i的取值范围  {0,127}
		 //没有找到子线程的情况，再从后往前找
		if (i == Threads_Max - 1) {
			sleep(1);
			i--;
		}
	}
	//参数一：描述符文件赋值
	pinfo->fd = acceptSocket;
	//参数二：客户端的套接字结构体赋值,len就是结构体的大小
	memcpy(&pinfo->clientaddr, &outSocketadrr, sizeof(sockaddr_in));

	// 创建子线程,顺便可以给参数一赋值
	pthread_create(&pinfo->tid, NULL, thread_func, pinfo);

	pthread_detach(pinfo->tid); //分离线程，


	}
	//------------------------------------------

	close(acceptSocket); 
	close(socketServer);
	return 0;
}
