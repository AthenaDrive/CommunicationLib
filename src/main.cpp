
#include "Ethernet.hpp"

int main() {

	Ethernet eth;

	int32_t value0 = 1024;
	int32_t value1 = 0;

	while (true) {
		auto udpDataArr = eth.getUDPData();

		for (int i = 0; i < udpDataArr.size(); i++) {
			std::cout << i << ", " << udpDataArr[i].timestamp << ", " << udpDataArr[i].position << " | ";
		}
		std::cout << "\n";

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		std::atomic<std::array<int32_t, 4>> send{{value0++, value1++, 0, 0}};
		eth.bufferTcpSend.store(send);

		auto recv = eth.bufferTcpRecv.load();
		std::cout << recv[0] << ":" << recv[1] << "\n";
	}
}