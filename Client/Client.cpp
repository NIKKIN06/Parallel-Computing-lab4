#include <iostream>
#include <vector>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <chrono>
#include <thread>
#include "../Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;
const char* SERVER_IP = "127.0.0.1";

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WinSock initialization ERROR!\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation ERROR!\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0)
    {
        cerr << "IP address is not valid!\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Trying to connect to the server " << SERVER_IP << ":" << PORT << "...\n";

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Unable to connect to the server! ERROR: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Successfully connected to the server!\n\n";

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
            cout << "Server has received configuration successfully!\n\n";
        }

        cout << "Generating matrix " << N << "x" << N << "...\n";
        vector<int> matrix(N * N);

        for (uint32_t i = 0; i < N * N; i++)
        {
            matrix[i] = rand() % 100;
            cout << matrix[i];
            cout << (((i + 1) % N == 0) ? "\n" : " ");
        }

        vector<uint32_t> networkMatrix(N * N);
        for (uint32_t i = 0; i < N * N; ++i)
        {
            networkMatrix[i] = htonl(matrix[i]);
        }

        MessageHeader dataHeader;
        dataHeader.tag = CMD_SEND_DATA;
        dataHeader.length = htonl(N * N * sizeof(uint32_t));

        cout << "Matrix sending...\n";
        
        send(clientSocket, (char*)&dataHeader, sizeof(MessageHeader), 0);
        send(clientSocket, (char*)networkMatrix.data(), N * N * sizeof(uint32_t), 0);

        int dataBytesReceived = recv(clientSocket, (char*)&responseHeader, sizeof(MessageHeader), 0);

        if (dataBytesReceived > 0 && responseHeader.tag == RESP_ACK)
        {
            uint8_t status;
            recv(clientSocket, (char*)&status, 1, 0);

            if (status == 0)
            {
                cout << "Server has received matrix successfully!\n\n";
            }
        }
    }

    MessageHeader startHeader;
    startHeader.tag = CMD_START;
    startHeader.length = htonl(0);

    cout << "Sending CMD_START to launch processing...\n";
    send(clientSocket, (char*)&startHeader, sizeof(MessageHeader), 0);

    MessageHeader startResp;
    recv(clientSocket, (char*)&startResp, sizeof(MessageHeader), 0);

    if (startResp.tag == RESP_STARTED)
    {
        cout << "Server confirmed: The processing has been started!\n\n";
    }

    bool isDone = false;

    while (!isDone)
    {
        MessageHeader statusReq;
        statusReq.tag = CMD_GET_STATUS;
        statusReq.length = htonl(0);

        send(clientSocket, (char*)&statusReq, sizeof(MessageHeader), 0);

        MessageHeader statusResp;
        int bytesReceived = recv(clientSocket, (char*)&statusResp, sizeof(MessageHeader), 0);

        if (bytesReceived <= 0)
        {
            cerr << "Connection lost while waiting for status.\n";
            break;
        }

        if (statusResp.tag == RESP_STATUS)
        {
            uint8_t currentStatus;
            recv(clientSocket, (char*)&currentStatus, 1, 0);

            cout << "Current status: in progress. Waiting 10 second...\n";

            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
        else if (statusResp.tag == RESP_RESULT)
        {
            cout << "\n[Client] Server has finished! Receiving final result...\n";

            uint32_t payloadLength = ntohl(statusResp.length);

            vector<uint32_t> networkMatrix(N * N);
            char* buffer = (char*)networkMatrix.data();
            uint32_t totalReceived = 0;

            while (totalReceived < payloadLength)
            {
                int bytes = recv(clientSocket, buffer + totalReceived, payloadLength - totalReceived, 0);
                if (bytes <= 0) break;
                totalReceived += bytes;
            }

            if (totalReceived == payloadLength)
            {
                vector<int> finalMatrix(N * N);

                cout << "\n<-- Mirrored matrix from Server -->\n\n";

                for (uint32_t i = 0; i < N * N; ++i)
                {
                    finalMatrix[i] = ntohl(networkMatrix[i]);
                    cout << finalMatrix[i];
                    cout << (((i + 1) % N == 0) ? "\n" : " ");
                }
                cout << "\n";
            }

            isDone = true;
        }
    }

    system("pause");

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}