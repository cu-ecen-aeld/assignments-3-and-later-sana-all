#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <errno.h>


#define PORT "9000"
#define BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"


void error(const char *msg)
{
	perror(msg);
	exit(1);
}


int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, n;
	char buffer[BUFFER_SIZE];

	struct sockaddr_in serv_addr, cli_addr;
	// socklen_t clilen;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int portno = atoi(PORT);


	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if( bind(sockfd), (struct sockaddr*) &serv_addr, sizeof(serv_addr) < 0)
	{
		error("binding failed...");
	}

	// listen(sockfd, 5);
	if(listen(sockfd,5) < 0)
	{
		// freeaddrinfo(serv_info);
  //       close(my_socket);
  //       closelog();
		error("listening failed...");
	}


	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, sizeof(cli_addr));
	if(newsockfd < 0)
	{
		error("accepting failed...");
	}


	char client_ip[INET_ADDRSTRLEN]; // for ipv6 client_ip[INET6_ADDRSTRLEN];
	// if( getnameinfo((struct sockaddr *) &cli_addr, sizeof(cli_addr) ) ) // i commented this because getnameinfo is very very long
	if( inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL )
	{
		error("inet_ntop failed...");
	}
	syslog(LOG_INFO,  "Accepted connection from %s", client_ip);


	int fd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT|O_APPEND, 0600);




}