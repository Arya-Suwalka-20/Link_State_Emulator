#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <csignal>
using namespace std;

int tcp_sock = -1, udp_sock = -1;
bool running = true;

struct Neighbor {
    char name;
    string ip;
    int port;
    int cost;
};

struct LSP {
    char origin;
    uint32_t seq;
    vector<pair<char, int>> links;
};

// link-state database: node → (sequence number, list of (neighbor, cost))
map<char, pair<uint32_t, vector<pair<char, int>>>> lsp_db;

void handle_sigint(int) {
    running = false;
    cout << "\n🛑 Ctrl-C detected, shutting down...\n";
}

// ---------- helper: get local IP ----------
string get_local_ip() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in tmp{};
    tmp.sin_family = AF_INET;
    tmp.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &tmp.sin_addr);
    connect(s, (sockaddr*)&tmp, sizeof(tmp));
    sockaddr_in loc{};
    socklen_t l = sizeof(loc);
    getsockname(s, (sockaddr*)&loc, &l);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &loc.sin_addr, buf, sizeof(buf));
    close(s);
    return string(buf);
}

// ---------- serialize / deserialize ----------
vector<uint8_t> serialize_lsp(const LSP& l) {
    vector<uint8_t> pkt;
    pkt.push_back((uint8_t)l.origin);
    uint32_t seqn = htonl(l.seq);
    pkt.insert(pkt.end(), (uint8_t*)&seqn, (uint8_t*)&seqn + 4);
    pkt.push_back((uint8_t)l.links.size());
    for (auto& p : l.links) {
        pkt.push_back((uint8_t)p.first);
        uint32_t c = htonl(p.second);
        pkt.insert(pkt.end(), (uint8_t*)&c, (uint8_t*)&c + 4);
    }
    return pkt;
}

bool deserialize_lsp(const uint8_t* b, int len, LSP& o) {
    if (len < 6) return false;
    int i = 0;
    o.origin = b[i++];
    uint32_t s;
    memcpy(&s, b + i, 4);
    i += 4;
    o.seq = ntohl(s);
    uint8_t n = b[i++];
    if (len < 6 + n * 5) return false;
    o.links.clear();
    for (int k = 0; k < n; k++) {
        char nb = b[i++];
        uint32_t c;
        memcpy(&c, b + i, 4);
        i += 4;
        o.links.push_back({nb, ntohl(c)});
    }
    return true;
}

// ---------- print DB ----------
void print_db() {
    cout << "\n=== Link-State DB ===\n";
    for (auto& x : lsp_db) {
        cout << x.first << "(seq " << x.second.first << "): ";
        bool f = true;
        for (auto& p : x.second.second) {
            if (!f) cout << ", ";
            cout << p.first << "=" << p.second;
            f = false;
        }
        cout << "\n";
    }
    cout << "=====================\n";
}

// ---------- topology matrix printer ----------
void print_topology_matrix(const map<char, vector<pair<char, int>>>& adj) {
    vector<char> nodes;
    for (auto& x : adj) nodes.push_back(x.first);
    sort(nodes.begin(), nodes.end());

    if (nodes.empty()) return;
    cout << "\n# Topology Matrix (in ON config format)\n";

    for (size_t i = 0; i < nodes.size(); i++) {
        char src = nodes[i];
        cout << "# Costs from " << src;
        if (i + 1 < nodes.size()) {
            cout << " to ";
            for (size_t j = i + 1; j < nodes.size(); j++) {
                cout << nodes[j];
                if (j + 1 < nodes.size()) cout << ", ";
            }
        }
        cout << "\n";

        // Indentation
        for (size_t t = 0; t < i; t++) cout << "\t";

        for (size_t j = i + 1; j < nodes.size(); j++) {
            char dst = nodes[j];
            int cost = -1;

            auto it = adj.find(src);
            if (it != adj.end()) {
                for (auto& p : it->second) {
                    if (p.first == dst) {
                        cost = p.second;
                        break;
                    }
                }
            }

            cout << cost;
            if (j + 1 < nodes.size()) cout << "\t";
        }
        cout << "\n";
    }
}

// ---------- parse LINK-STATE message from ON ----------
void parse_linkstate(const string& buf, vector<Neighbor>& neigh) {
    neigh.clear();
    int len = buf.size();
    for (int i = 1; i < len; i++) {
        if (buf[i] == '(') {
            string nm = "", ip = "", port = "", cost = "";
            int f = 0;
            i++;
            while (i < len && buf[i] != ')') {
                if (buf[i] == '\'') {
                    i++;
                    continue;
                }
                if (buf[i] == ',') {
                    f++;
                    i++;
                    continue;
                }
                if (buf[i] != ' ') {
                    if (f == 0)
                        nm += buf[i];
                    else if (f == 1)
                        ip += buf[i];
                    else if (f == 2)
                        port += buf[i];
                    else if (f == 3)
                        cost += buf[i];
                }
                i++;
            }
            if (!nm.empty()) {
                Neighbor nb;
                nb.name = nm[0];
                nb.ip = ip;
                nb.port = stoi(port);
                nb.cost = stoi(cost);
                neigh.push_back(nb);
            }
        }
    }
}

