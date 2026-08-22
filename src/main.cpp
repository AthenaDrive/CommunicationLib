
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

	if (data.debugFloat0.has_value()) {
		std::cout << ", f0: " << data.debugFloat0.value();
	}

	if (data.debugFloat1.has_value()) {
		std::cout << ", f1: " << data.debugFloat1.value();
	}

	if (data.debugFloat2.has_value()) {
		std::cout << ", f2: " << data.debugFloat2.value();
	}

	if (data.debugFloat3.has_value()) {
		std::cout << ", f3: " << data.debugFloat3.value();
	}

	std::cout << "\n";
}

int main() {

	Ethernet eth;

	CommandUDP commandUDP;
	commandUDP.motorIx = 0;
	commandUDP.debugFloat0 = 1.0;
	commandUDP.debugFloat1 = 1.0;
	commandUDP.debugFloat2 = 1.0;
	eth.setUDPCommand({commandUDP});

	while (true) {
		commandUDP.debugFloat0.value() += 0.0001f;
		eth.setUDPCommand({commandUDP});
		std::cout << commandUDP.debugFloat0.value() << "\n";

		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			// printUDPData(udpDataArr[i]);
		}

		std::vector<uint8_t> outVec{};

		outVec = Ethernet::getTcpDataPacket({
			{TCP_DATA_LED_STATUS},
			{TCP_DATA_LED_STATUS}
		});


		outVec = Ethernet::getTcpCommandPacket({
		   {{TCP_COMMAND_UDP_DATA_CONFIG}, {0b111000000000000000000000000}}
		});

		if (eth.packetsInQueueTCP() == 0) {
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