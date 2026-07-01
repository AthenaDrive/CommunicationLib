
#include "Ethernet.hpp"

int main() {

	Ethernet eth;

	uint32_t lengthPrefixReadVoltCurr = 0b11111000011110010000000000000100;

	// Firs bit 0 => Read data (Not command)
	// Second bit 1 => Bus voltage
	// Third bit 1 => Bus current
	uint32_t headerReadVoltCurr = 0b00000000000000000000000000000110;

	uint32_t lengthPrefixSetUDPHeader = 0b11111000011110010000000000001000;
	uint32_t headerSetUDPHeader = (1 << 0) | (1 << 19);
	uint32_t packetSetUDPHeader = 0b0000000000000000000000000001101;

	uint32_t lengthPrefixDual = 0b11111000011110010000000000010100;

	while (true) {
		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			std::cout << i << ", " << udpDataArr[i].timestamp << ", " << udpDataArr[i].position << " | ";
		}
		std::cout << "\n";

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		uint32_t lengthPrefix = lengthPrefixDual;
		uint32_t header = headerSetUDPHeader;
		uint32_t packet = packetSetUDPHeader;

		std::vector<uint8_t> outVec{};
		outVec.push_back((lengthPrefix >> 0) & 0xFF);
		outVec.push_back((lengthPrefix >> 8) & 0xFF);
		outVec.push_back((lengthPrefix >> 16) & 0xFF);
		outVec.push_back((lengthPrefix >> 24) & 0xFF);

		outVec.push_back((header >> 0) & 0xFF);
		outVec.push_back((header >> 8) & 0xFF);
		outVec.push_back((header >> 16) & 0xFF);
		outVec.push_back((header >> 24) & 0xFF);

		outVec.push_back((packet >> 0) & 0xFF);
		outVec.push_back((packet >> 8) & 0xFF);
		outVec.push_back((packet >> 16) & 0xFF);
		outVec.push_back((packet >> 24) & 0xFF);

		lengthPrefix = lengthPrefixSetUDPHeader;
		outVec.push_back((lengthPrefix >> 0) & 0xFF);
		outVec.push_back((lengthPrefix >> 8) & 0xFF);
		outVec.push_back((lengthPrefix >> 16) & 0xFF);
		outVec.push_back((lengthPrefix >> 24) & 0xFF);

		outVec.push_back((header >> 0) & 0xFF);
		outVec.push_back((header >> 8) & 0xFF);
		outVec.push_back((header >> 16) & 0xFF);
		outVec.push_back((header >> 24) & 0xFF);

		outVec.push_back((packet >> 0) & 0xFF);
		outVec.push_back((packet >> 8) & 0xFF);
		outVec.push_back((packet >> 16) & 0xFF);
		outVec.push_back((packet >> 24) & 0xFF);

		eth.setTCPSend(outVec);

		if (!eth.shouldSendTCP) {
			eth.shouldSendTCP.store(true);

			std::vector<uint8_t> recvBuffer{};
			eth.getTCPRecv(recvBuffer);
			// std::cout << "Count: " << eth.numTCPReads.load() << " | ";
			for (uint8_t val : recvBuffer) {
				// std::cout << static_cast<int>(val) << " ";
			}
		}
	}
}