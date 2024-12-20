#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "192.168.162.1" // Địa chỉ IP của server
#define PORT 1234
#define BUFFER_SIZE 100
#define VOL_DATA_MAX    10

typedef struct share_queue{
    int id;
    char message[BUFFER_SIZE];
} share_queue;

share_queue share_data;
int vol_data;

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int temp = 20; 

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Config server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Đã kết nối đến server!\n");

    // Send temperature
    while (1) {
        share_data.id ++;
        sprintf(share_data.message, "%d", temp);
        send(client_socket, &share_data,sizeof(share_queue), 0);
        printf("[Client] Sensor Node %d Gửi nhiệt độ: %d\n",share_data.id, temp);

        // Simulate temperature
        temp = (temp + (rand() % 11 - 5)) % 100;
        if (temp < 0) temp = 0;
        sleep(2);
    }

    // Close socket
    close(client_socket);
    return 0;
}
