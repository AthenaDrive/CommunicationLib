
#include <bitset>

#include "Ethernet.hpp"
#include "Protocol.hpp"
#include "RuntimeError.hpp"

std::pair<std::vector<CommandTCP>, std::vector<DataTCP>> parseTcpPacket(const std::vector<uint8_t> &packet) {
	if (packet.size() < 4) {
		throw RuntimeError("Invalid TCP packet. Packet size: " + std::to_string(packet.size()) + "\n");
	}

	std::pair<std::vector<CommandTCP>, std::vector<DataTCP>> out;
	uint32_t packetOffset = 0;
	uint32_t header;
	uint32_t tmpUint;
	uint32_t tmpFloat;

	while (packetOffset < packet.size()) {
		memcpy(&header, packet.data() + packetOffset, 4);
		packetOffset += 4;

		std::bitset<32> headerBits(header);
		if (headerBits[0]) {
			// Command
			CommandTCP commandTCP;
			commandTCP.motorIx = out.first.size() + out.second.size();
			for (int i = 1; i < 32; i++) {
				if (headerBits[i]) {

					switch (1 << i) {
						case (TCP_COMMAND_TORQUE_SETPOINT): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueSetpoint = tmpFloat;
						} break;

						case (TCP_COMMAND_TORQUE_KP): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueKp = tmpFloat;
						} break;

						case (TCP_COMMAND_TORQUE_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueKd = tmpFloat;
						} break;

						case (TCP_COMMAND_TORQUE_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueKi = tmpFloat;
						} break;

						case (TCP_COMMAND_TORQUE_LIMIT): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueLimit = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_SETPOINT): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocitySetpoint = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_KP): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityKp = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityKd = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityKi = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_LIMIT): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityLimit = tmpFloat;
						} break;

						case (TCP_COMMAND_POSITION_SETPOINT): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionSetpoint = tmpFloat;
						} break;

						case (TCP_COMMAND_POSITION_KP): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionKp = tmpFloat;
						} break;

						case (TCP_COMMAND_POSITION_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionKd = tmpFloat;
						} break;

						case (TCP_COMMAND_POSITION_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionKi = tmpFloat;
						} break;

						case (TCP_COMMAND_DRIVING_MODE): {
						    memcpy(&tmpUint, packet.data() + packetOffset, 4);
						    commandTCP.drivingMode = tmpUint;
						} break;

						case (TCP_COMMAND_CURRENT_LIMIT_BUS): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.currentLimitBus = tmpFloat;
						} break;

						case (TCP_COMMAND_CURRENT_LIMIT_PHASE): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.currentLimitPhase = tmpFloat;
						} break;

						case (TCP_COMMAND_POLE_PAIRS): {
						    memcpy(&tmpUint, packet.data() + packetOffset, 4);
						    commandTCP.polePairs = tmpUint;
						} break;

						case (TCP_COMMAND_UDP_DATA_CONFIG): {
						    memcpy(&tmpUint, packet.data() + packetOffset, 4);
						    commandTCP.udpDataConfig = tmpUint;
						} break;

						case (TCP_COMMAND_CLEAR_ERROR_REGISTER): {
						    memcpy(&tmpUint, packet.data() + packetOffset, 4);
						    commandTCP.confirmErrorCode = tmpUint;
						} break;

						default: {
							// Error?
						} break;
					}

					packetOffset += 4;
				}
			}

			out.first.emplace_back(commandTCP);
		} else {
			// Data
			DataTCP dataTCP;
			dataTCP.motorIx = out.first.size() + out.second.size();
			for (int i = 1; i < 32; i++) {
				if (headerBits[i]) {

					switch (1 << i) {
						case (TCP_DATA_BUS_VOLTAGE): {
							memcpy(&tmpFloat, packet.data() + packetOffset, 4);
							dataTCP.busVoltage = tmpFloat;
						} break;

						case (TCP_DATA_BUS_CURRENT): {
							memcpy(&tmpFloat, packet.data() + packetOffset, 4);
							dataTCP.busCurrent = tmpFloat;
						} break;

						case (TCP_DATA_LED_STATUS): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.ledStatus = tmpUint;
						}

						default: {
							// Error?
						} break;
					}

					packetOffset += 4;
				}
			}

			out.second.emplace_back(dataTCP);
		}
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

		eth.setTCPSend(outVec);

		if (!eth.shouldSendTCP.load()) {
			eth.shouldSendTCP.store(true);

			std::vector<uint8_t> recvBuffer{};
			eth.getTCPRecv(recvBuffer);

			if (recvBuffer.size() >= 4) {
				auto outTCP = parseTcpPacket(recvBuffer);
				std::cout << "Num packets: " << outTCP.first.size() + outTCP.second.size() << " | ";

				for (auto &commandPacket : outTCP.first) {
					// Command
					// TODO: clear from ethernet buffer.
				}

				for (auto &dataPacket : outTCP.second) {
					// Data

					if (dataPacket.ledStatus.has_value()) {
						std::cout << "ix: " << dataPacket.motorIx << " | LED: " << dataPacket.ledStatus.value() << " | ";
					}
				}

				std::cout << "\n";

			} else {
				std::cout << "No TCP packet.\n";
			}
		}
	}
}