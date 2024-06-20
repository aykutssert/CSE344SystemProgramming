#ifndef COMMON_H
#define COMMON_H

#include <unistd.h>

#define REG_SERVER_FIFO_TEMPLATE "serverFIFO.%d"

#define CLIENT_FIFO_TEMPLATE "clientFIFO.%d"

#define REG_SERVER_FIFO_NAME_LEN (sizeof(REG_SERVER_FIFO_TEMPLATE) + 20)

#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)

enum resp_status {RESP_OK, RESP_ERROR, RESP_CONT, RESP_APPROVE, RESP_CONNECT, RESP_DISCONNECT};

struct request_header {
    size_t data_size; 
};

struct response_header {
    enum resp_status stat; 
    size_t data_size; 
};

struct client_info {
    pid_t pid;
    int wait;
    char cwd[256];
};

struct queue {
    int front;
    int rear;
    int capacity;
    int size;
    struct client_info **elements;
};

struct processArray {
    int size;
    int capacity;
    pid_t *pids;
};

int enqueue(struct queue *que, struct client_info *item);

struct client_info *dequeue(struct queue *que);

int resize_queue(struct queue *que);


void destroy_queue(struct queue *que); 

int add_pid_list(struct processArray * plist, pid_t pid);

int remove_pid_list(struct processArray *plist, pid_t pid);

void destroy_pid_list(struct processArray *plist);

int find_pid_list(const struct processArray *plist, pid_t pid);

int parse_command(char *cmd, char *cmd_argv[]);

void err_exit(const char *err);

#endif