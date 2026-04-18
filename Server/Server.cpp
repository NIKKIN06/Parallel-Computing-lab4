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
	cout << "[Client thread] New client has been connected! Waiting for commands...\n";

	MessageHeader header;

	int bytesReceived = recv(clientSocket, (char*)&header, sizeof(MessageHeader), 0);

	if (bytesReceived == sizeof(MessageHeader)) {
		uint32_t payloadLength = ntohl(header.length);

		if (header.tag == CMD_SEND_CONFIG) {
			cout << "[Client Thread] Received command CMD_SEND_CONFIG. Length: " << payloadLength << " bytes.\n";

			ConfigPayload config;
			recv(clientSocket, (char*)&config, payloadLength, 0);

			uint32_t N = ntohl(config.matrix_size);
			uint32_t threads = ntohl(config.thread_count);

			std::cout << "[Client Thread] Configuration has been read successfully! N=" << N << ", Threads=" << threads << "\n";

			MessageHeader ackHeader;
			ackHeader.tag = RESP_ACK;
			ackHeader.length = htonl(1);
			uint8_t status = 0;

			send(clientSocket, (char*)&ackHeader, sizeof(MessageHeader), 0);
			send(clientSocket, (char*)&status, 1, 0);
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