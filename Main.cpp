#include <iostream>
#include <future>
#include <vector>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <set>

const int MAX_FUTURES = 100;

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
    std::vector<std::future<void>> futures;

    for (int port : ports) {
        if (futures.size() >= MAX_FUTURES) {
            for (auto& fut : futures) fut.get();
            futures.clear();
        }
        futures.emplace_back(std::async(std::launch::async, scanPort, ip, port, timeout_sec));
    }

    for (auto& fut : futures) fut.get();
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

void scanBatchIPs(const std::set<int>& ports, int timeout_sec, int batchSize) {
    for (uint32_t i = 0; i <= 0xFFFFFFFF; i += batchSize) {
        std::vector<std::future<void>> futures;

        for (int j = 0; j < batchSize && (i + j) <= 0xFFFFFFFF; ++j) {
            struct in_addr addr;
            addr.s_addr = htonl(i + j);
            std::string ip = inet_ntoa(addr);
            futures.emplace_back(std::async(std::launch::async, scanIP, ip, ports, timeout_sec));
        }

        for (auto& fut : futures) fut.get();
    }
}

void printUsage(const std::string& programName) {
    std::cerr << "Usage: " << programName << " <IP_ADDRESS|ALL> <PORTS> <TIMEOUT_IN_SECONDS> [BATCH_SIZE]" << std::endl;
    std::cerr << "PORTS can be a single port (80), a range (1-1024), or a comma-separated list (22,80,443)." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        printUsage(argv[0]);
        return 1;
    }

    std::string ip = argv[1];
    std::string portInput = argv[2];
    int timeout_sec = std::stoi(argv[3]);
    int batchSize = (argc == 5) ? std::stoi(argv[4]) : 100;

    std::set<int> ports = parsePorts(portInput);
    if (ports.empty()) {
        std::cerr << "No valid ports specified." << std::endl;
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    if (ip == "ALL") {
        scanBatchIPs(ports, timeout_sec, batchSize);
    } else {
        scanIP(ip, ports, timeout_sec);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Scan completed in " << elapsed.count() << " seconds." << std::endl;

    return 0;
}
