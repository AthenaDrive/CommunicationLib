
#ifndef COMPUTERCONTROLL_ETHERNET_HPP
#define COMPUTERCONTROLL_ETHERNET_HPP

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <array>
#include <chrono>

#include <asio.hpp>

#include "CommunicationConfig.hpp"

class Ethernet {
public:
	Ethernet();
	~Ethernet();

	std::vector<UDPData> getUDPData();

	// Temporary for testing.
	std::atomic<std::array<int32_t, 4>> bufferTcpSend;
	std::atomic<std::array<int32_t, 4>> bufferTcpRecv;

private:
	std::atomic_bool _stopFlag = false;

	void _updateUDP();
	UDPData readSingleUDP(int &offset, int len);
	void _updateTCP();

	std::vector<UDPData> _udpData{};
	std::mutex _udpDataMutex;
	std::jthread _udpDataThread;

	std::mutex _tcpDataMutex;
	std::jthread _tcpDataThread;

	asio::io_context _ioContext;
	asio::ip::udp::endpoint _senderEndpointUDP;
	asio::ip::tcp::endpoint _senderEndpointTCP;
	asio::ip::udp::socket _socketUDP;
	asio::ip::tcp::socket _socketTCP;
	std::array<char, 4 * 1024> _bufferUDP;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP