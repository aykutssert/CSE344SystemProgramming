#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <arpa/inet.h>

#define MAX_CHEFS 10
#define MAX_DELIVERERS 10
#define MAX_OVEN_CAPACITY 6
#define MAX_DELIVERY_BAG_CAPACITY 3
#define MAX_OVEN_SLOTS 2
#define MAX_OVEN_PEELS 3


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

typedef struct {
    int id;
} Chef;

typedef struct {
    int id;
    int activeOrders[MAX_DELIVERY_BAG_CAPACITY];
    Order orders[MAX_DELIVERY_BAG_CAPACITY];
    int numActiveOrders;
    int totalDelivered;
} Deliverer;


Chef *chefs;
Deliverer *deliverers;
pthread_t sendNotificationThread;
pthread_t *deliveryThreadPool;
pthread_t *chefThreadPool;


pthread_mutex_t ordersMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ovenPeelsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chefMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ovenMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ovenSlotsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ovenPeelAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t ovenAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t orderAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t ovenSlotAvailable = PTHREAD_COND_INITIALIZER;
int ovenSlots = MAX_OVEN_SLOTS;
int ovenPeels = MAX_OVEN_PEELS;
int ovenCapacity = 0;
int clientID = 0;

Order *ordersArray;
FILE *logFile;
int deliveredOrders = 0;
int max_order=-1;
int delivererPoolSize=0;
int chefPoolSize=0;
int deliverySpeed=0;
int portnum=0;
int numOrders = 0;
int allOrders = 0;
char *server_ip;
int control=-1;
double calculateDistance(int x1, int y1, int x2, int y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

void logMessage(const char *message) {
    pthread_mutex_lock(&logMutex);
    fprintf(logFile, "%s\n", message);
    fflush(logFile);
    pthread_mutex_unlock(&logMutex);
}
void sendNotification() {
            for (int i = 0; i < delivererPoolSize; i++) {
                char logBuffer[256];
                sprintf(logBuffer, "Deliverer %d totalDeliveredOrders:%d", deliverers[i].id, deliverers[i].totalDelivered);
                logMessage(logBuffer);
            }

            deliveredOrders = 0;

            int notificationSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (notificationSocket < 0) {
                perror("Socket creation error");
                exit(EXIT_FAILURE);
            }

            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portnum);

            if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
                printf("Invalid address/Address not supported\n");
                close(notificationSocket);
                exit(EXIT_FAILURE);
            }

            if (connect(notificationSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection Failed");
                close(notificationSocket);
                exit(EXIT_FAILURE);
            }

            char message[] = "All customers served";
            write(notificationSocket, message, strlen(message) + 1);
            printf("done serving client %d\n",clientID);
            printf("active waiting for connection...\n");
            close(notificationSocket);
       
}

