#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>


int main(int argc, char *argv[]){
	// const char *filename = "non-existing-file.txt";
	// FILE *file = fopen(filename, "rb");
	// if(file==NULL){
	// 	fprintf(stderr, "lati lati %s: %d\n", filename, errno);
	// 	perror("perror returned");
	// 	fprintf(stderr, "lati lati part 2 %s: %s\n", filename, sterror(errno));
	// }
	// else{
	// 	fclose(file);
	// }
	// return 0;

	openlog(NULL, 0, LOG_USER);

	if (argc > 3){
		syslog(LOG_ERR, "unexpected error occured, 3 or more aguments");
		closelog();
		return 1;
	}


	const char *writefile = argv[1];
	const char *writestr = argv[2];

	int fd = open(writefile, O_WRONLY | O_CREAT, 0644);
	if( fd < 0 )
	{
		syslog(LOG_ERR, "Failed to open file this is line 38 shoo %s: %s", writefile, strerror(errno));
	    closelog();
	    return 1;
	}

	//syslog(LOG_DEBUG, writestr, writefile);
	write(fd,writestr,strlen(writestr));
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	close(fd);
	closelog();

	return 0;
}
