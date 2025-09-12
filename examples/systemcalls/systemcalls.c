#include "systemcalls.h"
#include <stdlib.h> // added by sana-all
#include <stdbool.h> // added by sana-all
#include <unistd.h> // added by sana-all fork(), exec(), getpid()
#include <sys/types.h> // added by sana-all size_t, ssize_t, and pid_t
#include <sys/wait.h> // added by sana-all wait() and macros like WIFEXITED() and WEXITSTATUS()
//#include <errno.h>  // For errno
#include <string.h>
#include <syslog.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	int rc = system(cmd); // added by sana-all
	if(rc == 0){
		return true;
	}
	return false;
			
    //return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    
    va_end(args);
    
    
    pid_t pid = fork();
    
        if (pid == -1) {
		perror("Fork failed");
		return false; 
        }
        else if (pid == 0) {

		execv(command[0], command);
		perror("Exec failed");
		exit(EXIT_FAILURE); 
        }
        
	    int status;
	    if (wait(&status) == -1) {
		perror("Wait failed");
		return false;
	    }

	    
	    if (WIFEXITED(status)) {
		return WEXITSTATUS(status) == 0; 
	    }

	    return false;

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/


}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...) {
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    va_end(args);

    // Open the output file for writing
    int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to open output file");
        return false;
    }

    // Redirect standard output to the file
    // Because if you use write it is 
    int loti = dup2(fd, STDOUT_FILENO);
    if (loti == -1) {
        perror("dup2 failed");
        close(fd);
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        close(fd);
        return false; 
    } else if (pid == 0) {
        // Child process
        execv(command[0], command);
        perror("Exec failed");
        exit(EXIT_FAILURE); 
    }

    // Parent process
    close(fd); // Close the file descriptor in the parent process
    int status;
    if (wait(&status) == -1) {
        perror("Wait failed");
        return false;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status) == 0; 
    }

    return false;
}
