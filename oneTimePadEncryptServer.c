/*********************************************************************
** oneTimePadDaemon.c
** Description: Acts as a server for one time pad encrytion.
* Receiving a plaintext and key file from otp_enc, encodes the plaintext 
* and sends the ciphered text back to otp_enc. otp_enc sends a code to 
* otp_enc_d to verify it is  from otp_enc.
* Calls fork() to process multiple encryptions.
* Usage: otp_enc_d [port] &
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "otpshared.h"

#define SECRETCODESIZE 16 // max size of secret code + number of bytes to rec
#define SECRETCODE "ENC" // secret key to look for
#define SECRET_MSG_SIZE 3 // size of the secret code
#define KEY_DELIMITER "@ENC@" // receives (plaintext)(KEY_DELIMITER)(key)(END_CODE)
#define DELIMITER_SIZE 5
#define END_CODE "@END@"

/********************************************************************* 
** encode()
* Given a string of plaintext, a key string and a buffer to place
* encrypted text into, encode() encodes the plaintext with the key
* and places encoded text into the provided buffer.
*********************************************************************/

void encode(char* plaintext, char* key, char* encryptedText)
{
    // Indexing available characters (e.g. A starts at 0)
    char charset[] = CHARS;
    
    int a, b, c, i;
    int plaintextLen = strlen(plaintext);
    // Iterate through the plaintext
    for (i = 0; i < plaintextLen; i++)
    {
        // Spaces are the 26th char in charset
        if (plaintext[i] == ' ')
        {
            a = 26;
        }
        // Get the 0-index by subtracting 65 ('A')
        else
        {
            a = plaintext[i] - 65;
        }
        // Do the same for the key text
        if (key[i] == ' ')
        {
            b = 26;
        }
        else
        {
            b = key[i] - 65;
        }
        // Add plaintext to the key and mod it by 27 (e.g. 26 is space, 27%27=0 or A)
        c = a + b;
        c = (c % 27);
        encryptedText[i] = charset[c];
    }
}

/* Summary: Listens for connections on the given port. Upon successful connection
   creates a child process to verify that a transmission from the correct client.
   Rejects connections from other clients. Upon verification, acknowledges client
   and accepts a transmission of one string: Plaintext + KEY_DELIMITER + key +
   END_CODE. Null terminates the plaintext, string-copies the key to its own 
   buffer, then calls encode() to place ciphered text in a new buffer. Sends
   back the ciphered text with an END_CODE that lets the client know the message
   is over. */
   
int main(int argc, char *argv[])
{
    // Server variables
	int listenSocketFD, establishedConnectionFD, portNumber;
	socklen_t sizeOfClientInfo;
	struct sockaddr_in serverAddress, clientAddress;

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections
    
	while (1)
	{
	    // Clear children as needed
	    int status;
	    waitpid(-1, &status, WNOHANG);
	    
	    // Accept a connection, blocking if one is not available until one connects
    	sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
    	establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
    	if (establishedConnectionFD < 0) error("ERROR on accept");
    
        int pid = fork();
        // if fork failed
        if (pid < 0)
        {
            close(establishedConnectionFD);
            fprintf(stderr, "fork() returned error\n");
            continue;
        }
        // child processes this code
        else if (pid == 0)
        {
            char msgBuffer[MAX_MSG_SIZE];
        	// Clear the buffer 
        	memset(msgBuffer, '\0', MAX_MSG_SIZE);
        	// Receive text from otp_enc until the transmission is over
        	char msgSize[SECRETCODESIZE];
        	memset(msgSize, '\0', sizeof(msgSize));
        	
        	// Get first message (secret key plus message size)
    	    recv(establishedConnectionFD, msgSize, SECRETCODESIZE, 0);
        	//printf("Received msgSize: %s\n", msgSize);
        	fflush(stdout);
	        // Look for the secret code, if not found, reject message/close connection. 
    	    if (strstr(msgSize, SECRETCODE) == NULL)
        	{
        	    fprintf(stderr, "otp_enc_d only accepts messages from otp_enc\n");
        	    shutdown(establishedConnectionFD, SHUT_RDWR);
                close(establishedConnectionFD);
                exit(1);
        	}
        	
        	// Extract the number of bytes to read from the coded message above.
        	int bytesToRead = atoi(msgSize + SECRET_MSG_SIZE);
            // Acknowledge that server is ready to receive the message.
        	sendAck(establishedConnectionFD);
        	// Receive message
        	recMsg(msgBuffer, bytesToRead, establishedConnectionFD);
        	//printf("SERVER: I received this from the client: \"%s\"\n", msgBuffer);
        	//fflush(stdout);
        	
        	// Look for where the key starts
        	char* keyStart = strstr(msgBuffer, KEY_DELIMITER);
            // Extract the key, null-terminating the plaintext
            keyStart[0] = '\0';
            char keyBuffer[MAX_MSG_SIZE];
            memset(keyBuffer, '\0', sizeof(keyBuffer));
            // Put the key in its own buffer
            strncpy(keyBuffer, keyStart+DELIMITER_SIZE, strlen(msgBuffer));
            // Create a buffer for the encrypted text and fill it
            char encryptedText[MAX_MSG_SIZE];
            memset(encryptedText, '\0', sizeof(encryptedText));
            encode(msgBuffer, keyBuffer, encryptedText);
            // Put the ending code on so client knows when to stop receiving
            strcat(encryptedText, END_CODE);
            // Send encrypted text over
            sendMsg(encryptedText, establishedConnectionFD);
            // Shut down socket to ensure no more transmissions
            shutdown(establishedConnectionFD, SHUT_RDWR);
        	close(establishedConnectionFD); // Close the existing socket which is connected to the client
        	exit(0);
        }
        // Parent code
        else
        {
            close(establishedConnectionFD);
            // Wait for any finished children
            waitpid(-1, &status, WNOHANG);
        }
    }
	close(listenSocketFD); // Close the listening socket
	return 0; 
}
