#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SUCCESS 1
#define FAIL 0

#define EXEC_COMMAND(arglist, ...) execvp(arglist[0], arglist)

#define WAITPID(pid, status)                                                                                           \
    while (waitpid(pid, status, 0) == -1 && (errno == EINTR || errno == ECHILD))                                                            \
    {                                                                                                                  \
    }

// This piece of code was take from http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
void handle_sigchld(int sig)
{
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0)
    {
    }
}

#define print_err(message, ...) fprintf(stderr, "Error: " message "\n", ##__VA_ARGS__)

int foreground_command(char **arglist)
{
    pid_t pid;

    pid = fork();
    if (pid == -1)
    {
        print_err("can not fork");
        return FAIL;
    }
    else if (pid == 0)
    {
        EXEC_COMMAND(arglist)
		// Check for an error
        print_err("can not execute the command");
		exit(1);
    }

    // Waiting for sons to finish
    // Remark - remember to handle errors in wait!
    waitpid(pid, NULL, 0);
    return SUCCESS;
}

int background_command(char **arglist)
{
    pid_t pid;

    pid = fork();
    if (pid == -1)
    {
        print_err("can not fork");
        return FAIL;
    }
    else if (pid == 0)
    {
        EXEC_COMMAND(arglist)
		// Check for an error
        print_err("can not execute the command");
		exit(1);
    }
    // This time we doesn't wait for son the to finish.
    // We handle SIGCHLD for the child to be wait()ed when finished (asynchrony)
    return SUCCESS;
}

int pipe_command(char **arglist1, char **arglist2)
{
    int fd[2];
    int dret, cls0, cls1;
    int wstatus;

    if (pipe(fd) == -1)
    {
        return -1;
    }

    int pid1 = fork();
    if (pid1 < 0)
    {
        print_err("can not fork");
        return FAIL;
    }
    if (pid1 == 0)
    {
        dret = dup2(fd[1], STDOUT_FILENO) == -1;
        cls0 = close(fd[0]);
        cls1 = close(fd[1]);

        // Check for an error
        if (dret || cls0 || cls1)
        {
            print_err("can not reroute stdout");
            exit(1);
        }

        EXEC_COMMAND(arglist1);

        // Check for an error
        print_err("can not execute the first command");
        exit(1);
    }

    int pid2 = fork();
    if (pid1 < 0)
    {
        waitpid(pid1, NULL, 0); // Preventing pid1 from being a zombie
        print_err("can not fork");
        return FAIL;
    }
    if (pid2 == 0)
    {
        dret = dup2(fd[0], STDIN_FILENO) == -1;
        cls0 = close(fd[0]);
        cls1 = close(fd[1]);

        // Check for an error
        if (dret || cls0 || cls1)
        {
            print_err("can not reroute stdin");
            exit(1);
        }

        EXEC_COMMAND(arglist2);

        // Check for an error
        print_err("can not execute the second command");
        exit(1);
    }

    cls0 = close(fd[0]);
    cls1 = close(fd[1]);

    // Check for an error
    if (cls0 || cls1)
    {
        print_err("can not close the pipe");
    }

    // Waiting for son1 to finish
    WAITPID(pid1, &wstatus)

    if (wstatus)
    {
        // In case there is an error in first son, kill the second son (that waiting for an input)
        if (kill(pid2, SIGKILL) == -1)
        {
            // Note - the first son may be valid, but the second son is an error. Than the first son may write,
            // and fill the buffer and exit with an error (and the kill may fail)
            if (errno != ESRCH)
            {
                print_err("can not kill a son process");
                return FAIL;
            }
        }
    }

    // Waiting for son2 to finish
    WAITPID(pid2, NULL)

    return SUCCESS;
}

int prepare(void)
{
    // This piece of code was taken from
    // http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1)
    {
        print_err("can not register handler");
        return 1;
    }
    return 0;
}

int process_arglist(int count, char **arglist)
{
    if (strcmp(arglist[count - 1], "&") == 0)
    {
        // This is a background command
        arglist[count - 1] = NULL;
        return background_command(arglist);
    }
    else
    {
        int ind;
        for (ind = 0; ind < count; ind++)
        {
            if (strcmp(arglist[ind], "|") == 0)
            {
                break; // Bingo
            }
        }
        if (ind < count)
        {
            // This is a pipe command
            // Let's split the args into 2 different commands
            arglist[ind] = NULL;
            return pipe_command(arglist, arglist + (ind + 1));
        }
        else
        {
            // This is a foreground command
            return foreground_command(arglist);
        }
    }
}

int finalize(void)
{
    return 0;
}