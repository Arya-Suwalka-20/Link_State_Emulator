#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
using namespace std;

ssize_t recv_all(int sock, void* buf, size_t n) {
    size_t total = 0;
    char* p = (char*)buf;
    while (total < n) {
        ssize_t r = recv(sock, p + total, n - total, 0);
        if (r <= 0) return r;
        total += r;
    }
    return total;
}
int main() {
    // const char* ON_IP = "0.0.0.0";   
    // const int ON_PORT = 5000;      
    char ON_IP[32] = "0.0.0.0";
    int ON_PORT = 5000;
    char VN_IP[32] = "127.0.0.1";
    int VN_UDP_PORT;

    // cout << "Give the ON IP address : ";
    // cin >> ON_IP;
    // cout << "Give the VN IP address : ";
    // cin >> VN_IP;
    cout << "Give the VN port number : ";
    cin >> VN_UDP_PORT; 

    // const char* VN_NAME = "A";
    // const char* VN_IP = "10.0.0.1";
    // int VN_UDP_PORT = 6000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in vn_addr;
    vn_addr.sin_family = AF_INET;
    vn_addr.sin_port = htons(VN_UDP_PORT);
    vn_addr.sin_addr.s_addr = inet_addr(VN_IP);

    if (bind(sock, (struct sockaddr*)&vn_addr, sizeof(vn_addr)) < 0) {
        cerr << "Error binding VN socket.\n";
        close(sock);
        return 1;
    }

    cout << "VN bound to " << VN_IP << ":" << VN_UDP_PORT << endl;


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
    snprintf(message, sizeof(message), "%s %d", VN_IP, VN_UDP_PORT);

    send(sock, message, strlen(message), 0);
    std::cout << "Sent to ON: " << message << std::endl;

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    cout << "Waiting for message from ON..." << endl;
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received < 0) {
        perror("Receive failed");
    } else if (bytes_received == 0) {
        cout << "Connection closed by ON." << endl;
    } else {
        buffer[bytes_received] = '\0'; // null-terminate
        cout << "Message received from ON: " << buffer << endl;
    }

    close(sock);
    std::cout << "Connection closed.\n";

    return 0;
}