void *chefRoutine(void *arg) {
    Chef *chef = (Chef *)arg;
    while (1) {
        pthread_mutex_lock(&ordersMutex);
        while (numOrders == 0) { // Order yoksa bekle
            pthread_cond_wait(&orderAvailable, &ordersMutex);
        }
        int orderIndex = -1;
        for(int i = 0; i < max_order; i++){
            if(ordersArray[i].status == 0 && ordersArray[i].isPlaced == 0 && ordersArray[i].send == 1){
                orderIndex = i;
                ordersArray[i].status = 1;
                ordersArray[i].isPlaced = 1;
                break;
            }   
        }  
        pthread_mutex_unlock(&ordersMutex);
        if (orderIndex != -1) {
            char logBuffer[256];
            sprintf(logBuffer, "Chef %d is preparing order %d", chef->id, ordersArray[orderIndex].id);
            logMessage(logBuffer);
            //sendNotificationToClient(logBuffer);
           
            sleep(5); // Replace with actual matrix inversion time

            // Wait for available oven slot
            pthread_mutex_lock(&ovenSlotsMutex);
            while (ovenSlots <= 0) {
                pthread_cond_wait(&ovenSlotAvailable, &ovenSlotsMutex);
            }
            ovenSlots--;
            pthread_mutex_unlock(&ovenSlotsMutex);

            // Wait for available oven peel
            pthread_mutex_lock(&ovenPeelsMutex);
            while (ovenPeels <= 0) {
                pthread_cond_wait(&ovenPeelAvailable, &ovenPeelsMutex);
            }
            ovenPeels--; // Take a peel
            pthread_mutex_unlock(&ovenPeelsMutex);

            // Place in the oven
            pthread_mutex_lock(&ovenMutex);
            while (ovenCapacity >= MAX_OVEN_CAPACITY) {
                pthread_cond_wait(&ovenAvailable, &ovenMutex);
            }
            ovenCapacity++;
            ordersArray[orderIndex].status = 2; // Mark as being cooked
            pthread_mutex_unlock(&ovenMutex);

            sprintf(logBuffer, "Chef %d placed order %d in the oven", chef->id, ordersArray[orderIndex].id);
            logMessage(logBuffer);
            //sendNotificationToClient(logBuffer);

            // Simulate cooking time
            sleep(2.5); // Replace with actual cooking time

            pthread_mutex_lock(&ovenMutex);
            ovenCapacity--;
            pthread_cond_signal(&ovenAvailable);
            pthread_mutex_unlock(&ovenMutex);

            // Free up oven slot
            pthread_mutex_lock(&ovenSlotsMutex);
            ovenSlots++;
            pthread_cond_signal(&ovenSlotAvailable);
            pthread_mutex_unlock(&ovenSlotsMutex);

            // Free up oven peel
            pthread_mutex_lock(&ovenPeelsMutex);
            ovenPeels++; // Release the peel
            pthread_cond_signal(&ovenPeelAvailable);
            pthread_mutex_unlock(&ovenPeelsMutex);

            // Mark as ready for delivery
            pthread_mutex_lock(&ordersMutex);
            ordersArray[orderIndex].status = 3;
            pthread_cond_signal(&orderAvailable); // Signal deliverers that an order is ready
            pthread_mutex_unlock(&ordersMutex);

            sprintf(logBuffer, "Order %d is ready for delivery by chef %d", ordersArray[orderIndex].id, chef->id);
            logMessage(logBuffer);
            //sendNotificationToClient(logBuffer);
        }
    }
    return NULL;
}

void *delivererRoutine(void *arg) {
    Deliverer *deliverer = (Deliverer *)arg;
    while (1) {
        pthread_mutex_lock(&ordersMutex);
        while (numOrders == 0) {
            pthread_cond_wait(&orderAvailable, &ordersMutex);
        }  
        int ordersCollected = 0;
        for (int i = 0; i < max_order && ordersCollected < MAX_DELIVERY_BAG_CAPACITY; i++) { 
            if (ordersArray[i].status == 3) {
                ordersArray[i].status = 4; // Mark as being delivered
                deliverer->orders[ordersCollected] = ordersArray[i];
                ordersCollected++;
            }
        }
        pthread_mutex_unlock(&ordersMutex);
        if (ordersCollected > 0) {
            char logBuffer[1024] = {0};
            char tempBuffer[256] = {0};
            sprintf(logBuffer, "Deliverer %d is delivering", deliverer->id);
            double totalDistance = 0.0;
            int lastX = 0, lastY = 0;
            for (int j = 0; j < ordersCollected; j++) {
                sprintf(tempBuffer, " order %d (%d,%d)", deliverer->orders[j].id, deliverer->orders[j].x, deliverer->orders[j].y);
                strcat(logBuffer, tempBuffer);
                if (j < ordersCollected - 1) {
                    strcat(logBuffer, ",");
                }
                // Calculate distance from last point to current order point
                totalDistance += calculateDistance(lastX, lastY, deliverer->orders[j].x, deliverer->orders[j].y);
                lastX = deliverer->orders[j].x;
                lastY = deliverer->orders[j].y;
            }
            // Calculate distance from last order point back to (0,0)
            totalDistance += calculateDistance(lastX, lastY, 0, 0);
            logMessage(logBuffer);
            //sendNotificationToClient(logBuffer);
        
            totalDistance = totalDistance / 2;
            sleep((int)totalDistance); 
            pthread_mutex_lock(&ordersMutex);
            sprintf(logBuffer, "Deliverer %d delivered", deliverer->id);
            for (int j = 0; j < ordersCollected; j++) {
               for(int i = 0; i < max_order; i++){
                    if(ordersArray[i].id == deliverer->orders[j].id){
                        ordersArray[i].status = 5;
                    }
                }
                numOrders--; // Decrement the number of active orders
                
                deliverer->totalDelivered++;
                sprintf(tempBuffer, " order %d ", deliverer->orders[j].id);
                strcat(logBuffer, tempBuffer);
                if (j < ordersCollected - 1) {
                    strcat(logBuffer, ",");
                }
                 

               for(int i = 0; i < max_order; i++){
                    if(ordersArray[i].id == deliverer->orders[j].id){
                        memset(&ordersArray[i], 0, sizeof(Order));
                    }
                }
               
                deliveredOrders++;
                 if(deliveredOrders == max_order){
                    sendNotification();
                }
            }
            
            logMessage(logBuffer);
            //sendNotificationToClient(logBuffer);
            pthread_mutex_unlock(&ordersMutex);
        }
    }
    return NULL;
}


