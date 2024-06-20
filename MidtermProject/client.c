#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include "client.h"
#include "common.h"

#define CWD_SIZE 256
volatile sig_atomic_t connected = 1;
#define FIFO_PERM 0666

int main(int argc, char *argv[]) 
{
    struct request_header req;
    struct response_header resp;
    int server_fd;
    int client_fd;
    int num_read;
    int client_turn;
    int yesOrNo,skip;
    char client_fifo[CLIENT_FIFO_NAME_LEN], server_fifo[REG_SERVER_FIFO_NAME_LEN];
    char cmd[64];
    
    char *resp_data;
    struct client_info cli_info;
    
    if(argc!=3)exit(EXIT_FAILURE); // 3 arguman girilmezse

    //bağlama şekli
    if(strcmp(argv[1],"connect") == 0){
        cli_info.wait = 1;
    }
    else if(strcmp(argv[1],"tryConnect") == 0){
        cli_info.wait = 0;
    }
    else{
        exit(EXIT_FAILURE);
    }
    
    pid_t server_pid = atoi(argv[2]); //server id'si alınır.

    cli_info.pid = getpid(); //client id'si alınır.
    if (getcwd(cli_info.cwd, CWD_SIZE) == NULL) //client'in değişkenine current directory atanır.
        err_exit("getcwd");



    /* server'a istek atıp cevap alma kısmı */
    snprintf(server_fifo, REG_SERVER_FIFO_NAME_LEN, REG_SERVER_FIFO_TEMPLATE, server_pid);  //we write server_pid to server_fifo
    snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, cli_info.pid);// we write client_pid to client_fifo
    if (mkfifo(client_fifo, FIFO_PERM) == -1)//********
        err_exit("mkfifo");
    if ((server_fd = open(server_fifo, O_WRONLY)) == -1) //server'i açıyoruz
        err_exit("open server_fifo");
    if (write(server_fd, &cli_info, sizeof(struct client_info)) < (long) sizeof(struct client_info)) {
        fprintf(stderr, "Error writing registeration request header to FIFO %s\n", server_fifo);
        exit(EXIT_FAILURE); 
    }
    if ((client_fd = open(client_fifo, O_RDONLY)) == -1) //server'in client'e cevap yazısı readleme
        err_exit("client_fifo");
    if (read(client_fd, &client_turn, sizeof(int)) != (long) sizeof(int)) {
        fprintf(stderr, "Error reading registeration turn; discarding\n");
        exit(EXIT_FAILURE); 
    }
    /* server'a istek atıp cevap alma kısmı */

    if ((close(server_fd) == -1))
        err_exit("close reg server_fd");

    printf("Client turn: %d\n", client_turn); 
   
    if (client_turn > 0 && !cli_info.wait) {
        printf("exit\n");
        return 0; 
    }
    printf("Waiting for Que... ");
    snprintf(server_fifo, REG_SERVER_FIFO_NAME_LEN, REG_SERVER_FIFO_TEMPLATE, cli_info.pid);

    /* Create new server FIFO for server send the the responses */
    if (mkfifo(server_fifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
        err_exit("mkfifo");

    /* open() blocks until the server client opens the other end of the server FIFO for writing. */
    if ((server_fd = open(server_fifo, O_WRONLY)) == -1) 
        err_exit("open req server_fd");

   

    printf("Connection established");

    skip = 0;
    while (connected) {
        printf("\n\n");

        if (skip == 1)
            skip = 0;
        else {
            printf("Enter comment : ");
    
            if (read_command_line(cmd, 64) == -1) {
                fprintf(stderr, "Error reading user command %s\n", server_fifo);
                continue;
            }
            else if (cmd[0] == '\n')
                continue;
            
            
            req.data_size = strlen(cmd) + 1;
            
            /* Write the header */
            if (write(server_fd, &req, sizeof(struct request_header)) < (long) sizeof(struct request_header)) {
                fprintf(stderr, "Error writing request header to FIFO %s\n", server_fifo);
                continue;
            }
            /* Write the command */
            if (write(server_fd, cmd, req.data_size) < (long) req.data_size) {
                fprintf(stderr, "Error writing request cmd to FIFO %s\n", server_fifo);
            }
        }

        do {
            /* Wait for the response */
            num_read = read(client_fd, &resp, sizeof(struct response_header));
            if (num_read == 0) {
                printf("Server fifo is closed\n");
                break;
            }
            else if (num_read < (long) sizeof(struct response_header)) {
                fprintf(stderr, "Error reading response header; discarding\n");
                break;
            }

            printf("\n");

             //quit yada kill geldiyse.
            if (resp.stat == RESP_DISCONNECT) {
                connected = 0;
            }

            //data geliyorsa
            else if (resp.data_size > 0) { 
                if ((resp_data = malloc(resp.data_size + 1)) == NULL) {
                    break;
                }
                if (read(client_fd, resp_data, resp.data_size) < (long) resp.data_size) {
                    fprintf(stderr, "Error reading response data; discarding\n");
                    free(resp_data);
                    break;
                }
                
                resp_data[resp.data_size + 1] = '\0';

                printf("%s", resp_data);
                free(resp_data);
            } 
        } while (resp.stat == RESP_CONT);

        
        if (resp.stat == RESP_APPROVE) { //yesOrNO cevabı beklenir.

            // Not overwrite is the default behaviour
            if ((skip = yesOrNo = get_approve()) == -1)
                yesOrNo = 0;
            if (write(server_fd, &yesOrNo, sizeof(int)) < (long) sizeof(int)) {
                fprintf(stderr, "Error writing approve response to FIFO %s\n", server_fifo);
                break;
            }
        }
        
    }

    printf("\nClosing resources...\n");

    if (close(server_fd) == -1) 
        perror("close server_fd");

    if (close(client_fd) == -1) 
        perror("close client_fd");

    if (unlink(client_fifo) == -1)
        perror("unlink client_fifo");
        
    if (unlink(server_fifo) == -1)
        perror("unlink server_fifo");

    printf("exit\n");
}

int get_approve()
{
    char resp[1024];
    int len;
    while (1) {
        if (fgets(resp, 1024, stdin) == NULL)
            return -1;
        len = lower(resp);
        resp[len - 1] = '\0';
        if (strcmp(resp, "yes") == 0)
            return 1;
        else if (strcmp(resp, "no") == 0)
            return 0;
        printf("Please write only yes or no: ");
    }
    fflush(stdin);
}

int lower(char *str)
{
    int i;
    for (i = 0; str[i] != '\0'; ++i)
        if ('A' <= str[i] && str[i] <= 'Z')
            str[i] = 'a' + str[i] - 'A';
    return i;
}

int read_command_line(char buff[], int buff_size) 
{
    int len;

    if (fgets(buff, buff_size, stdin) == NULL)
        return -1;

    len = strlen(buff);

    /* in windows line ends with '\r\n' */
    if (len > 1 && (buff[len - 2] == '\r' || buff[len - 2] == '\n'))
        len -= 2;        
    else if (len > 1 && (buff[len - 1] == '\n' || buff[len - 1] == '\r'))
        len -= 1;
    
    buff[len] = '\0';
    return len;
}



