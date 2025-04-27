#include "simpleTcpDebug.hpp"
#include <iostream>
#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

void sendMessageToPort(const char* host, int port, std::string message) {
    message = message + '\n';
    #ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return;
    }
    #endif

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        std::cerr << "Could not create socket. Error: " 
                  #ifdef _WIN32
                  << WSAGetLastError()
                  #else
                  << errno
                  #endif
                  << std::endl;
        #ifdef _WIN32
        WSACleanup();
        #endif
        return;
    }

    sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (sendto(sock, message.c_str(), message.size(), 0, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cerr << "Send failed with error: " 
                  #ifdef _WIN32
                  << WSAGetLastError()
                  #else
                  << errno
                  #endif
                  << std::endl;
        #ifdef _WIN32
        closesocket(sock);
        WSACleanup();
        #else
        close(sock);
        #endif
        return;
    }

    #ifdef _WIN32
    closesocket(sock);
    WSACleanup();
    #else
    close(sock);
    #endif
}