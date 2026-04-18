#include <iostream>
#include <vector>
#include <winsock2.h>
#include <WS2tcpip.h>
#include "../Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;
const char* SERVER_IP = "127.0.0.1";

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WinSock initialization ERROR!\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation ERROR!\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        cerr << "IP address is not valid!\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Trying to connect to the server " << SERVER_IP << ":" << PORT << "...\n";

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Unable to connect to the server! ERROR: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Successfully connected to the server!\n";

    ConfigPayload configuration;
    uint32_t N = 10;
    uint32_t threads = 4;

    configuration.matrix_size = htonl(N);
    configuration.thread_count = htonl(threads);

    MessageHeader header;
    header.tag = CMD_SEND_CONFIG;
    header.length = htonl(sizeof(ConfigPayload));

    cout << "Sending configuration (N=" << N << ", Threads=" << threads << ")...\n";

    send(clientSocket, (char*)&header, sizeof(MessageHeader), 0);
    send(clientSocket, (char*)&configuration, sizeof(ConfigPayload), 0);

    MessageHeader responseHeader;
    int bytesReceived = recv(clientSocket, (char*)&responseHeader, sizeof(MessageHeader), 0);

    if (bytesReceived > 0 && responseHeader.tag == RESP_ACK)
    {
        uint8_t status;
        recv(clientSocket, (char*)&status, 1, 0);

        if (status == 0)
        {
            cout << "Server has received configuration successfully!\n";
        }
    }

    // TODO: 
    //      Generate matrix
    //      send configuration (0x01)
    //      send data (0x02)
    //      send start command (0x03) and wait for result (0x04)

    system("pause");

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}