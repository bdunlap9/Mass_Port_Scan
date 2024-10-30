#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <set>

const int MAX_THREADS = 100;

void scanPort(const std::string& ip, int port, int timeout_sec) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    if (select(sock + 1, nullptr, &fdset, nullptr, &timeout) > 0) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            std::cout << "Port " << port << " is open on " << ip << std::endl;
        }
    }

    close(sock);
}

void scanIP(const std::string& ip, const std::set<int>& ports, int timeout_sec) {
    std::vector<std::thread> threads;

    for (int port : ports) {
        if (threads.size() >= MAX_THREADS) {
            for (auto& th : threads) th.join();
            threads.clear();
        }
        threads.emplace_back(scanPort, ip, port, timeout_sec);
    }

    for (auto& th : threads) th.join();
}

std::set<int> parsePorts(const std::string& portInput) {
    std::set<int> ports;
    std::istringstream ss(portInput);
    std::string token;

    while (std::getline(ss, token, ',')) {
        if (token.find('-') != std::string::npos) {
            size_t dashPos = token.find('-');
            int start = std::stoi(token.substr(0, dashPos));
            int end = std::stoi(token.substr(dashPos + 1));
            for (int port = start; port <= end; ++port) {
                if (port >= 1 && port <= 65535) {
                    ports.insert(port);
                }
            }
        } else {
            int port = std::stoi(token);
            if (port >= 1 && port <= 65535) {
                ports.insert(port);
            }
        }
    }

    return ports;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <IP_ADDRESS> <PORTS> <TIMEOUT_IN_SECONDS>" << std::endl;
        std::cerr << "PORTS can be a single port (80), a range (1-1024), or a comma-separated list (22,80,443)." << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    std::string portInput = argv[2];
    int timeout_sec = std::stoi(argv[3]);

    std::set<int> ports = parsePorts(portInput);
    if (ports.empty()) {
        std::cerr << "No valid ports specified." << std::endl;
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    scanIP(ip, ports, timeout_sec);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Scan completed in " << elapsed.count() << " seconds." << std::endl;

    return 0;
}
