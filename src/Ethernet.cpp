
#include "Ethernet.hpp"

#include <bitset>
#include <cmath>
#include <vector>

Ethernet::Ethernet()
    : _socketUDP(_ioContext, asio::ip::udp::endpoint(asio::ip::make_address_v4("192.168.0.17"), 5000)),
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

UDPData Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _udpData;
}

void Ethernet::_updateUDP() {
	while (!_stopFlag) {
		size_t len = _socketUDP.receive_from(asio::buffer(_bufferUDP), _senderEndpointUDP);

		if (len < 4) {
			return;
		}

		uint32_t header;
		memcpy(&header, _bufferUDP.data(), 4);

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
			std::memcpy(base + offsets[packetIx], _bufferUDP.data() + 4 * (i+1), 4);
		}
	}
}

void Ethernet::_updateTCP() {
	asio::ip::tcp::endpoint ep(asio::ip::make_address_v4("192.168.0.10"), 5001);

	while (!_stopFlag) {
		if (!_socketTCP.is_open()) {
			try {
				_socketTCP.connect(ep);
				std::cout << "TCP connected" << std::endl;
			} catch (const std::system_error&) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		auto arrSend = bufferTcpSend.load();
		try {
			asio::write(_socketTCP, asio::buffer(arrSend));

			std::array<int32_t, 4> arrRecv;
			asio::read(_socketTCP, asio::buffer(arrRecv));
			bufferTcpRecv.store(arrRecv);
		} catch (const std::system_error&) {
			_socketTCP.close();
		}
	}
}
