/*********************************************************************
** oneTimePadEncryptClient.c
** Description: Given a textfile (presumably plaintext) and a OTP key
* file, encodes the textfile with the key. Acts as a client to otp_enc_d
* by sending the plaintext and key text. Receives the encoded text
* from otp_enc_d and prints it to stdout. 
* Uses processFile() and validateStr() from otpshared.c
* Usage: otp_enc [plaintext] [key] [port]
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <sys/ioctl.h>
#include "otpshared.h"

#define SECRETCODESIZE 16 // contains secret_key and incoming msg size
#define END_CODE_MSG_SIZE 3 // stops receiving messages when this code is found
#define END_CODE "@@" // size of the above, including null terminator
#define KEY_DELIMITER "@ENC@" 
#define END_DELIMITER "@END@" // this delimiter lets the server know to stop recv
#define DELIMITER_LEN 5 // length of the above
#define SECRET_KEY "ENC" // authentication key to server

/*
   Summary: Processes plaintext and key files, verifies validity of both
   files and reads them into buffers. Authenticates itself with the server
   as well as sending message size of both plaintext/key over to the server,
   along with string delimiters to let the server know where the key starts
   and where the message ends. Receives back ciphered text delimited with an
   end code and prints the ciphered text to stdout. 
*/

int main(int argc, char *argv[])
{
    // Check usage & args
	if (argc < 4) { fprintf(stderr,"USAGE: %s [plaintext file] [key file] port\n", argv[0]); exit(0); }
	
	FILE *fp;
    char* buffer;   // buffer holds plaintext
    char* keybuffer; // keybuffer holds key
    
    // Process and validate the plaintext and key files
    fp = fopen(argv[1], "r");
    if (fp)
    {
        // Plaintext is placed into buffer, which is allocated memory by processFile()
        buffer = processFile(fp);
        // validateStr searches for invalid characters
        if (validateStr(buffer) == 0)
        {
            fprintf(stderr, "%s contains invalid chars\n", argv[1]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Invalid filename\n");
        return 1;
    }
    // Do the same  as above for the key file
    fp = fopen(argv[2], "r");
    if (fp)
    {
        keybuffer = processFile(fp);
        if (validateStr(keybuffer) == 0)
        {
            fprintf(stderr, "Key contains invalid chars\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Invalid key\n");
        return 1;
    }

    // Ensure the key is long enough to encode the plaintext
    if (strlen(keybuffer) < strlen(buffer))
    {
        fprintf(stderr, "Key is too short to fully encrypt message\n");
        return 1;
    }
    
    int socketFD, portNumber;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	
	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0) error("CLIENT: ERROR opening socket");
	
	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
		error("CLIENT: ERROR connecting");
	
    // Allocate memory for and create a string from the plaintext and key
    // Full string has a key delimiter and ending delimiter, as well as a null char
    int fullStrLen = strlen(buffer) + DELIMITER_LEN + strlen(buffer) + DELIMITER_LEN + 1;
    int msgLen = strlen(buffer);
    char* fullStr = malloc(fullStrLen);
    memset(fullStr, '\0', fullStrLen);
    strcat(fullStr, buffer);
    // otp_enc_d will look for "@ENC" to start parsing the key
    strcat(fullStr, KEY_DELIMITER);
    // fullStr contains (plaintext)@ENC@(key)@END@
    strncat(fullStr, keybuffer, msgLen);
    strcat(fullStr, END_DELIMITER);
    
    // Create a code to send to the server. The code lets it know it is from
    // otp_enc and how big of a message to expect.
	fullStrLen = strlen(fullStr);
	char code[] = SECRET_KEY;
	char encSize[SECRETCODESIZE];
	memset(encSize, '\0', sizeof(encSize));
	// Convert size of the plaintext from a number to a string
	sprintf(encSize, "%i", fullStrLen);
	// Build up the code
	char codeSize[SECRETCODESIZE];
	memset(codeSize, '\0', sizeof(codeSize));
	strcat(codeSize, code);
	strcat(codeSize, encSize);
	// Send it
	sendMsg(codeSize, socketFD);
	//printf("Sent codeSize: %s\n", codeSize);
	fflush(stdout);
	
	// Get ack/confirmation from server that further transmissions are okay
	int confirm = getAck(socketFD);
	if (confirm == 1)
	{
		// Send the string to otp_enc_d
		sendMsg(fullStr, socketFD);
		// Get transmission length (msg length + ending code)
		int enclen = (msgLen + END_CODE_MSG_SIZE);
		// Place received message back from server in buffer
		char buf[MAX_MSG_SIZE];
		memset(buf, '\0', sizeof(buf));
		recMsg(buf, enclen, socketFD);
		// Strip off ending code
		buf[strcspn(buffer, END_CODE)] = '\0';
		printf("%s\n", buf);
	}
	
	// Shut socket from any further transmissions
	shutdown(socketFD, SHUT_RDWR);
	// Clean up allocated memory
    free(buffer);
    free(keybuffer);
    free(fullStr);
	close(socketFD); // Close the socket
	return 0;
}
