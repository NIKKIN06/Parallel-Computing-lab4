#include <iostream>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include "../Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;

void ClientThread(SOCKET clientSocket)
{
	cout << "[Client thread] New client has been connected! Waiting for commands...\n\n";

	MessageHeader header;
	uint32_t N = 0, threads = 0;

	int bytesReceived = recv(clientSocket, (char*)&header, sizeof(MessageHeader), 0);

	if (bytesReceived == sizeof(MessageHeader)) {
		uint32_t payloadLength = ntohl(header.length);

		if (header.tag == CMD_SEND_CONFIG) {
			cout << "[Client Thread] Received command CMD_SEND_CONFIG. Length: " << payloadLength << " bytes.\n";

			ConfigPayload config;
			recv(clientSocket, (char*)&config, payloadLength, 0);

			N = ntohl(config.matrix_size);
			threads = ntohl(config.thread_count);

			cout << "[Client Thread] Configuration has been read successfully! N=" << N << ", Threads=" << threads << "\n\n";

			MessageHeader ackHeader;
			ackHeader.tag = RESP_ACK;
			ackHeader.length = htonl(1);
			uint8_t status = 0;

			send(clientSocket, (char*)&ackHeader, sizeof(MessageHeader), 0);
			send(clientSocket, (char*)&status, 1, 0);
		}
		
		bytesReceived = recv(clientSocket, (char*)&header, sizeof(MessageHeader), 0);

		if (bytesReceived == sizeof(MessageHeader) && header.tag == CMD_SEND_DATA)
		{
			uint32_t matrixDataLength = ntohl(header.length);
			cout << "[Client Thread] Received command CMD_SEND_DATA. Waiting for bytes: " << matrixDataLength << "\n";

			if (N != 0 && threads != 0)
			{
				vector<uint32_t> networkMatrix(N * N);
				char* buffer = (char*)networkMatrix.data();

				uint32_t totalReceived = 0;
				while (totalReceived < matrixDataLength)
				{
					int bytes = recv(clientSocket, buffer + totalReceived, matrixDataLength - totalReceived, 0);

					if (bytes <= 0)
					{
						cerr << "[Client Thread] Error or disconnect occurred during data transmission.\n";
						break;
					}

					totalReceived += bytes;
				}

				if (totalReceived == matrixDataLength)
				{
					vector<int> hostMatrix(N * N);
					for (uint32_t i = 0; i < N * N; ++i)
					{
						hostMatrix[i] = ntohl(networkMatrix[i]);
						cout << hostMatrix[i];
						cout << (((i + 1) % N == 0) ? "\n" : " ");
					}

					cout << "[Client Thread] Matrix was received and decoded successfully!\n\n";

					MessageHeader dataAckHeader;
					dataAckHeader.tag = RESP_ACK;
					dataAckHeader.length = htonl(1);
					uint8_t status = 0;

					send(clientSocket, (char*)&dataAckHeader, sizeof(MessageHeader), 0);
					send(clientSocket, (char*)&status, 1, 0);
				}
			}
			else
			{
				cerr << "Firstly, Client have to send configuration data (N & threads)!\n\n";
			}
		}
	}

	closesocket(clientSocket);
	cout << "[Client thread] Connection has been closed!\n";
}

int main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cerr << "WinSock initialization ERROR!\n";
		return 1;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		cerr << "Socket creation ERROR!\n";
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cerr << "Bind ERROR: " << WSAGetLastError() << "\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cerr << "Listen ERROR: " << WSAGetLastError() << "\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	cout << "Server has been launched! Waiting for connections on PORT " << PORT << "...\n";

	vector<thread> clientThreads;

	while (true)
	{
		sockaddr_in clientAddress;
		int clientAddressSize = sizeof(clientAddress);

		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressSize);

		if (clientSocket == INVALID_SOCKET)
		{
			cerr << "Accept ERROR: " << WSAGetLastError() << "\n";
			continue;
		}

		clientThreads.emplace_back(thread(ClientThread, clientSocket));

		clientThreads.back().detach();
	}

	closesocket(serverSocket);
	WSACleanup();

	return 0;
}