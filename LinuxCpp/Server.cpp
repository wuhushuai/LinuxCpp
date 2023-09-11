#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>//assert
#include <unistd.h>
#include <arpa/inet.h>//=#include <sys/types.h> + #include <sys/socket.h>
#include <errno.h>
#include <sys/select.h>
#define DEFAULT_BUFLEN 256




int main() {//int argc , char * argv[]

	char recvBuf[DEFAULT_BUFLEN] = { 0 };
	char sendBuf[DEFAULT_BUFLEN] = { 0 };
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	bind(lfd, (sockaddr*)&serverAddr, sizeof(sockaddr));
	listen(lfd, 8);
	
	fd_set  readfds,tmp; //fd_set是一个集合，fd_set的size值在linux下一般被定义为1024
	FD_ZERO(&readfds);
	FD_SET(lfd, &readfds);
	int maxfd = lfd;
	//readfds要留着作为哪些被监控的描述符集合，tmp 是相应收到改变的文件描述符；
	while (1)
	{	//select自己不会创建出cfd，与客户端连接。作用就是监听
		//相当于多了个秘书，客户端都问他，最后通知总经理
		tmp = readfds;
		int ret = select(maxfd + 1, &tmp, NULL, NULL, NULL);
		//参数一  最大索引+1，生成数组的时候是0-maxfd,所以maxfd相对来说要+1才有索引
		//参数二  检测描述符读的集合 //三、写的集合 四、设置异常 五、超时时间，这里NULL是阻塞的
		//返回  -1 : 失败  ，正常返回大于0，几就代表多少个文件描述符发生了变化  0就是超时后的结果，这里不会
		if (ret > 0) {
			//对新连接建立的描述符操作：
			if (FD_ISSET(lfd,&tmp)) { //检测监听描述符是否被置1了，置1说明有新客户端链接，我们就要accept
				printf("client Get lfd = %d\n", lfd);
				sockaddr_in clientaddr;
				bzero(&clientaddr, sizeof(sockaddr_in));
				socklen_t len = sizeof(sockaddr);
				int cfd = accept(lfd, (sockaddr*)&clientaddr, &len);
				FD_SET(cfd, &readfds); //readfds得到改变，说明这个文件描述符需要被检测
				maxfd = cfd > maxfd ? cfd : maxfd;
			}
			//对被监听的描述符进行操作,为什么lfd+1，应为lfd最先被创建是最小的，而且要跳过他，因为已经操作过他了：FD_ISSET(lfd,&tmp)
			for (int i = lfd +1; i <= maxfd ; i++)//刚好处理到maxfd就好了
			{
				if (FD_ISSET(i, &tmp)) {//判断变化后的readfds,也就是tmp
					int recvLen = recv(i, recvBuf, sizeof(recvBuf), NULL);
					if (recvLen > 0) {
						printf("client msg :%s\n", recvBuf);
					}
					else if(recvLen == 0)
					{
						printf("client Out fd = %d", i);
						printf("client close...\n");
						close(i);
						FD_CLR(i, &readfds);//更新要检测的文件描述符
					}
					else
					{
						perror("recv error ");
						close(lfd); close(i);
						exit(-1);
					}
					send(i, recvBuf, strlen(recvBuf) + 1, 0);

				}
			}
		}
		else { perror("select error "); close(lfd); exit(-1); }




	}



	close(lfd);
	return 0;

}