void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nserver cancellation signal received\n");
        fclose(logFile);
        exit(EXIT_SUCCESS);
    }
}



int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [server_ip] [portnum] [ChefThreadPoolSize] [DelivererThreadPoolSize] [deliverySpeed]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_ip = argv[1];
    portnum = atoi(argv[2]);
    chefPoolSize = atoi(argv[3]);
    delivererPoolSize = atoi(argv[4]);
    deliverySpeed = atoi(argv[5]);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    chefs = (Chef *)malloc(sizeof(Chef) * chefPoolSize);
    deliverers = (Deliverer *)malloc(sizeof(Deliverer) * delivererPoolSize);
    deliveryThreadPool = (pthread_t *)malloc(sizeof(pthread_t) * delivererPoolSize);
    chefThreadPool = (pthread_t *)malloc(sizeof(pthread_t) * chefPoolSize);


    // Yeni istemci bağlantısını aldıktan sonra iş parçacıklarını başlat
        for (int i = 0; i < chefPoolSize; i++) {
            chefs[i].id = i + 1;
            pthread_create(&chefThreadPool[i], NULL, chefRoutine, &chefs[i]);
        }

        for (int i = 0; i < delivererPoolSize; i++) {
            deliverers[i].id = i + 1;
            deliverers[i].numActiveOrders = 0;
            deliverers[i].totalDelivered = 0;
            pthread_create(&deliveryThreadPool[i], NULL, delivererRoutine, &deliverers[i]);
        }
        //pthread_create(&sendNotificationThread, NULL, sendNotification, NULL);
        logFile = fopen("pideshop.log", "w");
        if (logFile == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }

    printf("PideShop active waiting for connection...\n");
    
    while (1) {


        // Setup socket
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
        
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        
        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);

        int new_max_order;
        sscanf(buffer, "%d", &new_max_order);
        
        if (new_max_order != max_order) {

            
            max_order = new_max_order;
            if (ordersArray != NULL) {
                free(ordersArray);
            }
            ordersArray = (Order *)malloc(sizeof(Order) * max_order);
        }
       
        
        
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        printf("%d new customers Serving...\n", max_order);
        Order *orders = (Order *)malloc(max_order * sizeof(Order));
        read(new_socket, orders, max_order * sizeof(Order));
        close(new_socket);
        close(server_fd);
        pthread_mutex_lock(&ordersMutex);
        for (int i = 0; i < max_order; i++) {
            if (orders[i].status == 0) {
                ordersArray[i] = orders[i];
                ordersArray[i].send = 1;
                clientID = ordersArray[i].clientID;
                ordersArray[i].id = allOrders;
                allOrders++;
                numOrders++;
            }
            
        }
 
        pthread_cond_broadcast(&orderAvailable);
        pthread_mutex_unlock(&ordersMutex);
        free(orders);
        
    }

    fclose(logFile);
    return 0;
}

