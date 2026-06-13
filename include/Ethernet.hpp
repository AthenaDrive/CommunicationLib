
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

private:
	std::atomic_bool _stopFlag = false;

	void _readUDP();
	void _sendUDP();
	void _readTCP();
	void _sendTCP();

	UDPData _udpData{};
	std::mutex _udpDataMutex;
	std::jthread _udpDataThread;

	asio::io_context _io_context;
	asio::ip::udp::socket _socket;
	asio::ip::udp::endpoint _sender_endpoint;
	std::array<char, 4 * 64> _buffer;

	std::chrono::steady_clock::time_point _lastPacketTime;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP