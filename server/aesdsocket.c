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
#include <stdbool.h>


#define PORT "9000"
#define BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"


bool sig_quit = false;


void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// void signal_handler(int n)
void signal_handler()
{
	syslog(LOG_INFO, "Caught signal, exiting");
	sig_quit = true;
}

void send_data_to_client(int sockfd)
{
	int fd = open(DATA_FILE_PATH, O_RDWR|O_CREAT|O_APPEND, 0600);
	if(fd < 0)
	{
		error("send_data_to_client, open function error...");
	}
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	// n = read(sockfd, buffer, BUFFER_SIZE);
	// if(n < 0)
	// 	error("Error on read");

	ssize_t bytes_received;
	bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
	if(bytes_received < 0)
	{
		error("error send data to client...");
	}
	buffer[bytes_received] = '\0';
	if( (write(fd, buffer, bytes_received)) < 0 )
	{
		error("send_data_to_client, write function error...");
	}

	if( (close(fd)) < 0)
	{
		error("send_data_to_client, close function error...");
	}
}


int main(int argc, char *argv[]) // will uncomment later
{
	// int demon_mode = 0;

	if( argc == 2 )
	{
		if(strcmp(argv[1], "-d") != 0)
		{
			error("Wrong parameter inputted, try -d for daemon mode");
		}
		// demon_mode = 1;
	}


	int sockfd, newsockfd, portno;
	// char buffer[BUFFER_SIZE];

	struct sockaddr_in serv_addr;
	// socklen_t clilen;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	portno = atoi(PORT);


	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if( bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
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


	struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action) );
    new_action.sa_handler = signal_handler;

    if( sigaction(SIGINT, &new_action, NULL) < 0 || sigaction(SIGTERM, &new_action, NULL) < 0 )
    {
    	error("sigaction failed...");
    }


    while( sig_quit == false )
    {
    	struct sockaddr_in cli_addr;
    	socklen_t clilen;
    	clilen = sizeof(cli_addr);
    	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
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


		send_data_to_client(newsockfd);


    }







	// int fd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT|O_APPEND, 0600);







}