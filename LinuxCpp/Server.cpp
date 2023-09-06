#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>//assert
#include <unistd.h>
#include <netinet/in.h>//目前没地方用到
#include <arpa/inet.h>//=#include <sys/types.h> + #include <sys/socket.h>
#include <cstring>
#define DEFAULT_BUFLEN 256

int echo_clinet(int sockfd,  sockaddr_in outSocketadrr) {
	char recvbuf[DEFAULT_BUFLEN] = "";
	while (int clientRecv = recv(sockfd, recvbuf, sizeof(recvbuf), 0)) {
		if (clientRecv == 0) {
			perror("client closed ...");
			exit(-1);
		}
		else if (clientRecv == -1) {
			perror("recv");
			exit(-1);
		}
		char* clientIp = inet_ntoa(outSocketadrr.sin_addr);
		int clientPort = ntohs(outSocketadrr.sin_port);
		printf("来自: ip %s port %d , 接收到的信息：%s\n", clientIp, clientPort, recvbuf);

		char sendbuf[] = "你好，刚刚接受到了你的消息!";
		int senlen = strlen(sendbuf) + strlen(recvbuf) + 1;
		memcpy(sendbuf + strlen(sendbuf), recvbuf, senlen);
		send(sockfd, sendbuf, senlen, 0);
	}
	;



}

int main(int argc, char* argv[])
{
	if (argc <=2) {
		printf("参数必须包含ip 和 port");
		return 0;
	}
	char * ip = argv[1];
	int port = atoi(argv[2]);// ascii to intege
	// ipv4  tcp  0
	int socketServer = socket(AF_INET, SOCK_STREAM, 0);//这里最好变量名是listenfd 确实是用来监听的
	struct sockaddr_in socketadrr;
	//TO 1.也可以不用memset填空
	socketadrr.sin_family = AF_INET;
	socketadrr.sin_port = htons(port);  //host to n s
	inet_pton(AF_INET, ip, &socketadrr.sin_addr.s_addr);
	//绑定 socket  ip,port  sizeof
	bind(socketServer, (sockaddr*)&socketadrr, sizeof(sockaddr_in));  //先给程序员使用再强制转化成操作系统用的
	//1.这里可以不用(struct sockaddr*)
	//监听
	listen(socketServer, SOMAXCONN);
	int acceptSocket{};
	while (1) {
	
	struct sockaddr_in outSocketadrr;
	socklen_t outSocketaddrlen = sizeof(sockaddr_in); //socklen =
	//socket 接受的 ip,port sizeof的地址;感觉用不到确实可以设置为NULL
	acceptSocket = accept(socketServer, (struct sockaddr*)&outSocketadrr,&outSocketaddrlen);

	int pip = fork();
	if (pip == 0) {
		//我们在socket编程中调用 close 关闭已连接描述符时，其实只是将访问计数值减 1。而描述符只在访问计数为 0 时才真正关闭。所以为了正确的关闭连接，当调用 fork 函数后父进程将不需要的 已连接描述符关闭，而子进程关闭不需要的监听描述符。
		close(socketServer);//子进程关闭监听套接字
		echo_clinet(acceptSocket, outSocketadrr);
		exit(0);
	}

	}


	close(acceptSocket); 
	close(socketServer);
	return 0;
}
