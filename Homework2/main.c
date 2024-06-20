

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"

int counter = 0;

void sigchld_handler(int signo)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        }
        else
        {
            printf("Child %d exited abnormally\n", pid);
        }
        counter++;

    }
}

void createFifo(char *fifo)
{
    if (mkfifo(fifo, 0666) == -1)
    {
        perror("mkfifo");
        exit(1);
    }
    
}


void childProcess1(int arraySize)
{
    
    
   
    int fd1, sum = 0;
    int numbers[arraySize];

    fd1 = open(FIFO1, O_RDONLY);
    if (fd1 == -1)
    {
        perror("Failed to open FIFO1");
        exit(1);
    }
    sleep(10);
    if (read(fd1, numbers, arraySize * sizeof(int)) == -1)
    {
        perror("Failed to read data from FIFO1");
        exit(1);
    }
    close(fd1);

    for (int i = 0; i < arraySize; i++)
    {
        sum += numbers[i];
    }
    printf("Processing sum: %d\n", sum);
    
    int fd2 = open(FIFO2, O_WRONLY);
    if (fd2 == -1)
    {
        perror("Failed to open FIFO2");
        exit(1);
    }
    if (write(fd2, &sum, sizeof(int)) == -1)
    {
        perror("Failed to write data to FIFO2");
        exit(1);
    }
    close(fd2);

    exit(0);
}


void childProcess2(int arraySize)
{
    int fd2, sum, result = 1;
    char command[9];

    fd2 = open(FIFO2, O_RDONLY);
    if (fd2 == -1)
    {
        perror("Failed to open FIFO2");
        exit(1);
    }
    sleep(10);
    if (read(fd2, command, sizeof(command)) == -1)
    {
        perror("Failed to read command from FIFO2");
        exit(1);
    }
    if (strcmp(command, "multiply") != 0)
    {
        printf("Invalid command: %s\n", command);
        exit(1);
    }
    for (int i = 0; i < arraySize; i++)
    {
        int number;
        if (read(fd2, &number, sizeof(int)) == -1)
        {
            perror("Failed to read data from FIFO2");
            exit(1);
        }
        result *= number;
    }
    if (read(fd2, &sum, sizeof(int)) == -1)
    {
        perror("Failed to read sum from FIFO2");
        exit(1);
    }
    close(fd2);

    printf("Processing multiplication: %d\n", result);
    printf("Final result: %d\n", result + sum);

    exit(0);
}
void parentProcess(int *numbers, int arraySize)
{
   
    int fd1 = open(FIFO1, O_WRONLY);
    if (fd1 == -1)
    {
        perror("Failed to open FIFO1");
        exit(1);
    }
    
    if (write(fd1, numbers, arraySize * sizeof(int)) == -1)
    {
        perror("Failed to write data to FIFO1");
        exit(1);
    }
    close(fd1);

    int fd2_command = open(FIFO2, O_WRONLY);
    if (fd2_command == -1)
    {
        perror("Failed to open FIFO2");
        exit(1);
    }
    char *command = "multiply";
    int command_length = strlen(command) + 1; // Including null terminator

    if (write(fd2_command, command, command_length) == -1) {
        perror("Failed to write command to FIFO2");
        exit(1);
    }
    int fd2 = open(FIFO2, O_WRONLY);
        if (fd2 == -1)
        {
            perror("Failed to open FIFO2");
            exit(1);
    }
    for(int i=0;i<arraySize;i++){
        
        if (write(fd2, &numbers[i], sizeof(int)) == -1)
        {
            perror("Failed to write data to FIFO2");
            exit(1);
        }
    }
    close(fd2);
    
    while (counter < 2)
    {

        printf("Proceeding\n");
        sleep(2);
        
    }
}
int main(int argc, char *argv[])
{
    srand(time(NULL));
    if (argc != 2)
    {
        printf("Usage: %s <array size>\n", argv[0]);
        exit(1);
    }
    int arraySize = atoi(argv[1]);
    if (arraySize <= 0)
    {
        printf("Invalid array size: %d\n", arraySize);
        exit(1);
    }
    int numbers[arraySize];
    for (int i = 0; i < arraySize; i++)
    {
        numbers[i] = rand() % 10;
    }
    printf("Initial numbers: ");
    for (int i = 0; i < arraySize; i++)
    {
        printf("%d ", numbers[i]);
    }
    printf("\n");
    
    createFifo(FIFO1);
    createFifo(FIFO2);

    int id1 = fork();
    if (id1 == -1)
    {
        perror("Failed to fork child process 1");
        exit(1);
    }
    if (id1 == 0)
    {
        childProcess1(arraySize);
    }
    else{
        int id2 = fork();
        if (id2 == -1)
        {
            perror("Failed to fork child process 2");
            exit(1);
        }
        if (id2 == 0)
        {
            childProcess2(arraySize);
        }
        else{
            struct sigaction sa;
            sigemptyset(&sa.sa_mask);
            sa.sa_handler = sigchld_handler;
            sa.sa_flags = 0;
            sigaction(SIGCHLD, &sa, NULL);


            parentProcess(numbers, arraySize);

        }
    }

    printf("Program completed successfully\n");

    return 0;
}