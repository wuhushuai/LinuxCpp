#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>//assert
#include <unistd.h>
#include <netinet/in.h>//目前没地方用到
#include <arpa/inet.h>//=#include <sys/types.h> + #include <sys/socket.h>
#define DEFAULT_BUFLEN 256

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

	struct sockaddr_in outSocketadrr;
	socklen_t outSocketaddrlen = sizeof(sockaddr_in); //socklen =
	//socket 接受的 ip,port sizeof的地址;感觉用不到确实可以设置为NULL
	int acceptSocket = accept(socketServer, (struct sockaddr*)&outSocketadrr,&outSocketaddrlen);
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	recv(acceptSocket, recvbuf, sizeof(recvbuf), 0); 
	printf("接收到的信息：%s\n", recvbuf);
	char sendbuf[] = "你好，刚刚接受到了你的消息!";
	send(acceptSocket, sendbuf, strlen(sendbuf) + 1, 0);
	close(acceptSocket); 
	close(socketServer);
	return 0;
}