// ---------- main ----------
int main() {
    signal(SIGINT, handle_sigint);

    char ON_IP[32];
    int ON_PORT = 5000;
    cout << "Give the ON IP address: ";
    cin >> ON_IP;

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    string VN_IP = get_local_ip();
    sockaddr_in vn{};
    vn.sin_family = AF_INET;
    vn.sin_port = htons(0);
    inet_pton(AF_INET, VN_IP.c_str(), &vn.sin_addr);
    bind(tcp_sock, (sockaddr*)&vn, sizeof(vn));

    sockaddr_in assigned{};
    socklen_t alen = sizeof(assigned);
    getsockname(tcp_sock, (sockaddr*)&assigned, &alen);
    int VN_UDP_PORT = ntohs(assigned.sin_port);

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(ON_PORT);
    inet_pton(AF_INET, ON_IP, &serv.sin_addr);
    connect(tcp_sock, (sockaddr*)&serv, sizeof(serv));

    char msg[128];
    snprintf(msg, sizeof(msg), "%s %d", VN_IP.c_str(), VN_UDP_PORT);
    send(tcp_sock, msg, strlen(msg), 0);
    cout << "Sent CONNECT to ON: " << msg << endl;

    // Receive first LINK-STATE
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int n = recv(tcp_sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        cerr << "No LINK-STATE from ON\n";
        return 0;
    }
    buffer[n] = '\0';

    vector<Neighbor> neighbors;
    parse_linkstate(buffer, neighbors);

    char myName = '?';
    for (auto& nb : neighbors) {
        if (nb.cost == 0) {
            myName = nb.name;
            break;
        }
    }
    if (myName == '?') {
        cerr << "⚠️ Could not determine my node name from LINK-STATE!\n";
    }

    cout << "Initial topology:\n";
    for (auto& nb : neighbors)
        cout << "  " << nb.name << " " << nb.ip << ":" << nb.port << " cost=" << nb.cost << "\n";

    // open UDP
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in u{};
    u.sin_family = AF_INET;
    u.sin_port = htons(VN_UDP_PORT);
    inet_pton(AF_INET, VN_IP.c_str(), &u.sin_addr);
    bind(udp_sock, (sockaddr*)&u, sizeof(u));
    cout << "UDP bound on " << VN_IP << ":" << VN_UDP_PORT << "\n";

    // ---- select loop ----
    uint32_t seq_no = 1;
    timeval tv{10, 0};

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(udp_sock, &rfds);
        FD_SET(tcp_sock, &rfds);
        int maxfd = max(udp_sock, tcp_sock);

        int rv = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);

        if (rv == 0) {  // timeout: send own LSP
            LSP l;
            l.origin = myName;
            l.seq = seq_no++;
            for (auto& nb : neighbors)
                l.links.push_back({nb.name, nb.cost});

            vector<uint8_t> pkt = serialize_lsp(l);
            for (auto& nb : neighbors) {
                sockaddr_in d{};
                d.sin_family = AF_INET;
                d.sin_port = htons(nb.port);
                inet_pton(AF_INET, nb.ip.c_str(), &d.sin_addr);
                sendto(udp_sock, pkt.data(), pkt.size(), 0, (sockaddr*)&d, sizeof(d));
            }
            cout << "[LSP] sent seq=" << l.seq << " to " << neighbors.size() << " neighbours\n";
            tv.tv_sec = 10;
            tv.tv_usec = 0;
        } else if (rv > 0) {
            if (FD_ISSET(udp_sock, &rfds)) {
                uint8_t buf[1024];
                sockaddr_in src{};
                socklen_t sl = sizeof(src);
                int r = recvfrom(udp_sock, buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
                if (r > 0) {
                    LSP in;
                    if (deserialize_lsp(buf, r, in)) {
                        auto it = lsp_db.find(in.origin);
                        if (it == lsp_db.end() || in.seq > it->second.first) {
                            lsp_db[in.origin] = {in.seq, in.links};
                            // flood to all neighbours
                            for (auto& nb : neighbors) {
                                sockaddr_in d{};
                                d.sin_family = AF_INET;
                                d.sin_port = htons(nb.port);
                                inet_pton(AF_INET, nb.ip.c_str(), &d.sin_addr);
                                sendto(udp_sock, buf, r, 0, (sockaddr*)&d, sizeof(d));
                            }
                            cout << "[LSP] new update from " << in.origin << " seq=" << in.seq << "\n";
                            print_db();

                            // build adjacency and print matrix
                            map<char, vector<pair<char, int>>> adj;
                            for (auto& x : lsp_db)
                                adj[x.first] = x.second.second;
                            print_topology_matrix(adj);
                        }
                    }
                }
                tv.tv_sec = 10;
                tv.tv_usec = 0;
            }

            if (FD_ISSET(tcp_sock, &rfds)) {
                // new LINK-STATE from ON
                int r = recv(tcp_sock, buffer, sizeof(buffer) - 1, 0);
                if (r > 0) {
                    buffer[r] = '\0';
                    parse_linkstate(buffer, neighbors);
                    cout << "Topology updated from ON:\n";
                    for (auto& nb : neighbors)
                        cout << "  " << nb.name << " " << nb.ip << ":" << nb.port << " cost=" << nb.cost << "\n";

                    // resend LSP with new info
                    LSP l;
                    l.origin = myName;
                    l.seq = seq_no++;
                    for (auto& nb : neighbors)
                        l.links.push_back({nb.name, nb.cost});

                    vector<uint8_t> pkt = serialize_lsp(l);
                    for (auto& nb : neighbors) {
                        sockaddr_in d{};
                        d.sin_family = AF_INET;
                        d.sin_port = htons(nb.port);
                        inet_pton(AF_INET, nb.ip.c_str(), &d.sin_addr);
                        sendto(udp_sock, pkt.data(), pkt.size(), 0, (sockaddr*)&d, sizeof(d));
                    }
                    cout << "Immediate LSP sent after ON update.\n";
                } else if (r == 0) {
                    cout << "ON closed connection.\n";
                    running = false;
                }
            }
        }
    }

    close(udp_sock);
    close(tcp_sock);
    return 0;
}
