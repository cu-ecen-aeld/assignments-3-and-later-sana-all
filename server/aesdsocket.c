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
#include "../aesd-char-driver/aesd_ioctl.h"
// #include "queue.h"

#define PORT "9000"
#define BUFFER_SIZE 1024
// #define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
// #define DATA_FILE_PATH "/dev/aesdchar"

/* Default to using the char device, but allow the build system to override. */
#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif

#if USE_AESD_CHAR_DEVICE
#define DATA_FILE_PATH "/dev/aesdchar"
#else
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#endif

volatile sig_atomic_t sig_quit = false;
// pthread_mutex_t mutex;
// pthread_mutex_init(&mutex, NULL);

struct thread_node {
    pthread_t thread_id;
    int client_fd;
    // int completed;
    struct thread_node *next;
};

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct thread_data {
    pthread_mutex_t *mutex;
    int newsockfd;
    int data_fd;
    bool thread_complete_success;
};

// typedef struct connection_t{
//     SLIST_ENTRY(connection_t) entries; // Linked list entry
//     pthread_t thread_id;
// } connection_t;

struct thread_node *head = NULL;
struct thread_node *current = NULL;

#if !USE_AESD_CHAR_DEVICE
void *timestamp_thread(void *arg) {
    pthread_mutex_t *mtx = (pthread_mutex_t *)arg;
    int data_fd = open(DATA_FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0600);
    if (data_fd < 0) {
        syslog(LOG_ERR, "timestamp_thread: open failed: %s", strerror(errno));
        return NULL;
    }

    while (!sig_quit) {
        sleep(10);

        char buffer[BUFFER_SIZE];
        struct tm *t_ptr;
        time_t t = time(NULL);
        t_ptr = localtime(&t);

        size_t len = strftime(buffer, sizeof(buffer), "timestamp:%F %T\n", t_ptr);
        if (len == 0) {
            syslog(LOG_ERR, "timestamp_thread: strftime failed");
            continue;
        }

        pthread_mutex_lock(mtx);
        ssize_t bytes_written = write(data_fd, buffer, len);
        if (bytes_written < 0) {
            syslog(LOG_ERR, "timestamp_thread: write failed: %s", strerror(errno));
        } else {
            syslog(LOG_INFO, "timestamp_thread wrote %zd bytes", bytes_written);
        }
        pthread_mutex_unlock(mtx);
    }

    close(data_fd);
    return NULL;
}
#endif


static short int handle_client_parse_seekto(const char *line, struct aesd_seekto *st) {
	char *endptr;
	const char *colon = strchr(line, ':');
	const char *comma = strchr(line, ',');
	const char *nl = strchr(line, '\n');

	if( colon == NULL || comma == NULL || nl == NULL ) return -1;

	long x = strtol(colon + 1, &endptr, 10);

	if( endptr != comma ) return -1;

	long y = strtol(comma + 1, &endptr, 10);

	if( endptr != nl && *endptr != '\0' ) return -1;
	if( x < 0 || y < 0 ) return -1;
	st->write_cmd = (uint32_t)x;
	st->write_cmd_offset = (uint32_t)y;


	return 0;
}

static short int handle_client_ioctl_handler_single(int client_fd, const char *line) {
	struct aesd_seekto st;
	if (handle_client_parse_seekto(line, &st) != 0) return -1;
	int fd = open(DATA_FILE_PATH, O_RDWR);
	if (fd < 0) return -1;
	if (ioctl(fd, AESDCHAR_IOCSEEKTO, &st) < 0) { close(fd); return -1; }
	char buf[1024];
	ssize_t r;
	while ((r = read(fd, buf, sizeof(buf))) > 0){
		ssize_t sent = 0;
		char *p = buf;
		while (r > 0){
			ssize_t s = send(client_fd, p, r, 0);
			if(s <= 0){
				close(fd);
				return -1;
			}
			r -= s;
			p += s;
		}
	}
	close(fd);
	return (r < 0) ? -1 : 0;

}


