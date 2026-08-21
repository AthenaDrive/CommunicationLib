
#include "Ethernet.hpp"
#include "Protocol.hpp"
#include "RuntimeError.hpp"

void printUDPData(DataUDP &data) {
	// if (data.motorIx == -1) { return; }

	std::cout << "ix: " << data.motorIx;

	if (data.timestamp.has_value()) {
		std::cout << ", " << data.timestamp.value();
	}

	if (data.position.has_value()) {
		std::cout << ", " << data.position.value();
	}

	if (data.velocity.has_value()) {
		std::cout << ", " << data.velocity.value();
	}

	std::cout << "\n";
}

int main() {

	Ethernet eth;

	while (true) {
		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			printUDPData(udpDataArr[i]);
		}

		std::vector<uint8_t> outVec{};

		outVec = Ethernet::getTcpDataPacket({
			{TCP_DATA_LED_STATUS}
		});


		outVec = Ethernet::getTcpCommandPacket({
		   {{TCP_COMMAND_UDP_DATA_CONFIG}, {0b111}}
		});

		if (eth.packetsInList.load() == 0) {
			eth.addTCPPacket(outVec);
		}

		auto tcpData = eth.getTCPPacket();

		for (auto &dataPacket : tcpData) {
			// Data

			if (dataPacket.ledStatus.has_value()) {
				std::cout << "ix: " << dataPacket.motorIx << " | LED: " << dataPacket.ledStatus.value() << "\n";
			}
		}

		// std::cout << "\n";
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}