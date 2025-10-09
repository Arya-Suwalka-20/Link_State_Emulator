#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    const char* ON_IP = "0.0.0.0";   
    const int ON_PORT = 5000;           

    const char* VN_NAME = "A";
    const char* VN_IP = "10.0.0.1";
    int VN_UDP_PORT = 6000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }


    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ON_PORT);
    inet_pton(AF_INET, ON_IP, &server_addr.sin_addr);


    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    char message[128];
    snprintf(message, sizeof(message), "%s %s %d hi", VN_NAME, VN_IP, VN_UDP_PORT);

    send(sock, message, strlen(message), 0);
    std::cout << "Sent to ON: " << message << std::endl;

    close(sock);
    std::cout << "Connection closed.\n";

    return 0;
}
