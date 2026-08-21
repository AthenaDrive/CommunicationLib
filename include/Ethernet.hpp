
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

#include "Protocol.hpp"

class Ethernet {
public:
	Ethernet();
	~Ethernet();

	void setUDPCommand(const std::vector<CommandUDP> &data);
	std::vector<DataUDP> getUDPData();

	uint32_t packetsInQueueTCP() const;
	void addTCPPacket(const std::vector<uint8_t> &packet);
	std::vector<DataTCP> getTCPPacket();

	static std::vector<uint8_t> getTcpCommandPacket(const std::vector<std::pair<std::vector<uint32_t>, std::vector<uint32_t>>>& values);
	static std::vector<uint8_t> getTcpDataPacket(const std::vector<std::vector<uint32_t>>& values);

private:
	std::atomic_bool _stopFlag = false;

	// UDP Stuff
	void _updateUDP();
	void recvUDP();
	void sendUDP();

	DataUDP readSingleUDP(int &offset, int len);

	std::vector<DataUDP> _dataUDP;
	std::mutex _udpDataMutex;

	std::mutex _bufferCommandUDPMutex;
	uint32_t _bufferCommandUDPLen = 0;
	std::array<uint8_t, 4 * 1024> _bufferCommandUDP{};
	std::chrono::high_resolution_clock::time_point _prevCommandUDPTime;

	std::array<uint8_t, 4 * 1024> _bufferDataUDP{};

	std::jthread _udpDataThread;

	// TCP Stuff
	void _updateTCP();
	bool _connectionOpen();
	void _parseTcpPacket(const std::vector<uint8_t> &packet);

	std::mutex _dataTCPMutex;

	std::atomic_int _packetsInList = 0;
	std::queue<std::vector<uint8_t>> _tcpSendPacketBuffer;
	std::mutex _tcpSendingPacketMutex;

	std::queue<DataTCP> _tcpReceivingBuffer;
	std::mutex _tcpReceivingMutex;

	std::jthread _tcpDataThread;

	// Ethernet stuff
	asio::io_context _ioContext;
	asio::ip::udp::endpoint _senderEndpointUDP;  // will be filled by receive_from
	asio::ip::udp::endpoint _boardEndpointUDP{asio::ip::make_address_v4("192.168.0.2"), 5000};
	asio::ip::tcp::endpoint _remoteEndpointTCP;
	asio::ip::udp::socket _socketUDP;
	asio::ip::tcp::socket _socketTCP;
};


#endif //COMPUTERCONTROLL_ETHERNET_HPP