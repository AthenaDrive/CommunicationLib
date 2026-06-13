
#include "Ethernet.hpp"

#include <bitset>
#include <cmath>
#include <iomanip>

Ethernet::Ethernet()
    : _socket(_io_context, asio::ip::udp::endpoint(asio::ip::make_address("192.168.0.17"), 5000)) {
	std::cout << "Listening for UDP packets on 192.168.0.17:5000..." << std::endl;
}

void Ethernet::update() {
	size_t len = _socket.receive_from(asio::buffer(_buffer), _sender_endpoint);

	auto now = std::chrono::steady_clock::now();
	if (_packetCount > 0) {
		double dt = std::chrono::duration<double>(now - _lastPacketTime).count();
		_sumDt += dt;
		_sumDtSq += dt * dt;
	}
	_lastPacketTime = now;
	_packetCount++;

	if (_packetCount >= 100) {
		int n = _packetCount - 1;
		double mean = _sumDt / n;
		double variance = (_sumDtSq / n) - (mean * mean);
		if (variance < 0) variance = 0;
		double stddev = std::sqrt(variance);
		double pps = 1.0 / mean;
		std::cout << "PPS: " << (int)pps << ", stddev: " << std::fixed << std::setprecision(6) << stddev << "s" << std::endl;
		_packetCount = 0;
		_sumDt = 0;
		_sumDtSq = 0;
	}

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

	for (int i = 0; i < numValues; i++) {
		int packetIx = positions[i];
		auto* base = reinterpret_cast<std::byte*>(&_udpData);
		std::memcpy(base + offsets[packetIx], _buffer.data() + 4 * (i+1), 4);
	}
}

