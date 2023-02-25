#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>

void printError()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void handleExit()
{
    exit(0);
}

void handleCD(char *directory)
{
    if (chdir(directory) != 0)
    {
        printError();
    }
}

void handlePWD()
{
    int size = 256;
    char cwd[size];

    if (getcwd(cwd, size) == NULL)
    {
        printError();
        exit(1);
    }
    else
    {
        printf("%s\n", cwd);
        fflush(stdout);
    }
}

void handleOutputFD(char **args, int argc, int *output_fd){
    int redirect_i = 0;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            redirect_i = i;

            if (args[redirect_i + 1] == NULL)
            {
                printError();
            }
            {
                *output_fd = open(args[redirect_i + 1], O_CREAT | O_TRUNC | O_WRONLY, 0644);
            }
            break;
        }
    }
}

int pipeHelper(char **args, int in, int out)
{
    int pid;

    if ((pid = fork ()) == 0){
        if (in != 0){
            dup2 (in, 0);
            close (in);
        }

        if (out != 1){
            dup2 (out, 1);
            close (out);
        }

        return execv (args[0], args);
        }

    return pid;
}

int handlePipe(char **args, int argc, int input_fd, int output_fd){
    int cmd_is[argc];
    int pipec = 0;

    cmd_is[0] = 0;
    
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            args[i] = NULL;
            break;
        }
        if (strcmp(args[i], "|") == 0)
        {
            cmd_is[pipec + 1] = i + 1;
            args[i] = NULL;
            pipec++;
        }
    }

    if(pipec == 0){
        return 0;
    }
    
    int pid = fork();

    if (pid == 0)
    {
        int i;
        int in = 0;
        int fd[2];

        for (i = 0; i < pipec; i++)
        {
            pipe(fd);

            pipeHelper(args + cmd_is[i], in, fd[1]);
            close(fd[1]);
            in = fd[0];
        }

        if (in != 0)
        {
            dup2(in, 0);
        }

        if (output_fd != -1)
        {
            dup2(output_fd, 1);
            close(output_fd);
        }

        close(in);
        execv(args[cmd_is[i]], args + cmd_is[i]);
    } else{
        wait(&pid);
    }
    return 1;
    }

void handleExec(int argc, char **args)
{
    int pipec;
    int output_fd = -1;

    handleOutputFD(args, argc, &output_fd);
    if(handlePipe(args, argc, STDIN_FILENO, output_fd) == 0){
        int pid = fork();

        if (pid == 0)
        {
            if (output_fd != -1)
            {
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            if (execv(args[0], args) != 0)
            {
                if (output_fd != -1)
                {
                    dup2(output_fd, STDERR_FILENO);
                    close(output_fd);
                }
                printError();
            }
            exit(1);
        }
        else
        {
            // PARENT PROCESS
            int pid = pid;
            wait(&pid);
        }
    }

}


int lexer(char *line, char ***args, int *num_args)
{
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if (l == NULL)
    {
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while (token != NULL)
    {
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * *num_args);
    *num_args = 0;
    token = strtok(line, " \t\n");
    while (token != NULL)
    {
        char *token_copy = strdup(token);
        if (token_copy == NULL)
        {
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }

    return 0;
}

void handleInput(int argc, char **args)
{
    char *firstCMD = args[0];
    if (firstCMD == NULL)
    {
        printError();
    }
    else if (strcmp(firstCMD, "cd") == 0)
    {
        if (argc != 2)
        {
            printError();
        }
        else
        {
            handleCD(args[1]);
        }
    }
    else if (strcmp(firstCMD, "exit") == 0)
    {
        if (argc != 1)
        {
            printError();
        }
        else
        {
            handleExit();
        }
    }
    else if (strcmp(firstCMD, "pwd") == 0)
    {
        if (argc != 1)
        {
            printError();
        }
        else
        {
            handlePWD();
        }
    }
    else
    {
        handleExec(argc, args);
    }
}

int handleLoop(int argc, char **args)
{
    if (strcmp(args[0], "loop"))
    {
        return 0;
    }

    int loopc = atoi(args[1]);
    argc = argc - 2;
    args = args + 2;

    for (int i = 0; i < loopc; i++)
    {
        char **targs = malloc((argc + 1) * sizeof(char *));
        for (int i = 0; i < argc; i++)
        {
            targs[i] = args[i];
        }
        int targc = argc;
        targs[targc] = NULL;

        handleInput(targc, targs);

        free(targs);
    }

    return 1;
}

int handleMulargs(int argc, char **args)
{
    int num_args[argc];
    for (int i = 0; i < argc; i++)
    {
        num_args[i] = 0;
    }

    int num_semis = 0;

    int c = 0;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(args[i], ";") == 0)
        {
            num_semis++;
            c++;
        }
        else
        {
            num_args[c]++;
        }
    }

    if (num_semis == 0)
    {
        return 0;
    }

    c = 0;
    for (int i = 0; i <= num_semis; i++)
    {
        int targc = num_args[i];
        char **targs = malloc((targc + 1) * sizeof(char *));

        for (int i = 0; i < targc; i++)
        {
            targs[i] = args[c];
            c++;
        }
        c++;

        targs[targc] = NULL;

        if (targs[0] == NULL)
        {
            printError();
        }
        else if (strcmp(targs[0], "loop") == 0)
        {
            handleLoop(targc, targs);
        }
        else
        {
            handleInput(targc, targs);
        }

        targc = 0;
        free(targs);
    }

    return 1;
}

int shellContinous()
{
    while (1)
    {
        char *line;
        size_t bufsize = 128;

        printf("ashell> ");
        fflush(stdout);

        if (getline(&line, &bufsize, stdin) == 0)
        {
            printError();
        }

        char **args;
        int argc;

        lexer(line, &args, &argc);
        args[argc] = NULL;

        if (args[0] == NULL)
        {
            printError();
        }
        else
        {
            if (handleMulargs(argc, args) == 0)
            {
                if (handleLoop(argc, args) == 0)
                {
                    handleInput(argc, args);
                }
            }
        }

        free(args);
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    shellContinous();
    return 0;
}