void *handle_client(void *arg) {
    struct thread_data *t_data = (struct thread_data *)arg;
    int newsockfd = t_data->newsockfd;
    int data_fd = t_data->data_fd;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_received;
    // printf("OLOOOOOOOOOOOOL handle client\n");

    while ((bytes_received = recv(newsockfd, buffer, BUFFER_SIZE - 1, 0)) > 0 && sig_quit == false) {
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
        	if (strncmp(buffer, "AESDCHAR_IOCSEEKTO", 18) == 0){
        		pthread_mutex_lock(t_data->mutex);
        		handle_client_ioctl_handler_single(newsockfd, buffer);
        		pthread_mutex_unlock(t_data->mutex);
        	}

        	else{
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
    }

    if (bytes_received < 0) {
        syslog(LOG_ERR, "error sending data to client...");
    }

    close(data_fd);
    close(newsockfd);
    free(t_data);

    return NULL;
}

void signal_handler() {
    syslog(LOG_INFO, "Caught signal, exiting");
#if !USE_AESD_CHAR_DEVICE
    int data_fd = open(DATA_FILE_PATH, O_RDWR | O_CREAT, 0600);
    if (data_fd >= 0) {
        if (ftruncate(data_fd, 0) != 0) {
            syslog(LOG_ERR, "signal_handler: ftruncate failed: %s", strerror(errno));
        }
        close(data_fd);
    } else {
        syslog(LOG_ERR, "signal_handler: open failed: %s", strerror(errno));
    }
#endif
    sig_quit = true;
}

int main(int argc, char *argv[]) { // will uncomment later
    // SLIST_INIT(&head);
    int daemon_mode = 0;

    if (argc == 2) {
        if (strcmp(argv[1], "-d") != 0) {
            error("Wrong parameter inputted, try -d for daemon mode");
        }
        daemon_mode = 1;
    }

    int sockfd, newsockfd, portno;
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
    if (sockfd < 0) {
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

    if (bind(sockfd, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        error("binding failed...");
    }
    freeaddrinfo(server_info);

    if (listen(sockfd, 10) < 0) {
        error("listening failed...");
    }

    printf("Server is listening on port %s\n", PORT);

    if (daemon_mode == 1) {
        int dae = daemon(1, 1);
        if (dae < 0) {
            error("daemon failed");
        }

        if (dae == 1) {
            exit(EXIT_SUCCESS);
        }
    }

    // sigaction part
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = signal_handler;

    if (sigaction(SIGINT, &new_action, NULL) < 0 || sigaction(SIGTERM, &new_action, NULL) < 0) {
        error("sigaction failed...");
    }
    // sigaction part

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_t thread_timestamp_0;
    // int data_fd;

#if !USE_AESD_CHAR_DEVICE
    if (pthread_create(&thread_timestamp_0, NULL, timestamp_thread, (void *)&mutex) != 0) {
        syslog(LOG_ERR, "thread_timestamp create failed");
    } else {
        pthread_detach(thread_timestamp_0);
    }
#endif

    while (sig_quit == false) {
        struct sockaddr_storage cli_addr;
        socklen_t clilen;
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            syslog(LOG_ERR, "accepting failed... \n");
            continue;
        }

        // printf("OLOOOOOOOOOOOOL 222 \n");

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

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // open data
        int data_fd = open(DATA_FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0600);
        if (data_fd < 0) {
            error("send_data_to_client, open function error...");
            close(data_fd);
            continue;
        }

        struct thread_data *t_data = malloc(sizeof(struct thread_data));
        t_data->mutex = &mutex;
        t_data->newsockfd = newsockfd;
        t_data->data_fd = data_fd;
        t_data->thread_complete_success = false;
        // freed in handle_client

        // connection_t *new_conn = malloc(sizeof(connection_t));
        struct thread_node *new_conn = malloc(sizeof(struct thread_node));
        new_conn->client_fd = newsockfd;
        new_conn->next = NULL;

        int lomi = pthread_create(&new_conn->thread_id, NULL, handle_client, t_data);
        // SLIST_INSERT_HEAD(&head, new_conn, entries);
        if (head == NULL) {
            head = new_conn;
            current = head;
        } else {
            current->next = new_conn;
            current = current->next;
            current->next = NULL;
        }

        if (lomi != 0) {
            syslog(LOG_ERR, "pthread_create error...");
            close(newsockfd);
            // close(data_fd);
            free(t_data);
        } else {
            // pthread_detach(new_conn->thread_id); // Detach the thread for automatic cleanup
            pthread_join(new_conn->thread_id, NULL);
            continue;
        }
    }

    struct thread_node *head_temp = head;
    while (head_temp != NULL) {
        pthread_join(head_temp->thread_id, NULL); // Join the thread
        // printf("HAHAHA\n");
        head_temp = head_temp->next; // Move to the next node
    }

    struct thread_node *head_next;
    while (head != NULL) {
        head_next = head->next; // Store the next node before freeing
        free(head);             // Free the current node
        head = head_next;       // Move to the next node
    }

    pthread_join(thread_timestamp_0, NULL);

    pthread_mutex_destroy(&mutex);
    close(sockfd);

#if !USE_AESD_CHAR_DEVICE
    if (remove(DATA_FILE_PATH) == 0) {
        printf("File deleted successfully.\n");
    } else {
        perror("Error deleting file");
    }
#endif

    return 0;
}

