
#ifndef COMPUTERCONTROLL_ETHERNET_HPP
#define COMPUTERCONTROLL_ETHERNET_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include <asio.hpp>

#include "Protocol.hpp"

class Ethernet {
public:
	Ethernet();
	~Ethernet();

	Ethernet(const Ethernet&) = delete;
	Ethernet& operator=(const Ethernet&) = delete;
	Ethernet(Ethernet&&) = delete;
	Ethernet& operator=(Ethernet&&) = delete;

	void setUDPCommand(const std::vector<CommandUDP>& data);
	std::vector<DataUDP> getUDPData();

	std::uint32_t packetsInQueueTCP() const;
	void addTCPPacket(const std::vector<std::uint8_t>& packet);
	std::vector<DataTCP> getTCPPacket();

	static std::vector<std::uint8_t> getTcpCommandPacket(
		const std::vector<std::pair<std::vector<std::uint32_t>, std::vector<std::uint32_t>>>& values);
	static std::vector<std::uint8_t> getTcpDataPacket(
		const std::vector<std::vector<std::uint32_t>>& values);

private:
	static constexpr std::size_t kCommandBufferSize = 4 * 1024;
	static constexpr std::size_t kMaxTcpPayloadSize = 0xFFFF;
	static constexpr std::chrono::milliseconds kUdpSendInterval{10};
	static constexpr std::chrono::milliseconds kUdpRetryDelay{10};
	static constexpr std::chrono::milliseconds kTcpReconnectDelay{200};

	std::atomic_bool _stopFlag{false};

	// UDP
	void _startUdpReceive();
	void _handleUdpDatagram(std::size_t len);
	void _startUdpSendTimer();
	void _sendUdp();
	bool _parseSingleUdp(std::size_t& offset, std::size_t len, DataUDP& out) const;

	std::vector<DataUDP> _dataUDP;
	std::mutex _udpDataMutex;

	std::mutex _bufferCommandUDPMutex;
	std::size_t _bufferCommandUDPLen = 0;
	std::array<std::uint8_t, kCommandBufferSize> _bufferCommandUDP{};
	std::array<std::uint8_t, kCommandBufferSize> _bufferDataUDP{};

	// TCP
	void _startTcpConnect();
	void _scheduleTcpReconnect();
	void _handleTcpDisconnect();
	void _startTcpReadHeader();
	void _startTcpReadBody();
	void _sendNextTcpPacket();
	bool _parseTcpPacket(const std::vector<std::uint8_t>& packet);

	std::mutex _tcpReceivingMutex;
	std::queue<DataTCP> _tcpReceivingBuffer;

	std::mutex _tcpSendingPacketMutex;
	std::queue<std::vector<std::uint8_t>> _tcpSendPacketBuffer;
	std::atomic<std::uint32_t> _packetsInList{0};
	bool _tcpConnected = false;
	bool _tcpWriting = false;

	std::array<std::uint8_t, 4> _tcpLengthPrefix{};
	std::vector<std::uint8_t> _tcpReadBuffer;

	// Networking / event loop
	asio::io_context _ioContext;
	asio::executor_work_guard<asio::io_context::executor_type> _workGuard;
	asio::ip::udp::endpoint _senderEndpointUDP;
	asio::ip::udp::endpoint _boardEndpointUDP;
	asio::ip::tcp::endpoint _remoteEndpointTCP;
	asio::ip::udp::socket _socketUDP;
	asio::ip::tcp::socket _socketTCP;
	asio::steady_timer _udpSendTimer;
	asio::steady_timer _udpReceiveRetryTimer;
	asio::steady_timer _tcpReconnectTimer;
	std::jthread _ioThread;
};

#endif //COMPUTERCONTROLL_ETHERNET_HPP
