/*
 File: client.c
 PS: The first client connected to the server is called client1, and the second client connected to the server is called client2.
 The function of this program is: first connect to the server, according to the return of the server to determine whether it is client1 or client2,
 If it is client1, it gets the IP and Port of client2 from the server and connects to client2.
 If it is client2, it gets the IP and Port of client1 and its converted port from the server.
 After trying to connect client1 (this operation will fail), and then listen according to the port returned by the server.
 This way, peer-to-peer communication between the two clients is possible.
*/
 
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
 
#define MAXLINE 128
#define SERV_PORT 8877
 
typedef struct
{
	char ip[32];
	int port;
}server;
 
 //A fatal error occurred, exiting the program
void error_quit(const char *str)    
{    
	fprintf(stderr, "%s", str); 
	 / / If the error number is set, enter the cause of the error
	if( errno != 0 )
		fprintf(stderr, " : %s", strerror(errno));
	printf("\n");
	exit(1);    
}   
 
int main(int argc, char **argv)     
{          
	int i, res, port;
	int connfd, sockfd, listenfd; 
	unsigned int value = 1;
	char buffer[MAXLINE];      
	socklen_t clilen;        
	struct sockaddr_in servaddr, sockaddr, connaddr;  
	server other;
 
	if( argc != 2 )
		error_quit("Using: ./client <IP Address>");
 
	 / / Create a socket for the link (the main server)        
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	memset(&sockaddr, 0, sizeof(sockaddr));      
	sockaddr.sin_family = AF_INET;      
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
	sockaddr.sin_port = htons(SERV_PORT);      
	inet_pton(AF_INET, argv[1], &sockaddr.sin_addr);
	 / / Set the port can be reused
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
 
	 / / Connect to the main server
	res = connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)); 
	if( res < 0 )
		error_quit("connect error");
 
	 / / Read the information from the main server
	res = read(sockfd, buffer, MAXLINE);
	if( res < 0 )
		error_quit("read error");
	printf("Get: %s", buffer);
 
	 / / If the server returns first, it proves to be the first client
	if( 'f' == buffer[0] )
	{
		 / / Read the second client's IP + port from the server
		res = read(sockfd, buffer, MAXLINE);
		sscanf(buffer, "%s %d", other.ip, &other.port);
		printf("ff: %s %d\n", other.ip, other.port);
 
		 / / Create a socket for        
		connfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&connaddr, 0, sizeof(connaddr));      
		connaddr.sin_family = AF_INET;      
		connaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		connaddr.sin_port = htons(other.port);    
		inet_pton(AF_INET, other.ip, &connaddr.sin_addr);
 
		 / / Try to connect to the second client, the first few may fail, because the penetration has not been successful,
		 / / If the connection fails 10 times, it proves that the penetration failed (may be hardware does not support)
		while( 1 )
		{
			static int j = 1;
			res = connect(connfd, (struct sockaddr *)&connaddr, sizeof(connaddr)); 
			if( res == -1 )
			{
				if( j >= 10 )
					error_quit("can't connect to the other client\n");
				printf("connect error, try again. %d\n", j++);
				sleep(1);
			}
			else 
				break;
		}
 
		strcpy(buffer, "Hello, world\n");
		 //After the connection is successful, send a hello to the other party (client 2) every second.
		while( 1 )
		{
			res = write(connfd, buffer, strlen(buffer)+1);
			if( res <= 0 )
				error_quit("write error");
			printf("send message: %s", buffer);
			sleep(1);
		}
	}
	 / / The behavior of the second client
	else
	{
		 //Remove the IP+port of client 1 and the port mapped by its own public network from the information returned by the primary server.
		sscanf(buffer, "%s %d %d", other.ip, &other.port, &port);
 
		 / / Create a socket for the TCP protocol        
		sockfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&connaddr, 0, sizeof(connaddr));      
		connaddr.sin_family = AF_INET;      
		connaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		connaddr.sin_port = htons(other.port);      
		inet_pton(AF_INET, other.ip, &connaddr.sin_addr);
		 / / Set port reuse
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
 
		 //Trying to connect to client 1 will definitely fail, but it will leave a record on the router.
		 / / To help the client 1 successfully penetrate, connect yourself 
		res = connect(sockfd, (struct sockaddr *)&connaddr, sizeof(connaddr)); 
		if( res < 0 )
			printf("connect error\n");
 
		 / / Create a socket for listening        
		listenfd = socket(AF_INET, SOCK_STREAM, 0); 
		memset(&servaddr, 0, sizeof(servaddr));      
		servaddr.sin_family = AF_INET;      
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      
		servaddr.sin_port = htons(port);
		 / / Set port reuse
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
 
		 / / Link the socket and socket address structure 
		res = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));    
		if( -1 == res )
			error_quit("bind error");
 
		 / / Start listening port       
		res = listen(listenfd, INADDR_ANY);    
		if( -1 == res )
			error_quit("listen error");
 
		while( 1 )
		{
			 / / Receive the connection from client 1
			connfd = accept(listenfd,(struct sockaddr *)&sockaddr, &clilen);  
			if( -1 == connfd )
				error_quit("accept error");
 
			while( 1 )
			{
				 / / Loop to read information from client 1
				res = read(connfd, buffer, MAXLINE);
				if( res <= 0 )
					error_quit("read error");
				printf("recv message: %s", buffer);
			}
			close(connfd);
		}
	}
 
	return 0;
}