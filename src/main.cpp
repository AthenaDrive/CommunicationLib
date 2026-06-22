
#include "Ethernet.hpp"

int main() {

	Ethernet eth;

	while (true) {
		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			std::cout << i << ", " << udpDataArr[i].timestamp << ", " << udpDataArr[i].position << " | ";
		}
		std::cout << "\n";

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::atomic<std::array<int32_t, 1>> send{{0b01111}};
		eth.bufferTcpSend.store(send);

		auto recv = eth.bufferTcpRecv.load();
		std::cout << "Count: " << eth.numTCPReads.load() << ": ";
		for (int i = 0; i < 32; i++) {
			std::cout << recv[i];
		}
		std::cout << "\n";
	}
}