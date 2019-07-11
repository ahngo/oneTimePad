/*********************************************************************
** otpshared.h
** Description: Function prototypes for one time pad programs and daemons.
*********************************************************************/

#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/ioctl.h>

#define CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ "
#define MAX_MSG_SIZE 150000

void error(const char *msg);
char* processFile(FILE* fp);
int validateStr(char* str);
void sendMsg(char* buffer, int socketFD);
void recMsg(char* buffer, int bytesToReceive, int socketFD);
int getAck(int socketFD);
void sendAck(int socketFD);