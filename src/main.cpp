
#include "Ethernet.hpp"

int main() {

	Ethernet eth;

	while (true) {
		eth.update();

		// std::cout << eth._udpData.index << " | " << eth._udpData.timestamp << " | " << eth._udpData.position << "\n";
	}

}