/*********************************************************************
** otpshared.c
** Description: Shared functions between one time pad programs and
* daemon programs.
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
#include <unistd.h>
#include "otpshared.h"

void error(const char *msg) { perror(msg); exit(0); } // Error function used for reporting issues

/********************************************************************* 
** validateStr()
* Given a string, iterates through the string to ensure the characters
* are all capital letters. Returns 0 if it finds a non-capital letter
* or non-alphanumeric character that is not space (' ').
*********************************************************************/

int validateStr(char* str)
{
    int strLength = strlen(str);
    int i;
    for (i = 0; i < strLength; i++)
    {
        // Searching for characters by ASCII range
        if (str[i] < 'A' || str[i] > 'Z')
        {
            // if the char is space, go on
            if (str[i] == 32)
            {
                continue;
            }
            else 
            {
                return 0;
            }
        }
    }
    return 1;
}

/********************************************************************* 
** processFile()
* Given a valid file pointer, uses fseek() and ftell()
* to get the length of the file and malloc a buffer large enough to fit
* the contents of the file. Returns a pointer to the buffer.
* Memory allocated here should be freed in main.
*********************************************************************/

char* processFile(FILE* fp)
{   
    // go to the end of the file and count how many bytes from the beginning
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    // make a buffer big enough for the file
    char* strbuffer;
    strbuffer = malloc(length * sizeof(char));
    memset(strbuffer, '\0', length);
    // go to the beginning of the file and read the file into the buffer
    fseek(fp, 0, SEEK_SET);
    fread(strbuffer, 1, length, fp);
    fclose(fp);
    // strip off newlines
    strbuffer[strcspn(strbuffer, "\n")] = '\0';
    return strbuffer;
}

/********************************************************************* 
** sendMsg(): Given a buffer and a valid socket file descriptor,
* sendMsg() ensures all data in the buffer is sent through the socket.
* Ensures all data is sent before proceeding by using ioctl.
*********************************************************************/

void sendMsg(char* buffer, int socketFD)
{
    // current iteration of send()
    int charsSent = 0;
    // total count of chars that have gone through send()
    int totalCharsSent = 0;
    // max data to send per send call
    int maxSendBytes = 1000;
    // remaining data to send
    int remainingBytes = strlen(buffer);
    // bytesToSend = full amount of data
    int bytesToSend = remainingBytes;
    // update maxSendBytes to bytesToSend when it below 1000
    if (bytesToSend < maxSendBytes)
    {
        maxSendBytes = bytesToSend;
    }
    
    // Save buffer pointer so that send can resume printing to the
    // right place if interrupted
    char* ptr = buffer;
    while (totalCharsSent < bytesToSend)
    {
        charsSent = send(socketFD, ptr, maxSendBytes, 0);
        // Error handling
        if (charsSent < 0)
        {
            error("ERROR writing to socket");
        }
        else if (charsSent == 0)
        {
            error("Unexpected EOF");
        }
        // Update total and ptr as needed
        else
        {
            totalCharsSent += charsSent;
            ptr += charsSent;
        }
    	// Update maxSendBytes as needed so that erroneous data is not sent
    	remainingBytes = bytesToSend - totalCharsSent;
    	if (remainingBytes < maxSendBytes && totalCharsSent < bytesToSend)
    	{
    	    maxSendBytes = remainingBytes;
    	}
    }
    // verified sending
    int checkSend = -5;  // Holds amount of bytes remaining in send buffer
	do
	{
		ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
	}
	while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
	if (checkSend < 0)
	{
	    error("ioctl error");
	}
}

/********************************************************************* 
** recMsg(): Given a buffer and a valid socket file descriptor, and
* a message size, receives a specific amount of data and places it in
* the buffer. Very similar to sendMsg, but calls recv() until
* an end code is found in the buffer. Ideally, the function
* should "time out" after enough calls to recv and no end code is found,
* since it will block indefinitely on bad messages.
*********************************************************************/

void recMsg(char* buffer, int bytesToReceive, int socketFD)
{
    // bytes rec'd from current iteration of recv
    int charsRec = 0;
    // total bytes received across all iterations
    int totalCharsRec = 0;
    // max data to receive at a time
    int maxRecBytes = 1000;
   
    if (bytesToReceive < maxRecBytes)
    {
        maxRecBytes = bytesToReceive;
    }
    
    char* ptr = buffer;
    while (totalCharsRec < bytesToReceive && strstr(buffer, "@END@") == NULL)
    {
        charsRec = recv(socketFD, ptr, maxRecBytes, 0);
        if (charsRec < 0)
        {
            error("ERROR receiving from socket");
        }
        
        else if (charsRec == 0)
        {
            error("RECMSG: Unexpected EOF");
        }
        else 
        {
            totalCharsRec += charsRec;
            ptr += charsRec;
        }
        
    	int remainingBytes = bytesToReceive - totalCharsRec;
    	if (remainingBytes < maxRecBytes && totalCharsRec < bytesToReceive)
    	{
    	    maxRecBytes = remainingBytes;
    	}
    	
    }
}

/********************************************************************* 
** getAck()
* Given a valid socket file descriptor, waits for an acknowledgement statement.
* Typically used to ensure the other side is ready to continue, otherwise
* send() and recv() calls may fall out of synchronization.
* Waits two seconds for an ack, otherwise it returns 0 and the programs
* using getAck() time out.
*********************************************************************/

int getAck(int socketFD)
{
	char ack[3];
	memset(ack, '\0', sizeof(ack));
	
	int seconds = 0;
	while ((strstr(ack, "ACK")) == NULL)
	{
	    sleep(1);
	    recv(socketFD, ack, sizeof(ack) + 1, 0);
	    seconds++;
	    if (seconds > 2)
	    {
	        return 0;
	    }
	}
	return 1;
}

/********************************************************************* 
** sendAck()
* Counterpart to getAck(). Used to send an acknowledgement statement
* to tell the other side that it is ready to receive or send more data.
*********************************************************************/

void sendAck(int socketFD)
{
    char ack[] = "ACK";
	send(socketFD, ack, sizeof(ack), 0);
	fflush(stdout);
}