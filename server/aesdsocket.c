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
#include <pthread.h>
#include <time.h>
// #include "queue.h"


#define PORT "9000"
#define BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

bool sig_quit = false;

void error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

struct thread_data {
    pthread_mutex_t *mutex;
    int newsockfd;
    int data_fd;
    bool thread_complete_success;
};

void *timestamp_thread(void *arg){
	printf("OLOOOOOOOOOOOOL  timestamp_thread\n");
	struct thread_data *t_data = (struct thread_data *)arg;

	while(sig_quit == false)
	{

		// struct thread_data *t_data = (struct thread_data *)arg;
	    // int newsockfd = t_data->newsockfd;
	    int data_fd = t_data->data_fd;
	    char buffer[BUFFER_SIZE];
	    bzero(buffer, BUFFER_SIZE);
	    ssize_t bytes_received;
	    struct tm *t_ptr;
    	time_t t;


	    
	    sleep(10);
	    t = time(NULL);
	    t_ptr = localtime(&t);
	    strftime(buffer,BUFFER_SIZE, "timestamp:%F %T", t_ptr);

	    bytes_received = strlen(buffer);
	    puts(buffer);
		pthread_mutex_lock(t_data->mutex);
		if (write(data_fd, buffer, bytes_received) < 0) {
	        syslog(LOG_ERR, "handle_client, write function error...");
	        pthread_mutex_unlock(t_data->mutex);
	        break;
	    }
	    pthread_mutex_unlock(t_data->mutex);
	}
	

 

    // close(newsockfd);
    free(t_data);
    return NULL;
}

void *handle_client(void *arg){
	struct thread_data *t_data = (struct thread_data *)arg;
    int newsockfd = t_data->newsockfd;
    int data_fd = t_data->data_fd;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_received;

	// int data_fd = open(DATA_FILE_PATH, O_RDWR|O_CREAT|O_APPEND, 0600);
	// if(data_fd < 0)
	// {
	// 	error("send_data_to_client, open function error...");
	// 	close(data_fd);
	// 	return NULL;
	// 	// continue;
	// }

    while ((bytes_received = recv(newsockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';

        // Lock the mutex before writing to the file
        pthread_mutex_lock(t_data->mutex);
        if (write(data_fd, buffer, bytes_received) < 0) {
            syslog(LOG_ERR, "handle_client, write function error...");
            pthread_mutex_unlock(t_data->mutex);
            break;
        }
        pthread_mutex_unlock(t_data->mutex);

        // Check if the last character is a newline
        if (buffer[bytes_received - 1] == '\n') {
            lseek(data_fd, 0, SEEK_SET);
            char read_buffer[BUFFER_SIZE];
            ssize_t read_bytes;

            // Read the entire content of the file and send it to the client
            while ((read_bytes = read(data_fd, read_buffer, BUFFER_SIZE)) > 0) {
                send(newsockfd, read_buffer, read_bytes, 0);
            }
            lseek(data_fd, 0, SEEK_END);
        }
    }


    if (bytes_received < 0) {
        syslog(LOG_ERR, "error sending data to client...");
    }


	close(data_fd);
	close(newsockfd);
    free(t_data);

	return NULL;
}




void signal_handler()
{
	syslog(LOG_INFO, "Caught signal, exiting");
	sig_quit = true;
}



int main(int argc, char *argv[]) // will uncomment later
{
	int daemon_mode = 0;
	// openlog("aesdsocket", LOG_PID, LOG_USER);
	if( argc == 2 )
	{
		if(strcmp(argv[1], "-d") != 0)
		{
			error("Wrong parameter inputted, try -d for daemon mode");
		}
		daemon_mode = 1;
	}


	int sockfd, newsockfd, portno;
	// char buffer[BUFFER_SIZE];

	struct sockaddr_in serv_addr;


	struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *server_info; // node

    int chapi = getaddrinfo(NULL, PORT, &hints, &server_info);
    if (chapi != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(chapi));
        error("chapi failed...");
    }

	sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	if(sockfd < 0)
	{
		error("sockfd failed...");
	}

	int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed...");
    }



	portno = atoi(PORT);


	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if( bind(sockfd, server_info->ai_addr, server_info->ai_addrlen) < 0)
	{
		error("binding failed...");
	}
	freeaddrinfo(server_info);


	if(listen(sockfd,10) < 0)
	{

		error("listening failed...");
	}

	printf("Server is listeasdasddsaning on port %s\n", PORT);

	if( daemon_mode == 1 )
	{
		int dae = daemon(1,1);
		if(dae < 0){
			error("daemon failed");
			// perror("Daemonization failed");
			// exit(EXIT_FAILURE);
		}

		if(dae == 1){
			exit(EXIT_SUCCESS);
		}


	// sigaction part
	struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action) );
    new_action.sa_handler = signal_handler;

    if( sigaction(SIGINT, &new_action, NULL) < 0 || sigaction(SIGTERM, &new_action, NULL) < 0 )
    {
    	error("sigaction failed...");
    }

    // sigaction part
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    while( sig_quit == false )
    {
    	struct sockaddr_storage cli_addr;
    	socklen_t clilen;
    	clilen = sizeof(cli_addr);
    	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if(newsockfd < 0)
		{

			syslog(LOG_ERR, "accepting failed... \n");
			continue;
		}

		printf("OLOOOOOOOOOOOOL 222 \n");

		bool is_ipv4 = (cli_addr.ss_family == AF_INET);
		char client_ip[INET6_ADDRSTRLEN]; // for ipv4 client_ip[INET_ADDRSTRLEN];


		if (is_ipv4) {
	        struct sockaddr_in *addr = (struct sockaddr_in *)&cli_addr;
	        if (inet_ntop(AF_INET, &addr->sin_addr, client_ip, sizeof(client_ip)) == NULL) {
	            error("inet_ntop failed...");
	        }
	    } else {
	        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&cli_addr;
	        if (inet_ntop(AF_INET6, &addr->sin6_addr, client_ip, sizeof(client_ip)) == NULL) {
	            error("inet_ntop failed...");
	        }
	    }

		syslog(LOG_INFO,  "Accepted connection from %s", client_ip);


		// open data

		int data_fd = open(DATA_FILE_PATH, O_RDWR|O_CREAT|O_APPEND, 0600);
		if(data_fd < 0)
		{
			error("send_data_to_client, open function error...");
			close(data_fd);
			// return NULL;
			continue;
		}



		struct thread_data *t_data = malloc(sizeof(struct thread_data));
		t_data->mutex = &mutex;
		t_data->newsockfd = newsockfd;
		t_data->data_fd = data_fd;
        t_data->thread_complete_success = false;


        pthread_t thread_timestamp_0;
        if( pthread_create(&thread_timestamp_0, NULL, timestamp_thread, t_data) != 0 ){
        	syslog(LOG_ERR, "thread_timestamp...");
            close(newsockfd);
            close(data_fd);
            free(t_data);
        } else {
        	pthread_detach(thread_timestamp_0);
        }





        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, t_data) != 0) {
            syslog(LOG_ERR, "pthread_create error...");
            close(newsockfd);
            // close(data_fd);
            free(t_data);
        } else {
            pthread_detach(thread_id); // Detach the thread for automatic cleanup
        }

    }




    // cleanup();
    if (remove("/var/tmp/aesdsocketdata") == 0) {
    	printf("File deleted successfully.\n");
	} else {
	    perror("Error deleting file");
	}
	pthread_mutex_destroy(&mutex);
    close(sockfd);
    return 0;
}

}