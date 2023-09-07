#include <stdio.h>
#include <string.h>
#include <netinet/in.h>//目前没地方用到
#include <stdlib.h>
#include <arpa/inet.h>//=#include <sys/types.h> + #include <sys/socket.h>
#include <unistd.h> //close(fd)
#define  ip "127.0.0.1"
#define DEFAULT_BUFLEN 256
int main(int argc, char* argv[])
{

	int ClinSock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in Sockaddr;
	Sockaddr.sin_port = htons( 8080 );
	Sockaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip , &Sockaddr.sin_addr.s_addr);
	connect(ClinSock, (sockaddr*)&Sockaddr,  sizeof(sockaddr_in));
	while (1) {
	
	char sendBuf[DEFAULT_BUFLEN] = "";
	fgets(sendBuf, DEFAULT_BUFLEN,stdin);
	send(ClinSock, sendBuf, strlen(sendBuf) + 1, 0);
	char recvBuf[DEFAULT_BUFLEN]="";
	int iResult = recv(ClinSock, recvBuf, DEFAULT_BUFLEN, 0); //不确定会接受多大的内容
	printf("Bytes received: %d\n", iResult);
	printf("Webserver send : %s\n", recvBuf); 
	}
	close(ClinSock);
	return 0;
}
