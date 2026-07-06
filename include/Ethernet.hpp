
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
#include "CommunicationConfig.hpp"

class Ethernet {
public:
	Ethernet();
	~Ethernet();

	std::vector<UDPDataFromPeripheral> getUDPData();

	std::atomic_int packetsInList = 0;
	void addTCPPacket(const std::vector<uint8_t> &packet);
	std::vector<DataTCP> getTCPPacket();

	static std::vector<uint8_t> getTcpCommandPacket(const std::vector<std::pair<std::vector<uint32_t>, std::vector<uint32_t>>>& values);
	static std::vector<uint8_t> getTcpDataPacket(const std::vector<std::vector<uint32_t>>& values);

private:
	std::atomic_bool _stopFlag = false;

	// UDP Stuff
	void _updateUDP();
	UDPDataFromPeripheral readSingleUDP(int &offset, int len);

	std::vector<UDPDataFromPeripheral> _udpData{};
	std::array<char, 4 * 1024> _bufferUDP{};
	std::mutex _udpDataMutex;
	std::jthread _udpDataThread;

	// TCP Stuff
	void _updateTCP();
	void _parseTcpPacket(const std::vector<uint8_t> &packet);

	std::queue<std::vector<uint8_t>> _tcpSendPacketBuffer;
	std::mutex _tcpSendingPacketMutex;

	std::queue<DataTCP> _tcpReceivingBuffer;
	std::mutex _tcpReceivingMutex;

	// TODO!
	std::vector<CommandTCP> _internalCommandBuffer;
	std::mutex _internalCommandMutex;

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