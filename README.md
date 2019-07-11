# oneTimePadProject

The oneTimePad (OTP) Project is a series of files designed to explore some networking (specifically TCP) capabilities of C, designed around the one-time pad encryption technique. 

In short, one program is designed to act as a server or daemon, and the other program is a client. The client sends plaintext and a key to the server program, and the server program returns ciphered text depending on the key, assuming the key is long enough to cipher the entire text. 

Specifics: the server program should be run on an available port. These files are designed specifically to run on a UNIX system. The client is then invoked with the proper port. There are some "mock" techniques used, such as the client sending a specific code to the server to let it know it is from the client and not any other program. There is also an acknowledgement technique (ACK) sent from the server to the client to let it know it is authenticated and ready to accept text.
