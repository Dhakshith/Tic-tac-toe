#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <time.h>

#define SERVER_PORT 80
#define MAXNAME 32
#define MAXLINE (7 + MAXNAME + 1)

#define realerr(str, ...) do {\
	fprintf(stderr, str "\nError Code: %d\nError message: %s\n", ## __VA_ARGS__, errno, strerror(errno));\
	exit(1);\
} while (0)

char onespacer(uint8_t const *, int);
char onlyletnumsym(uint8_t const *, int);
char Compare(uint8_t const *, uint8_t const *);
char boardState(uint16_t, uint16_t);

int main(int argc, char **argv) {
	int listenfd, connfdX, connfdO, sendbytes, n;
	struct sockaddr_in servaddr;
	uint8_t sendline[MAXNAME + 2], recvline[MAXLINE], XName[MAXNAME + 1], OName[MAXNAME + 1];
	char waitingfor = 'X', shifting;
	uint16_t XBoard = 0, OBoard = 0;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		realerr("socket() failed");

	int enable = 1;
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable)) == -1)
		realerr("Setsockopt() failed");
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof enable)) == -1)
		realerr("Setsockopt() failed");

	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof servaddr) == -1)
		realerr("Bind() failed");

	if (listen(listenfd, 10) == -1)
		realerr("Listen() failed");

	for (;;) {
		printf("Waiting for connection on port %d\n", SERVER_PORT);
		fflush(stdout);

		for (char itr = 0; itr < 2; itr++)
			for (;;) {
				if (itr == 0) {
					if ((connfdX = accept(listenfd, NULL, NULL)) == -1)
						realerr("Accept() failed");
				} else {
					if ((connfdO = accept(listenfd, NULL, NULL)) == -1)
						realerr("Accept() failed");
				}

				memset(recvline, 0, MAXLINE);
				if ((n = recv(itr == 0 ? connfdX : connfdO, recvline, MAXLINE, 0)) <= 0)
					continue;

				if (n == MAXLINE || n <= 7 || !Compare(recvline, "search ") || !onespacer(recvline, n) || !onlyletnumsym(recvline + 7, n - 7))
					continue;
				else {
					strncpy(itr == 0 ? XName : OName, recvline + 7, n - 7);
					(itr == 0 ? XName : OName)[n - 7] = '\0';
					break;
				}
			}

		printf("%s vs %s\n", XName, OName);

		XBoard = 0;
		OBoard = 0;

		sprintf(sendline, "X%s", OName);
		sendbytes = strlen(sendline);
		if (write(connfdX, sendline, sendbytes) != sendbytes) {
			write(connfdO, "D", 1);
			close(connfdO);
		}

		sprintf(sendline, "O%s", XName);
		sendbytes = strlen(sendline);
		if (write(connfdO, sendline, sendbytes) != sendbytes) {
			write(connfdO, "D", 1);
			close(connfdO);
		}

		char xtoplay = 1;
		for (;;) {
			memset(recvline, 0, MAXLINE);
			if ((n = recv(xtoplay ? connfdX : connfdO, recvline, MAXLINE, 0)) <= 0) {
				write(xtoplay ? connfdO : connfdX, "D", 1);
				break;
			}

			if (n != 1 || !isdigit(*recvline) || ((XBoard >> (shifting = '0' + 15 - *recvline)) & 1) || ((OBoard >> shifting) & 1)) {
				write(xtoplay ? connfdO : connfdX, "D", 1);
				break;
			}

			if (xtoplay)
				XBoard |= 1 << shifting;
			else
				OBoard |= 1 << shifting;

			if ((shifting = boardState(XBoard, OBoard))) {
				write(connfdX, shifting == 1 ? "S" : shifting == 2 ? "W" : "L", 1);
				write(connfdO, shifting == 1 ? "S" : shifting == 3 ? "W" : "L", 1);

				break;
			}

			if (write(xtoplay ? connfdO : connfdX, recvline, 1) != 1) {
				write(xtoplay ? connfdX : connfdO, "D", 1);
				break;
			}

			xtoplay = !xtoplay;
		}

		close(connfdX);
		close(connfdO);
	}

	return 0;
}

char onespacer(uint8_t const *str, int len) {
	char spaces = 0;

	while (len-- > 0)
		if (*str++ == ' ')
			if (++spaces == 2)
				return 0;

	return 1;
}

char onlyletnumsym(uint8_t const *str, int len) {
	while (len-- > 0)
		if (!isalnum(*str) && *str != '~' && *str != '!' && *str != '@' && *str != '#'
						   && *str != '$' && *str != '%' && *str != '^' && *str != '&'
						   && *str != '*' && *str != '-' && *str != '+' && *str != '_')
			return 0;
		else str++;

	return 1;
}

char Compare(uint8_t const *str, uint8_t const *str2) {
	int i = 0;
	while (*str2)
		if (*str++ != *str2++)
			return 0;
		else i++;

	return 1;
}

/******************/
/* 0 -> Nothing   */
/* 1 -> Stalemate */
/* 2 -> X Won     */
/* 3 -> O Won     */
/******************/
char boardState(uint16_t XBoard, uint16_t OBoard) {
	if ((XBoard & (XBoard >> 1) & (XBoard >> 2) & 0b0010010010000000) || (XBoard & (XBoard >> 3) & (XBoard >> 6) & 0b0000001110000000)
		|| (XBoard & 0b0010101000000000) == 0b0010101000000000 || (XBoard & 0b1000100010000000) == 0b1000100010000000)
		return 2;

	if ((OBoard & (OBoard >> 1) & (OBoard >> 2) & 0b0010010010000000) || (OBoard & (OBoard >> 3) & (OBoard >> 6) & 0b0000001110000000)
		|| (OBoard & 0b0010101000000000) == 0b0010101000000000 || (OBoard & 0b1000100010000000) == 0b1000100010000000)
		return 3;

	if (((XBoard | OBoard) & 0b1111111110000000) == 0b1111111110000000)
		return 1;

	return 0;
}