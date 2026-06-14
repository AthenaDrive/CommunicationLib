
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

	UDPData getUDPData();

	std::atomic<std::array<int32_t, 4>> bufferTcpSend;
	std::atomic<std::array<int32_t, 4>> bufferTcpRecv;

private:
	std::atomic_bool _stopFlag = false;

	void _updateUDP();
	void _updateTCP();

	UDPData _udpData{};
	std::mutex _udpDataMutex;
	std::jthread _udpDataThread;

	std::mutex _tcpDataMutex;
	std::jthread _tcpDataThread;

	asio::io_context _ioContext;
	asio::ip::udp::socket _socketUDP;
	asio::ip::tcp::socket _socketTCP;
	asio::ip::udp::endpoint _senderEndpointUDP;
	asio::ip::tcp::endpoint _senderEndpointTCP;
	std::array<char, 4 * 64> _bufferUDP;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP