#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_BUFLEN 256
#define CLIENT_MAX 1024

int main() {

	char recvBuf[DEFAULT_BUFLEN] = { 0 };
	char sendBuf[DEFAULT_BUFLEN] = { 0 };

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(lfd != -1);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(lfd, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
		perror("bind failed");
		close(lfd);
		return 1;
	}
	assert(listen(lfd, 8) != -1);

	pollfd fds[CLIENT_MAX];
	for (int i = 0; i < CLIENT_MAX; i++) {
		fds[i].fd = -1;
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}
	fds[0].fd = lfd;

	int fdsMaxIndex = 0;

	while (1) {
		int ret = poll(fds, fdsMaxIndex + 1, -1);
		assert(ret != -1);

		if (fds[0].revents & POLLIN) {
			sockaddr_in clientaddr;
			bzero(&clientaddr, sizeof(sockaddr_in));
			socklen_t len = sizeof(sockaddr);
			int cfd = accept(lfd, (sockaddr*)&clientaddr, &len);
			printf("client Get cfd = %d\n", cfd);

			int i;
			for (i = 1; i < CLIENT_MAX; i++) {
				if (fds[i].fd == -1) {
					fds[i].fd = cfd;
					fds[i].events = POLLIN;
					fdsMaxIndex = fdsMaxIndex > i ? fdsMaxIndex : i;
					break;
				}
			}

			if (i == CLIENT_MAX) {
				fprintf(stderr, "too many clients\n");
				close(cfd);
			}
		}

		for (int i = 1; i <= fdsMaxIndex; i++) {
			if (fds[i].revents & POLLIN) {
				memset(recvBuf, 0, sizeof(recvBuf));
				int recvLen = recv(fds[i].fd, recvBuf, sizeof(recvBuf), 0);
				if (recvLen > 0) {
					printf("client msg: %s\n", recvBuf);
					send(fds[i].fd, recvBuf, strlen(recvBuf) + 1, 0);
				}
				else if (recvLen == 0) {
					printf("client Out fd = %d\n", i);
					printf("client close...\n");
					close(fds[i].fd);
					fds[i].fd = -1;
				}
				else {
					perror("recv error");
					close(lfd);
					close(fds[i].fd);
					exit(-1);
				}
			}
		}
	}

	close(lfd);
	return 0;
}