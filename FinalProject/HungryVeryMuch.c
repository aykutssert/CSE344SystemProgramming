#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>

typedef struct {
    int id;
    int x, y;
    int status; // 0: placed, 1: being prepared, 2: being cooked, 3: being delivered, 4: delivered
    int isPlaced;
    int send;
    time_t timestamp;
    int customer;
    int clientID;
} Order;
Order *orders;
int client_socket;
int order_canceled = 0;
void handle_signal(int sig);
void wait_for_notification(int portnum);

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [server_ip] [portnum] [CustomerCount] [p] [q]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *server_ip = argv[1];
    int portnum = atoi(argv[2]);
    int customerCount = atoi(argv[3]);
    int p = atoi(argv[4]);
    int q = atoi(argv[5]);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    srand(time(NULL));
    char buffer[1024] = {0};
    struct sockaddr_in server_addr;
    int customercountSocket = 0;
    if ((customercountSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portnum);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

    if (connect(customercountSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    sprintf(buffer, "%d", customerCount);
    send(customercountSocket, buffer, strlen(buffer), 0);
    close(customercountSocket);
    



    Order *orders = (Order *)malloc(customerCount * sizeof(Order));
    for(int i=0;i<customerCount;i++){
        int x = rand() % p;
        int y = rand() % q;
        Order newOrder;
        newOrder.id = i+1;
        newOrder.status = 0;
        newOrder.isPlaced = 0;
        newOrder.x = x;
        newOrder.y = y;
        newOrder.send = 0;
        newOrder.timestamp = time(NULL);
        newOrder.customer = customerCount;
        newOrder.clientID = getpid();
        orders[i] = newOrder;
    }
    int socket_fd;
    
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portnum);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            printf("Invalid address/Address not supported\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    send(socket_fd, orders, customerCount * sizeof(Order), 0); // Dikkat edin burada '&orders' yerine 'orders' kullanıyoruz.
    free(orders); // Hafızayı serbest bırakın.
    close(socket_fd);
    
    
    wait_for_notification(portnum);
    return 0;
}


void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        order_canceled = 1;
        printf("\nOrder cancellation signal received\n");
        exit(0);
    }
}
void wait_for_notification(int portnum) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(portnum);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("...\n");

    while (1) {
        
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        
        char buffer[1024] = {0};
        int valread = read(new_socket, buffer, 1024);
        if (valread < 0) {
            perror("Read failed");
        } else {
           
          
            printf("%s\n", buffer);
            printf("Log file written ...\n");
            break;
          
        }
        close(new_socket);
        sleep(2);
    }
    close(server_fd);
}
