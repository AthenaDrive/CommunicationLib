
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

	std::vector<UDPDataFromPeripheral> getUDPData();

	// Temporary for testing.
	std::atomic_uint32_t numTCPReads = 0;
	void setTCPSend(const std::vector<uint8_t> &vec);
	void getTCPRecv(std::vector<uint8_t> &vec);
	std::atomic_bool shouldSendTCP{false};

private:
	std::atomic_bool _stopFlag = false;

	void _updateUDP();
	UDPDataFromPeripheral readSingleUDP(int &offset, int len);
	void _updateTCP();

	std::vector<UDPDataFromPeripheral> _udpData{};
	std::mutex _udpDataMutex;
	std::jthread _udpDataThread;

	std::mutex _tcpDataMutex;
	std::jthread _tcpDataThread;

	asio::io_context _ioContext;
	asio::ip::udp::endpoint _senderEndpointUDP;
	asio::ip::tcp::endpoint _senderEndpointTCP;
	asio::ip::udp::socket _socketUDP;
	asio::ip::tcp::socket _socketTCP;
	std::array<char, 4 * 1024> _bufferUDP{};

	std::mutex _tcpRecvBufferMutex;
	std::mutex _tcpSendBufferMutex;
	std::vector<uint8_t> _tcpRecvBuffer;
	std::vector<uint8_t> _tcpSendBuffer;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP