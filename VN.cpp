#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <csignal>   // <-- for signal handling
using namespace std;

int sock = -1; // make global so handler can access

// ---------- Signal handler ----------
void handle_sigint(int sig) {
    cout << "\n🛑 Keyboard interrupt (Ctrl + C) detected!" << endl;
    if (sock != -1) {
        close(sock);
        cout << "🔒 Socket closed. VN shutting down gracefully." << endl;
    }
    exit(0); // terminate cleanly
}
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

string get_local_ip() {
    int temp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_sock < 0) return "127.0.0.1";

    sockaddr_in temp_addr{};
    temp_addr.sin_family = AF_INET;
    temp_addr.sin_port = htons(80); // arbitrary
    inet_pton(AF_INET, "8.8.8.8", &temp_addr.sin_addr);

    connect(temp_sock, (struct sockaddr*)&temp_addr, sizeof(temp_addr));

    sockaddr_in local_addr{};
    socklen_t addr_len = sizeof(local_addr);
    getsockname(temp_sock, (struct sockaddr*)&local_addr, &addr_len);

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str));
    close(temp_sock);
    return string(ip_str);
}

int main() {
    // const char* ON_IP = "0.0.0.0";   
    // const int ON_PORT = 5000;  
    signal(SIGINT, handle_sigint);

    char ON_IP[32] = "10.51.12.12";
    int ON_PORT = 5000;
    // char VN_IP[32] = "10.51.27.185";
    // int VN_UDP_PORT;
    
    // cout << "Give the ON IP address : ";
    // cin >> ON_IP;
    // cout << "Give the VN IP address : ";
    // cin >> VN_IP;
    // cout << "Give the VN port number : ";
    // cin >> VN_UDP_PORT; 

    // const char* VN_NAME = "A";
    // const char* VN_IP = "10.0.0.1";
    // int VN_UDP_PORT = 6000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // sockaddr_in vn_addr;
    // vn_addr.sin_family = AF_INET;
    // vn_addr.sin_port = htons(VN_UDP_PORT);
    // vn_addr.sin_addr.s_addr = inet_addr(VN_IP);

    // Automatically pick VN IP and port
    string VN_IP = get_local_ip();
    sockaddr_in vn_addr{};
    vn_addr.sin_family = AF_INET;
    vn_addr.sin_port = htons(0);  // 0 lets OS pick an available port
    inet_pton(AF_INET, VN_IP.c_str(), &vn_addr.sin_addr);

    if (bind(sock, (struct sockaddr*)&vn_addr, sizeof(vn_addr)) < 0) {
        cerr << "Error binding VN socket.\n";
        close(sock);
        return 1;
    }

    // Find assigned port
    sockaddr_in assigned_addr{};
    socklen_t len = sizeof(assigned_addr);
    getsockname(sock, (struct sockaddr*)&assigned_addr, &len);
    int VN_UDP_PORT = ntohs(assigned_addr.sin_port);

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
    snprintf(message, sizeof(message), "%s %d", VN_IP.c_str(), VN_UDP_PORT);

    send(sock, message, strlen(message), 0);
    std::cout << "Sent to ON: " << message << std::endl;
    while (true){
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

        vector<pair<char, int>> neighbor_info;
        int leng = strlen(buffer);
        bool nodestart = false;
        for(int i=1; i<leng; i++){
            if(buffer[i]=='('){
                nodestart = true;
                string nodename = "";
                string costy = "";
                int tupleInd = 0;
                i++;
                while(i < leng && buffer[i]!=')'){
                    if (buffer[i] == '\'') { // skip quote marks
                        i++;
                        continue;
                    }
                    if(buffer[i]==','){
                        if(tupleInd==3) tupleInd = 0;
                        else tupleInd += 1;
                        i++;
                        continue;
                    }

                    if (buffer[i] != ' ' && buffer[i] != ')') {
                        if (tupleInd == 0) nodename += buffer[i];   
                        else if (tupleInd == 3) costy += buffer[i];  
                    }

                    i++;
                    
                }
                if (!nodename.empty() && !costy.empty()) {
                    char node_char = nodename[0];   // convert string to single char
                    int cost_val = stoi(costy);      // convert string to int
                    neighbor_info.push_back({node_char, cost_val});
                }
            
            }
        }
    }

    return 0;
}
