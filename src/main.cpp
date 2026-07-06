
#include "Ethernet.hpp"
#include "Protocol.hpp"
#include "RuntimeError.hpp"

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

		outVec = Ethernet::getTcpDataPacket({
			{TCP_DATA_LED_STATUS},
			{TCP_DATA_LED_STATUS}
		});

		// outVec = Ethernet::getTcpCommandPacket({
		// 	{{TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}},
		// 		{{TCP_COMMAND_UDP_DATA_CONFIG}, {0b1111}}
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