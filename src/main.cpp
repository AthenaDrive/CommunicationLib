
#include <bitset>

#include "Ethernet.hpp"
#include "Protocol.hpp"
#include "RuntimeError.hpp"

dataTCP parseTcpPacket(const std::vector<uint8_t> &packet) {
	if (packet.size() < 4) {
		throw RuntimeError("Invalid TCP packet. Packet size: " + std::to_string(packet.size()) + "\n");
	}

	dataTCP out;

	uint32_t header{};
	memcpy(&header, packet.data(), 4);
	uint32_t packetOffset = 4;

	std::cout << "Header:" << header << "\n";
	std::bitset<32> headerBits(header);
	if (headerBits[0]) {
		// Was command, ESP is just confirming command executed.
		// TODO: read back command to clear from executed command history?

		out.readWriteFlag = true;
		return out;
	}
	out.readWriteFlag = false;

	float tmpFloat;
	uint32_t tmpUint;
	for (int i = 1; i < 32; i++) {
		if (headerBits[i]) {
			std::cout << "ix: " << i << "\n";

			switch (1 << i) {
				case (TCP_DATA_BUS_VOLTAGE): {
					memcpy(&tmpFloat, packet.data() + packetOffset, 4);
					out.busVoltage = tmpFloat;
				} break;

				case (TCP_DATA_BUS_CURRENT): {
					memcpy(&tmpFloat, packet.data() + packetOffset, 4);
					out.busCurrent = tmpFloat;
				} break;

				case (TCP_DATA_LED_STATUS): {
					memcpy(&tmpUint, packet.data() + packetOffset, 4);
					out.ledStatus = tmpUint;
				}

				default: {
					// Error?
				} break;
			}

			packetOffset += 4;
		}
	}

	std::cout << "Data size: " << packet.size() << " | Offset: " << packetOffset << "\n";
	if (packetOffset < packet.size()) {
		uint32_t h;
		memcpy(&h, packet.data() + packetOffset, 4);
		packetOffset += 4;
		memcpy(&tmpUint, packet.data() + packetOffset, 4);
		std::cout << "Next header: " << h << " | Led: " << tmpUint << "\n";
	}

	return out;
}

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
			std::cout << i << ", " << udpDataArr[i].timestamp << ", " << udpDataArr[i].position << " | ";
		}
		std::cout << "\n";

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::vector<uint8_t> outVec{};

		outVec = getTcpDataPacket({
			{TCP_DATA, TCP_DATA_LED_STATUS},
			{TCP_DATA, TCP_DATA_LED_STATUS}
		});

		outVec = getTcpCommandPacket({
			{{TCP_COMMAND, TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}},
				{{TCP_COMMAND, TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}}
		});

		eth.setTCPSend(outVec);

		if (!eth.shouldSendTCP.load()) {
			eth.shouldSendTCP.store(true);

			std::vector<uint8_t> recvBuffer{};
			eth.getTCPRecv(recvBuffer);

			if (recvBuffer.size() >= 4) {
				dataTCP outTCP = parseTcpPacket(recvBuffer);

				if (outTCP.readWriteFlag.has_value()) {
					std::cout << "R/W flag: " << outTCP.readWriteFlag.value() << "\n";
				}

				if (outTCP.busVoltage.has_value()) {
					std::cout << "Bus voltage: " << outTCP.busVoltage.value() << "\n";
				}

				if (outTCP.busCurrent.has_value()) {
					std::cout << "Bus current: " << outTCP.busCurrent.value() << "\n";
				}

				if (outTCP.ledStatus.has_value()) {
					std::cout << "LED Status: " << outTCP.ledStatus.value() << "\n";
				}
			}
		}
	}
}