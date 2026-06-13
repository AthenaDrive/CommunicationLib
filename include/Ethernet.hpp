
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

	void update();
	int getIteration();
	float getPosition();

	UDPData _udpData;

private:
	std::jthread _udpReceiveThread;
	std::atomic_bool _stopFlag = false;

	asio::io_context _io_context;
	asio::ip::udp::socket _socket;
	asio::ip::udp::endpoint _sender_endpoint;
	std::array<char, 4 * 64> _buffer;

	std::chrono::steady_clock::time_point _lastPacketTime;
	int _packetCount = 0;
	double _sumDt = 0;
	double _sumDtSq = 0;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP