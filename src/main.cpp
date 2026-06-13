
#include "Ethernet.hpp"

int main() {

	Ethernet eth;

	while (true) {
		auto udpData = eth.getUDPData();
		std::cout << udpData.index << " | " << udpData.timestamp << " | " << udpData.position << "\n";
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}