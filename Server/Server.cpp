#include <iostream>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include "../Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;

string getCurrentTime()
{
	auto now = chrono::current_zone()->to_local(chrono::system_clock::now());
	string time = format("{:%T}", now);

	return time;
}

void MirrorMatrix(vector<int>& matrix, uint32_t N, uint32_t startRow, uint32_t endRow)
{
	for (uint32_t i = startRow; i < endRow; ++i)
	{
		uint32_t mirrorRow = N - 1 - i;

		for (uint32_t j = 0; j < N; ++j)
		{
			matrix[mirrorRow * N + j] = matrix[i * N + j];
		}
	}
}

enum class TaskState
{
	IDLE,
	PROCESSING,
	READY
};

void ParalellThreads(vector<int>& matrix, uint32_t N, uint32_t threadsCount)
{
	cout << "[Task Thread] [" << getCurrentTime() << "] Starting parallel processing with " << threadsCount << " threads...\n";
	
	uint32_t halfRows = N / 2;
	if (halfRows == 0 || threadsCount == 0) return;

	uint32_t threads = min(threadsCount, halfRows);

	uint32_t rowsPerThread = halfRows / threads;
	uint32_t remainderRows = halfRows % threads;

	vector<thread> workers;
	uint32_t currentRow = 0;

	for (uint32_t t = 0; t < threads; ++t)
	{
		uint32_t extraRow = (remainderRows > 0) ? 1 : 0;
		uint32_t currentThreadRows = rowsPerThread + extraRow;

		uint32_t startRow = currentRow;
		uint32_t endRow = startRow + currentThreadRows;

		workers.emplace_back(MirrorMatrix, std::ref(matrix), N, startRow, endRow);

		currentRow = endRow;
		if (remainderRows > 0) remainderRows--;
	}
	
	for (auto& worker : workers)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}

	cout << "[Task Thread] [" << getCurrentTime() << "] Parallel processing finished!\n\n";
}

void ClientThread(SOCKET clientSocket, int clientId)
{
	cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] New client has been connected! Waiting for commands...\n\n";

	uint32_t N = 0, threadsCount = 0;
	vector<int> hostMatrix;

	atomic<TaskState> taskState{ TaskState::IDLE };

	while (true)
	{
		MessageHeader header;
		int bytesReceived = recv(clientSocket, (char*)&header, sizeof(MessageHeader), 0);

		if (bytesReceived <= 0) break;

		uint32_t payloadLength = ntohl(header.length);

		if (header.tag == CMD_SEND_CONFIG)
		{
			ConfigPayload config;
			recv(clientSocket, (char*)&config, payloadLength, 0);

			N = ntohl(config.matrix_size);
			threadsCount = ntohl(config.thread_count);

			cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] Configuration read! N=" << N << ", Threads=" << threadsCount << "\n\n";

			MessageHeader ackHeader = { RESP_ACK, htonl(1) };
			uint8_t status = 0;
			send(clientSocket, (char*)&ackHeader, sizeof(MessageHeader), 0);
			send(clientSocket, (char*)&status, 1, 0);
		}
		
		else if (header.tag == CMD_SEND_DATA)
		{
			if (N != 0 && threadsCount != 0)
			{
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
					hostMatrix.resize(N * N);
					for (uint32_t i = 0; i < N * N; ++i)
					{
						hostMatrix[i] = ntohl(networkMatrix[i]);
						
						if (N <= 10)
						{
							cout << hostMatrix[i];
							cout << (((i + 1) % N == 0) ? "\n" : " ");
						}
					}

					cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] Matrix was received and decoded successfully!\n\n";

					MessageHeader dataAckHeader = { RESP_ACK, htonl(1) };
					uint8_t status = 0;
					send(clientSocket, (char*)&dataAckHeader, sizeof(MessageHeader), 0);
					send(clientSocket, (char*)&status, 1, 0);
				}
			}
		}
		
		else if (header.tag == CMD_START)
		{
			cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] CMD_START received. Launching task processing thread...\n\n";

			taskState = TaskState::PROCESSING;

			thread([&hostMatrix, N, threadsCount, &taskState]()
				{
					ParalellThreads(hostMatrix, N, threadsCount);
					taskState = TaskState::READY;
				}).detach();

			MessageHeader startAck = { RESP_STARTED, htonl(0) };
			send(clientSocket, (char*)&startAck, sizeof(MessageHeader), 0);
		}
		
		else if (header.tag == CMD_GET_STATUS)
		{
			TaskState currentState = taskState.load();
			
			if (currentState == TaskState::PROCESSING)
			{
				MessageHeader statusAck = { RESP_STATUS, htonl(1) };
				uint8_t currentStatus = 0;
				send(clientSocket, (char*)&statusAck, sizeof(MessageHeader), 0);
				send(clientSocket, (char*)&currentStatus, 1, 0);
			}
			else if (currentState == TaskState::READY)
			{
				cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] Sending result back to client...\n";

				vector<uint32_t> networkMatrix(N * N);
				for (uint32_t i = 0; i < N * N; ++i)
				{
					networkMatrix[i] = htonl(hostMatrix[i]);
					
					if (N <= 10)
					{
						cout << hostMatrix[i];
						cout << (((i + 1) % N == 0) ? "\n" : " ");
					}
				}

				MessageHeader resultHeader = { RESP_RESULT, htonl(N * N * sizeof(uint32_t)) };
				send(clientSocket, (char*)&resultHeader, sizeof(MessageHeader), 0);
				send(clientSocket, (char*)networkMatrix.data(), N * N * sizeof(uint32_t), 0);

				taskState = TaskState::IDLE;

				cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] All data has been sent!\n\n";
			}
		}
	}

	closesocket(clientSocket);
	cout << "[Client Thread #" << clientId << "] [" << getCurrentTime() << "] Connection has been closed!\n";
}

int main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cerr << "[Main Thread] [" << getCurrentTime() << "] WinSock initialization ERROR!\n";
		return 1;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		cerr << "[Main Thread] [" << getCurrentTime() << "] Socket creation ERROR!\n";
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cerr << "[Main Thread] [" << getCurrentTime() << "] Bind ERROR: " << WSAGetLastError() << "\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cerr << "[Main Thread] [" << getCurrentTime() << "] Listen ERROR: " << WSAGetLastError() << "\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	cout << "[Main Thread] [" << getCurrentTime() << "] Server has been launched! Waiting for connections on PORT " << PORT << "...\n\n";
	
	vector<thread> clientThreads;
	atomic<int> clientIdCounter{ 1 };

	while (true)
	{
		sockaddr_in clientAddress;
		int clientAddressSize = sizeof(clientAddress);

		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressSize);

		if (clientSocket == INVALID_SOCKET)
		{
			cerr << "[Main Thread] [" << getCurrentTime() << "] Accept ERROR: " << WSAGetLastError() << "\n";
			continue;
		}

		int currentClientId = clientIdCounter++;

		char clientIp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
		int clientPort = ntohs(clientAddress.sin_port);

		cout << "[Main Thread] Accepted connection from IP: " << clientIp << ", Port: " << clientPort << " -> Assigned Client ID: #" << currentClientId << "\n";

		clientThreads.emplace_back(thread(ClientThread, clientSocket, currentClientId));

		clientThreads.back().detach();
	}

	closesocket(serverSocket);
	WSACleanup();

	return 0;
}