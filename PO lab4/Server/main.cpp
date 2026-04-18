#include <iostream>
#include <thread>
#include <vector>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;

void ClientThread(SOCKET clientSocket)
{
	cout << "[Client thread] New client has been connected! Processing...\n";

	// TODO:
	//			receive commands from client
	//			decode Protocol.h
	//			Launch task processing for matrix calculations
	//			send result

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