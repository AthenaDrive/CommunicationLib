
#include <bitset>

#include "Ethernet.hpp"
#include "Protocol.hpp"
#include "RuntimeError.hpp"

std::vector<uint8_t> getTcpCommandPacket(const std::vector<std::pair<std::vector<uint32_t>, std::vector<uint32_t>>>& values) {
	std::vector<uint32_t> headers;
	std::vector<std::vector<uint32_t>> packetVals;
	uint32_t numValues = 0;

	for (int i = 0; i < values.size(); i++) {
		if ((values[i].first[0] != TCP_DATA) &&
			(values[i].first[0] != TCP_COMMAND)) {
			throw RuntimeError("The first header bit MUST be either DATA or COMMAND.\n");
		}

		for (int v = 1; v < values[i].first.size(); v++) {
			if ((values[i].first[v] == TCP_DATA) ||
				(values[i].first[v] == TCP_COMMAND)) {
				throw RuntimeError("Only the first header bit can be DATA or COMMAND.\n");
			}
		}

		if (values[i].first.size() - 1 != values[i].second.size()) {
			// Horrible error message.. idk.
			// This function expects exactly 1 (the first)
			// header value to be either the DATA or COMMAND flag.

			// The rest of the header values should match up with each of the values.
			throw RuntimeError("Num values must equal active header values.\n");
		}

		headers.emplace_back(0);
		packetVals.emplace_back();

		for (auto &v : values[i].first) {
			headers[i] |= v;
		}

		for (auto &v : values[i].second) {
			packetVals[i].emplace_back(v);
			numValues++;
		}
	}

	std::vector<uint8_t> outVec{};
	for (int i = 0; i < headers.size(); i++) {
		// std::cout << "Header size: " << headers.size() << " | i: " << i << " | NumVal: " << numValues << "\n";
		uint32_t prefix = PREFIX(((2 * (headers.size() - i) - 1) + numValues) * 4);
		outVec.push_back((prefix >> 0) & 0xFF);
		outVec.push_back((prefix >> 8) & 0xFF);
		outVec.push_back((prefix >> 16) & 0xFF);
		outVec.push_back((prefix >> 24) & 0xFF);

		outVec.push_back((headers[i] >> 0) & 0xFF);
		outVec.push_back((headers[i] >> 8) & 0xFF);
		outVec.push_back((headers[i] >> 16) & 0xFF);
		outVec.push_back((headers[i] >> 24) & 0xFF);

		// std::cout << "Prefix: " << prefix << " | Header: " << headers[i] << "\n";
		for (auto &v : packetVals[i]) {
			outVec.push_back((v >> 0) & 0xFF);
			outVec.push_back((v >> 8) & 0xFF);
			outVec.push_back((v >> 16) & 0xFF);
			outVec.push_back((v >> 24) & 0xFF);
			numValues--;

			// std::cout << "V: " << v << "\n";
		}
	}

	return outVec;
}

std::vector<uint8_t> getTcpDataPacket(const std::vector<std::vector<uint32_t>>& values) {
	std::vector<uint32_t> headers;

	for (int i = 0; i < values.size(); i++) {
		if ((values[i][0] != TCP_DATA) && (values[i][0] != TCP_COMMAND)) {
			throw RuntimeError("The first header bit MUST be either DATA or COMMAND.\n");
		}

		for (int v = 1; v < values[i].size(); v++) {
			if ((values[i][v] == TCP_DATA) ||
				(values[i][v] == TCP_COMMAND)) {
				throw RuntimeError("Only the first header bit can be DATA or COMMAND.\n");
			}
		}

		headers.emplace_back(0);
		for (auto &v : values[i]) {
			headers[i] |= v;
		}
	}

	std::vector<uint8_t> outVec{};
	for (int i = 0; i < headers.size(); i++) {
		uint32_t prefix = PREFIX((2 * (headers.size() - i) - 1) * 4);
		outVec.push_back((prefix >> 0) & 0xFF);
		outVec.push_back((prefix >> 8) & 0xFF);
		outVec.push_back((prefix >> 16) & 0xFF);
		outVec.push_back((prefix >> 24) & 0xFF);

		outVec.push_back((headers[i] >> 0) & 0xFF);
		outVec.push_back((headers[i] >> 8) & 0xFF);
		outVec.push_back((headers[i] >> 16) & 0xFF);
		outVec.push_back((headers[i] >> 24) & 0xFF);

		// std::cout << "Prefix: " << prefix << " | Header: " << headers[i] << "\n";
	}

	return outVec;
}

int main() {

	Ethernet eth;

	while (true) {
		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			// std::cout << i << ", " << udpDataArr[i].timestamp << ", " << udpDataArr[i].position << " | ";
		}
		// std::cout << "\n";

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::vector<uint8_t> outVec{};

		outVec = getTcpDataPacket({
			{TCP_DATA, TCP_DATA_LED_STATUS},
			{TCP_DATA, TCP_DATA_LED_STATUS}
		});

		// outVec = getTcpCommandPacket({
		// 	{{TCP_COMMAND, TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}},
		// 		{{TCP_COMMAND, TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}}
		// });

		if (eth.packetsInList.load() == 0) {
			std::cout << "Adding packet.\n";
			eth.addTCPPacket(outVec);
		}

		auto tcpData = eth.getTCPPacket();
		std::cout << "Num TCP data packets: " << tcpData.size() << "\n";

		for (auto &dataPacket : tcpData) {
			// Data

			if (dataPacket.ledStatus.has_value()) {
				std::cout << "ix: " << dataPacket.motorIx << " | LED: " << dataPacket.ledStatus.value() << " | ";
			}
		}

		std::cout << "\n";
	}
}