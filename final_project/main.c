#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define FIFO_FILE   "./myfifo"
#define DATA_SIZE   1024
#define TEMP_AVG    30
#define VOL_DATA_MAX    10

typedef struct share_queue{
    int id;
    char message[DATA_SIZE]; // store message from client
} share_queue;

share_queue share_data[VOL_DATA_MAX];

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

int vol_data = 0;
int server_fd, new_socket_fd;
int fd_fifo;
char recvbuff[256];
int sequence = 0;
char fifo_buffer[DATA_SIZE];

pthread_t connection_thread, data_thread, manager_thread;

void write_to_log(const char *message, int sequence) {
    int fd_log = open("gateway.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd_log == -1) {
        perror("Error opening gateway.log");
        return;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    char log_entry[DATA_SIZE];
    snprintf(log_entry, sizeof(log_entry), "%d %s %s\n", sequence, timestamp, message);
    
    write(fd_log, log_entry, strlen(log_entry));
    close(fd_log);
}

void write_to_fifo(const char *message) {
    pthread_mutex_lock(&mtx);
    write(fd_fifo, message, strlen(message) + 1);
    pthread_mutex_unlock(&mtx);
}

static void *handle_connection(void *args) {
    while (1) {
        if (vol_data >= VOL_DATA_MAX )
        {
            printf("Queue is full !!!\n");
            sleep(1);
        }
        else {
        pthread_mutex_lock(&mtx);
        int n_read = read(new_socket_fd, &share_data[vol_data], DATA_SIZE);
        if (n_read > 0) {
            printf("Received Node %d with Temp: %s\n",share_data->id, share_data->message);
            vol_data++;
        } else if (n_read == 0) {
            printf("Client disconnected.\n");
            close(new_socket_fd);
            pthread_exit(NULL);
        } else {
            perror("Read error");
        }
        pthread_mutex_unlock(&mtx);
        sleep(1);
        }
    }
    return NULL;
}

static void *handle_data(void *args) {
    share_queue data_receive;
    char message[256];
    int temp;
    while (1) {      
        if (vol_data > 0)
        {
            pthread_mutex_lock(&mtx);
            vol_data--;
            data_receive = share_data[vol_data];      
            pthread_mutex_unlock(&mtx);
            temp = atoi(data_receive.message);
             if (temp > TEMP_AVG) {
                snprintf(message, sizeof(message), "Sensor Node %d is so hot (with temp %d)",data_receive.id, temp);
            } else {
                snprintf(message, sizeof(message), "Sensor Node %d is so cool (with temp %d)",data_receive.id, temp);
            }
             write_to_fifo(message);
        }     
        sleep(1);
    }
    return NULL;
}

// handle_manager in development
static void *handle_manager(void *args) {
    char message[256];
    const char *events[] = {
        "Connection to SQL server established",
        "New table 'sensor_data' created",
        "Connection to SQL server lost",
        "Unable to connect to SQL server"
    };
    for (int i = 0; i < 4; i++) {
        snprintf(message, sizeof(message), "%s", events[i]);
        write_to_fifo(message);
        sleep(2);
    }
    return NULL;
}

void run_log_process() {
    mkfifo(FIFO_FILE, 0666);
    fd_fifo = open(FIFO_FILE, O_RDONLY);
    if (fd_fifo == -1) {
        perror("Error opening FIFO in log process");
        exit(1);
    }

    printf("Log process running...\n");
    while (1) {
        memset(fifo_buffer, 0, DATA_SIZE);
        ssize_t bytes_read = read(fd_fifo, fifo_buffer, DATA_SIZE - 1);
        if (bytes_read > 0) {
            printf("Log received: %s\n", fifo_buffer);
            write_to_log(fifo_buffer, sequence++);
        }
    }
    close(fd_fifo);
}

void run_main_process() {
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1234);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    listen(server_fd, 5);
    printf("Server listening on port 1234...\n");

    new_socket_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (new_socket_fd < 0) {
        perror("Accept failed");
        exit(1);
    }
    printf("Connection established with client.\n");

    fd_fifo = open(FIFO_FILE, O_WRONLY | O_CREAT, 0666);
    if (fd_fifo == -1) {
        perror("Error opening FIFO");
        exit(1);
    }

    pthread_create(&connection_thread, NULL, handle_connection, NULL);
    pthread_create(&data_thread, NULL, handle_data, NULL);
    pthread_create(&manager_thread, NULL, handle_manager, NULL);

    pthread_join(connection_thread, NULL);
    pthread_join(data_thread, NULL);
    pthread_join(manager_thread, NULL);

    close(server_fd);
}

int main() {
    pid_t pid = fork();
    if (pid == 0) { // Log process
        run_log_process();
        exit(0);
    } else if (pid > 0) { // Main process
        run_main_process();
    } else {
        perror("Fork failed");
        exit(1);
    }
    return 0;
}
