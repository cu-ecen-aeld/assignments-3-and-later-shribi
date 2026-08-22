#include "systemcalls.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/wait.h>
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
    int ret = system(cmd);
    return ret != -1 ? true : false;
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

    __pid_t pid = fork();
    if (pid == -1) 
    {
        printf("Fork failed.\n");
        va_end(args);
        return false;
    } 

    if (pid == 0)
    {
        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }
    else 
    {
        int ret = -1;
        if (waitpid(pid, &ret, 0) == -1)
        {
            printf("Error in waiting for the child\n");
            va_end(args);
            return false;
        }
        return (ret == 0);
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
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

    __pid_t pid = fork();
    if (pid == -1) 
    {
        printf("Fork failed.\n");
        va_end(args);
        return false;
    } 

    printf("Fork Successful for %s.\n", *command); 
    if(pid == 0)
    {        
        int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd < 0)
        {
            perror("Couldnt open the file");
            return false;
        }

        if (dup2(fd, 1) < 0) {
            perror("Couldnt redirect to the file");
            return false;
        }
        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }
    else 
    {
        int ret = -1;
        if (waitpid(pid, &ret, 0) == -1)
        {
            printf("Error in waiting for the child\n");
            va_end(args);
            return false;
        }
        printf("Successfully executed child %d.\n", ret);
        va_end(args);
        return (ret == 0);
    }
}
