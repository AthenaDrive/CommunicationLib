
#include "Ethernet.hpp"

#include <bitset>
#include <cmath>
#include <vector>

Ethernet::Ethernet()
    : _senderEndpointUDP(asio::ip::make_address_v4("192.168.0.17"), 5000),
	  _senderEndpointTCP(asio::ip::make_address_v4("192.168.0.18"), 5001),
	  _socketUDP(_ioContext, _senderEndpointUDP),
	  _socketTCP(_ioContext) {

	_udpDataThread = std::jthread(&Ethernet::_updateUDP, this);
	_tcpDataThread = std::jthread(&Ethernet::_updateTCP, this);
}

Ethernet::~Ethernet() {
	_stopFlag = true;
	if (_udpDataThread.joinable()) {
		_udpDataThread.join();
	}

	if (_tcpDataThread.joinable()) {
		_tcpDataThread.join();
	}
}

std::vector<UDPDataFromPeripheral> Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _udpData;
}

void Ethernet::_updateUDP() {

	while (!_stopFlag) {
		int offset = 0;
		size_t len = _socketUDP.receive_from(asio::buffer(_bufferUDP), _senderEndpointUDP);
		if (len < 4) {return;}

		std::vector<UDPDataFromPeripheral> newPackets;
		while (len > offset) {
			newPackets.emplace_back(readSingleUDP(offset, len));
		}

		std::lock_guard lock(_udpDataMutex);
		_udpData = newPackets;
	}
}

UDPDataFromPeripheral Ethernet::readSingleUDP(int &offset, int len) {
	if (len <= offset) {
		throw std::runtime_error("Invalid Packet read.\n");
	}

	uint32_t header;
	std::memcpy(&header, _bufferUDP.data() + offset, 4);
	offset += 4;

	int numValues = 0;
	std::vector<int> positions = {};

	std::bitset<32> headerBits(header);
	for (int i = 0; i < 32; i++) {
		if (headerBits[i]) {
			numValues++;
			positions.push_back(i);
		}
	}

	if ((len <= offset) && (numValues != 0)) {
		throw std::runtime_error("Invalid header.\n");
	}

	UDPDataFromPeripheral udpData{};
	for (int i = 0; i < numValues; i++) {
		int packetIx = positions[i];
		auto* base = reinterpret_cast<std::byte*>(&udpData);
		std::memcpy(base + offsetsUDPFromPeripheral[packetIx], _bufferUDP.data() + offset, 4);
		offset += 4;
	}

	return udpData;
}

void Ethernet::_updateTCP() {

	while (!_stopFlag) {
		if (!_socketTCP.is_open()) {
			try {
				_socketTCP.connect(_senderEndpointTCP);
				std::cout << "TCP connected" << std::endl;
			} catch (const std::system_error&) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		auto arrSend = bufferTcpSend.load();
		try {
			asio::write(_socketTCP, asio::buffer(arrSend));

			std::array<int32_t, 256> arrRecv{};
			size_t len = asio::read(_socketTCP, asio::buffer(arrRecv));
			bufferTcpRecv.store(arrRecv);
			numTCPReads.fetch_add(1);
		} catch (const std::system_error&) {
			_socketTCP.close();
		}
	}
}
