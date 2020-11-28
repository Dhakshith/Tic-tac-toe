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

#define SERVER_PORT 80
#define MAXNAME 32
#define MAXLINE (7 + MAXNAME + 1)
#define FORMAT_SPECIFICATION_HELPER(WN) ("%" #WN "s")
#define FORMAT_SPECIFICATION(W) FORMAT_SPECIFICATION_HELPER(W)

#define realerr(str, ...) do {\
	fprintf(stderr, str "\nError Code: %d\nError message: %s\n", ## __VA_ARGS__, errno, strerror(errno));\
	exit(1);\
} while (0)

#define opperr() do {\
	printf("Opponent got disconnected\n");\
	exit(0);\
} while (0)

char onlyletnumsym(uint8_t const *, int);
void printBoard(uint16_t, uint16_t);
char accessBoardAt(uint16_t, char);
char scanMove(char *, char *, uint16_t, uint16_t);

int main(void) {
	int sockfd, n, sendbytes;
	struct sockaddr_in servaddr;
	uint8_t sendline[MAXLINE], recvline[MAXLINE], usrname[MAXNAME + 1], oppusrname[MAXNAME + 1];
	uint16_t XBoard = 0, OBoard = 0;
	char Y, X, xtomove, iamx;

	printf("Enter your name (Can only contain A-Z, a-z, 0-9, ~!@#$%%^&*-+_ and no spaces, with a maximum of %d characters):\n", MAXNAME);
	for (;;) {
		scanf(FORMAT_SPECIFICATION(MAXNAME), usrname);
		if (getchar() != '\n' || !onlyletnumsym(usrname, strlen(usrname))) {
			printf("Your name can only contain A-Z, a-z, 0-9, ~!@#$%%^&*-+_ and no spaces, with a maximum of %d characters)\nPlease enter your name again:\n", MAXNAME);
			scanf("%*[^\n]%*c");
		} else
			break;
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		realerr("Socket() failed");

	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERVER_PORT);

	inet_pton(AF_INET, inet_ntoa(gethostbyname("ticktacktoe.herokuapp.com")->h_addr), &servaddr.sin_addr);

	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof servaddr) == -1)
		realerr("Connect() failed");

	sprintf(sendline, "search %s", usrname);
	sendbytes = strlen(sendline);
	if (write(sockfd, sendline, sendbytes) != sendbytes)
		realerr("Write() failed");

	memset(recvline, 0, MAXLINE);
	if ((n = recv(sockfd, recvline, MAXLINE, 0)) == -1)
		realerr("Read() falied");

	if (n <= 1 || (*recvline != 'X' && *recvline != 'O'))
		opperr();

	memcpy(oppusrname, recvline + 1, n);
	oppusrname[n - 1] = '\0';

	printf("Your opponent is %s\n", oppusrname);

	iamx = *recvline == 'X';
	xtomove = 1;
	for (;;) {
		printBoard(XBoard, OBoard);

		if (xtomove == iamx) {
			char ind = scanMove(&X, &Y, XBoard, OBoard);

			if (iamx)
				XBoard |= 1 << (15 - ind);
			else
				OBoard |= 1 << (15 - ind);
			
			sprintf(sendline, "%c", ind + '0');
			if (write(sockfd, sendline, 1) != 1)
				realerr("Write() failed");
		} else {
			printf("Waiting for %s to make a move...\n", oppusrname);
			if ((n = recv(sockfd, recvline, MAXLINE, 0)) == -1)
				realerr("Read() failed");

			if (isdigit(*recvline)) {
				if (iamx)
					OBoard |= 1 << (15 + '0' - *recvline);
				else
					XBoard |= 1 << (15 + '0' - *recvline);
			} else if (*recvline == 'S') {
				printBoard(XBoard, OBoard);
				printf("Stalemate!\n");
				exit(0);
			} else if (*recvline == 'W') {
				printBoard(XBoard, OBoard);
				printf("You Won!\n");
				exit(0);
			} else if (*recvline == 'L') {
				printBoard(XBoard, OBoard);
				printf("%s Won!\n", oppusrname);
				exit(0);
			} else
				opperr();
		}

		xtomove = !xtomove;
	}

	return 0;
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

void printBoard(uint16_t XBoard, uint16_t OBoard) {
	printf("%c|%c|%c\n-+-+-\n%c|%c|%c\n-+-+-\n%c|%c|%c\n",
		((XBoard >> 15) & 1) ? 'X' : ((OBoard >> 15) & 1) ? 'O' : ' ',
		((XBoard >> 14) & 1) ? 'X' : ((OBoard >> 14) & 1) ? 'O' : ' ',
		((XBoard >> 13) & 1) ? 'X' : ((OBoard >> 13) & 1) ? 'O' : ' ',
		((XBoard >> 12) & 1) ? 'X' : ((OBoard >> 12) & 1) ? 'O' : ' ',
		((XBoard >> 11) & 1) ? 'X' : ((OBoard >> 11) & 1) ? 'O' : ' ',
		((XBoard >> 10) & 1) ? 'X' : ((OBoard >> 10) & 1) ? 'O' : ' ',
		((XBoard >>  9) & 1) ? 'X' : ((OBoard >>  9) & 1) ? 'O' : ' ',
		((XBoard >>  8) & 1) ? 'X' : ((OBoard >>  8) & 1) ? 'O' : ' ',
		((XBoard >>  7) & 1) ? 'X' : ((OBoard >>  7) & 1) ? 'O' : ' ');
}

char accessBoardAt(uint16_t board, char index) {
	return (board >> (15 - index)) & 1;
}

char scanMove(char *X, char *Y, uint16_t XBoard, uint16_t OBoard) {
	while (1) {
		printf("Enter your move in the format YX where Y can be one of (t, c, b) denoting the (top, center, bottom) and X can be one of (l, c, r) denoting the (left, center, right):\n");
		scanf("%c%c%*[^\n]", Y, X);
		getchar();

		if ((*Y == 'T' || *Y == 't' || *Y == 'C' || *Y == 'c' || *Y == 'B' || *Y == 'b')
		&&  (*X == 'L' || *X == 'l' || *X == 'C' || *X == 'c' || *X == 'R' || *X == 'r')) {
			char ind = ((*Y == 't' || *Y == 'T') ? 0 : (*Y == 'C' || *Y == 'c') ? 1 : 2) * 3
					 + ((*X == 'l' || *Y == 'L') ? 0 : (*X == 'C' || *X == 'c') ? 1 : 2);
			if (accessBoardAt(XBoard, ind) || accessBoardAt(OBoard, ind))
				printf("The space is already occupied\n");
			else
				return ind;
		}
	}

	return 0;
}