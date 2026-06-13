
#include "Ethernet.hpp"

#include <bitset>
#include <cmath>
#include <iomanip>

Ethernet::Ethernet()
    : _socket(_io_context, asio::ip::udp::endpoint(asio::ip::make_address("192.168.0.17"), 5000)) {

	_udpDataThread = std::jthread(&Ethernet::_readUDP, this);
}

Ethernet::~Ethernet() {
	_stopFlag = true;
	if (_udpDataThread.joinable()) {
		_udpDataThread.join();
		std::cout << "Joined thread." << std::endl;
	}
}

UDPData Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _udpData;
}

void Ethernet::_readUDP() {
	while (!_stopFlag) {
		size_t len = _socket.receive_from(asio::buffer(_buffer), _sender_endpoint);

		if (len < 4) {
			return;
		}

		uint32_t header;
		memcpy(&header, _buffer.data(), 4);

		int numValues = 0;
		std::vector<int> positions;

		std::bitset<32> headerBits(header);
		for (int i = 0; i < 32; i++) {
			if (headerBits[i]) {
				numValues++;
				positions.push_back(i);
			}
		}

		if (len != (4 * (numValues + 1))) {return;}

		std::lock_guard lock(_udpDataMutex);
		for (int i = 0; i < numValues; i++) {
			int packetIx = positions[i];
			auto* base = reinterpret_cast<std::byte*>(&_udpData);
			std::memcpy(base + offsets[packetIx], _buffer.data() + 4 * (i+1), 4);
		}
	}
}

