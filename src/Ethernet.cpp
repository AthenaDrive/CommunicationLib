
#include "Ethernet.hpp"

#include <bitset>
#include <cmath>
#include <vector>

#include "RuntimeError.hpp"

Ethernet::Ethernet()
	: _remoteEndpointTCP(asio::ip::make_address_v4("192.168.0.2"), 5001),
	  _socketUDP(_ioContext, asio::ip::udp::endpoint(asio::ip::udp::v4(), 5000)),
	  _socketTCP(_ioContext) {

	_senderEndpointUDP = asio::ip::udp::endpoint(asio::ip::make_address_v4("192.168.0.2"), 5000);

	_udpDataThread = std::jthread(&Ethernet::_updateUDP, this);
	_tcpDataThread = std::jthread(&Ethernet::_updateTCP, this);
}

Ethernet::~Ethernet() {
	_stopFlag = true;
	if (_udpDataThread.joinable()) {
		_udpDataThread.join();
	}

	if (_tcpDataThread.joinable()) {
		_tcpDataThread.join();
	}
}

void Ethernet::setUDPData(const std::vector<CommandUDP>& data) {
	std::vector<CommandUDP> actualData;

	// Sorting data. Discarding duplicates.
	for (auto &d : data) {
		if (d.motorIx == -1) { continue; }

		while (actualData.size() < d.motorIx) {
			actualData.push_back({});
		}

		actualData[d.motorIx] = d;
	}

	std::lock_guard lock(_bufferCommandUDPMutex);

	uint32_t outerOffset = 0;
	for (auto &d : data) {
		uint32_t offset = 4 + outerOffset; // Save space for header

		uint32_t header = 0;
		if (d.torqueSetpoint.has_value()) {
			header |= UDP_COMMAND_TORQUE_SETPOINT;
			memcpy(_bufferCommandUDP.data() + offset, &d.torqueSetpoint.value(), 4);
			offset += 4;
		}

		if (d.velocitySetpoint.has_value()) {
			header |= UDP_COMMAND_VELOCITY_SETPOINT;
			memcpy(_bufferCommandUDP.data() + offset, &d.velocitySetpoint.value(), 4);
			offset += 4;
		}

		if (d.positionSetpoint.has_value()) {
			header |= UDP_COMMAND_POSITION_SETPOINT;
			memcpy(_bufferCommandUDP.data() + offset, &d.positionSetpoint.value(), 4);
			offset += 4;
		}

		if (d.drivingMode.has_value()) {
			header |= UDP_COMMAND_DRIVING_MODE;
			memcpy(_bufferCommandUDP.data() + offset, &d.drivingMode.value(), 4);
			offset += 4;
		}

		if (d.maxTorque.has_value()) {
			header |= UDP_COMMAND_MAX_TORQUE;
			memcpy(_bufferCommandUDP.data() + offset, &d.maxTorque.value(), 4);
			offset += 4;
		}

		if (d.maxVelocity.has_value()) {
			header |= UDP_COMMAND_MAX_VELOCITY;
			memcpy(_bufferCommandUDP.data() + offset, &d.maxVelocity.value(), 4);
			offset += 4;
		}

		if (d.currentLimitBus.has_value()) {
			header |= UDP_COMMAND_CURRENT_LIMIT_BUS;
			memcpy(_bufferCommandUDP.data() + offset, &d.currentLimitBus.value(), 4);
			offset += 4;
		}

		if (d.currentLimitPhase.has_value()) {
			header |= UDP_COMMAND_CURRENT_LIMIT_PHASE;
			memcpy(_bufferCommandUDP.data() + offset, &d.currentLimitPhase.value(), 4);
			offset += 4;
		}

		memcpy(_bufferCommandUDP.data() + outerOffset, &header, 4);
		outerOffset += offset;
	}

	_bufferCommandUDPLen = outerOffset;
}

std::vector<DataUDP> Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _dataUDP;
}

void Ethernet::_updateUDP() {

	while (!_stopFlag) {
		recvUDP();
		sendUDP();
	}
}

void Ethernet::recvUDP() {
	int offset = 0;
	size_t len = _socketUDP.receive_from(asio::buffer(_bufferDataUDP), _senderEndpointUDP);
	if (len < 4) { return; }

	int ix = 1;
	std::vector<DataUDP> newPackets;
	while (len > offset) {
		newPackets.emplace_back(readSingleUDP(offset, len));
		newPackets.back().motorIx = ix;
		ix++;
	}

	std::lock_guard lock(_udpDataMutex);
	_dataUDP = newPackets;
}

void Ethernet::sendUDP() {
	auto tNow = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(tNow - _prevCommandUDPTime).count();

	std::lock_guard lock(_bufferCommandUDPMutex);

	if ((time > 10000) and (_bufferCommandUDPLen > 0)) {
		_socketUDP.send_to(asio::buffer(_bufferCommandUDP, _bufferCommandUDPLen), _boardEndpointUDP);
		_prevCommandUDPTime = tNow;
	}
}

DataUDP Ethernet::readSingleUDP(int &offset, int len) {
	if (len <= offset) {
		throw std::runtime_error("Invalid Packet read.\n");
	}

	uint32_t header;
	std::memcpy(&header, _bufferDataUDP.data() + offset, 4);
	offset += 4;

	int numValues = 0;
	std::vector<int> positions = {};

	std::bitset<32> headerBits(header);
	for (int i = 0; i < 32; i++) {
		if (headerBits[i]) {
			numValues++;
			positions.push_back(i);
		}
	}

	if ((len <= offset) && (numValues != 0)) {
		throw std::runtime_error("Invalid header.\n");
	}

	DataUDP udpData{};
	for (int i = 0; i < numValues; i++) {
		int packetIx = positions[i];

		if (packetIx == 0) {
			uint32_t timestamp;
			std::memcpy(&timestamp, _bufferDataUDP.data() + offset, 4);
			udpData.timestamp = timestamp;
		} else if (packetIx == 1) {
			float position;
			std::memcpy(&position, _bufferDataUDP.data() + offset, 4);
			udpData.position = position;
		} else if (packetIx == 2) {
			float velocity;
			std::memcpy(&velocity, _bufferDataUDP.data() + offset, 4);
			udpData.velocity = velocity;
		} else if (packetIx == 3) {
			float acceleration;
			std::memcpy(&acceleration, _bufferDataUDP.data() + offset, 4);
			udpData.acceleration = acceleration;
		} else if (packetIx == 4) {
			float torque;
			std::memcpy(&torque, _bufferDataUDP.data() + offset, 4);
			udpData.torque = torque;
		} else if (packetIx == 5) {
			float phaseCurrentA;
			std::memcpy(&phaseCurrentA, _bufferDataUDP.data() + offset, 4);
			udpData.phaseCurrentA = phaseCurrentA;
		} else if (packetIx == 6) {
			float phaseCurrentB;
			std::memcpy(&phaseCurrentB, _bufferDataUDP.data() + offset, 4);
			udpData.phaseCurrentB = phaseCurrentB;
		} else if (packetIx == 7) {
			float phaseCurrentC;
			std::memcpy(&phaseCurrentC, _bufferDataUDP.data() + offset, 4);
			udpData.phaseCurrentC = phaseCurrentC;
		} else if (packetIx == 8) {
			float busCurrent;
			std::memcpy(&busCurrent, _bufferDataUDP.data() + offset, 4);
			udpData.busCurrent = busCurrent;
		} else if (packetIx == 9) {
			float busVoltage;
			std::memcpy(&busVoltage, _bufferDataUDP.data() + offset, 4);
			udpData.busVoltage = busVoltage;
		} else if (packetIx == 10) {
			uint32_t errorRegister;
			std::memcpy(&errorRegister, _bufferDataUDP.data() + offset, 4);
			udpData.errorRegister = errorRegister;
		} else if (packetIx == 11) {
			uint32_t loopTimeFOC;
			std::memcpy(&loopTimeFOC, _bufferDataUDP.data() + offset, 4);
			udpData.loopTimeFOC = loopTimeFOC;
		} else if (packetIx == 12) {
			uint32_t loopTimeSecondary;
			std::memcpy(&loopTimeSecondary, _bufferDataUDP.data() + offset, 4);
			udpData.loopTimeSecondary = loopTimeSecondary;
		} else {
			throw RuntimeError("Invalid Packet read.\n");
		}

		offset += 4;
	}

	return udpData;
}

std::vector<DataTCP> Ethernet::getTCPPacket() {
	std::lock_guard lock(_tcpReceivingMutex);

	std::vector<DataTCP> out;
	while (!_tcpReceivingBuffer.empty()) {
		out.emplace_back(_tcpReceivingBuffer.front());
		_tcpReceivingBuffer.pop();
	}

	return out;
}

uint32_t Ethernet::packetsInQueueTCP() const {
	return _packetsInList.load();
}


void Ethernet::addTCPPacket(const std::vector<uint8_t>& packet) {
	std::lock_guard lock(_tcpSendingPacketMutex);
	_tcpSendPacketBuffer.push(packet);
	_packetsInList.fetch_add(1);
}

void Ethernet::_updateTCP() {

	while (!_stopFlag) {
		if (!_connectionOpen()) {
			continue;
		}

		bool haveCommand = false;
		std::vector<uint8_t> tcpSend;
		{
			std::lock_guard lock(_tcpSendingPacketMutex);
			if (!_tcpSendPacketBuffer.empty()) {
				haveCommand = true;

				tcpSend = _tcpSendPacketBuffer.front();
				_tcpSendPacketBuffer.pop();
				_packetsInList.fetch_sub(1);
			}
		}

		try {
			if (haveCommand) {
				asio::write(_socketTCP, asio::buffer(tcpSend));
			}

			if (_socketTCP.available() >= 4) {
				uint32_t lengthPrefix;
				std::size_t len = asio::read(_socketTCP, asio::buffer(&lengthPrefix, sizeof(lengthPrefix)));

				if (len != sizeof(lengthPrefix)) {
					throw RuntimeError("Error in asio read?\n");
				}

				std::vector<uint8_t> recvArr(lengthPrefix & 0xFFFF);
				len = asio::read(_socketTCP, asio::buffer(recvArr));
				if (len != (lengthPrefix & 0xFFFF)) {
					throw RuntimeError("Did not read all data? This should be blocking op.\n");
				}

				_parseTcpPacket(recvArr);
			}

		} catch (const asio::system_error&) {
			_socketTCP.close();
		}
	}
}

bool Ethernet::_connectionOpen() {
	if (!_socketTCP.is_open()) {
		try {
			_socketTCP.connect(_remoteEndpointTCP);
			asio::ip::tcp::no_delay option(true);
			_socketTCP.set_option(option);
		} catch (const std::system_error&) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			return false;
		}
	}

	return true;
}


void Ethernet::_parseTcpPacket(const std::vector<uint8_t> &packet) {
	if (packet.size() < 4) {
		throw RuntimeError("Invalid TCP packet. Packet size: " + std::to_string(packet.size()) + "\n");
	}

	uint32_t packetOffset = 0;
	uint32_t header;
	uint32_t tmpUint;
	uint32_t tmpFloat;
	uint32_t motorIx = 0;

	while (packetOffset < packet.size()) {
		memcpy(&header, packet.data() + packetOffset, 4);
		packetOffset += 4;

		std::bitset<32> headerBits(header);
		if (headerBits[0]) {
			// Command
			CommandTCP commandTCP;
			commandTCP.motorIx = motorIx;
			motorIx++;
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

						case (TCP_COMMAND_TORQUE_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueKi = tmpFloat;
						} break;

						case (TCP_COMMAND_TORQUE_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.torqueKd = tmpFloat;
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

						case (TCP_COMMAND_VELOCITY_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityKi = tmpFloat;
						} break;

						case (TCP_COMMAND_VELOCITY_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.velocityKd = tmpFloat;
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

						case (TCP_COMMAND_POSITION_KI): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionKi = tmpFloat;
						} break;

						case (TCP_COMMAND_POSITION_KD): {
						    memcpy(&tmpFloat, packet.data() + packetOffset, 4);
						    commandTCP.positionKd = tmpFloat;
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

			// TODO: delete command from internal buffer.
		} else {
			// Data
			DataTCP dataTCP;
			dataTCP.motorIx = motorIx;
			motorIx++;
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
						} break;

						case (TCP_DATA_BUTTON_STATUS): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.buttonStatus = tmpUint;
						} break;

						case (TCP_DATA_CURRENT_LIMIT_BUS): {
							memcpy(&tmpFloat, packet.data() + packetOffset, 4);
							dataTCP.currentLimitBus = tmpFloat;
						} break;

						case (TCP_DATA_CURRENT_LIMIT_PHASE): {
							memcpy(&tmpFloat, packet.data() + packetOffset, 4);
							dataTCP.currentLimitPhase = tmpFloat;
						} break;

						case (TCP_DATA_BOARD_STATE): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.boardState = tmpUint;
						} break;

						case (TCP_DATA_DRIVING_MODE): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.drivingMode = tmpUint;
						} break;

						case (TCP_DATA_POLE_PAIRS): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.polePairs = tmpUint;
						} break;

						case (TCP_DATA_PHASE_RMS_VOLTAGE): {
							memcpy(&tmpFloat, packet.data() + packetOffset, 4);
							dataTCP.phaseRmsVoltage = tmpFloat;
						} break;

						case (TCP_DATA_ERROR_REGISTER): {
							memcpy(&tmpUint, packet.data() + packetOffset, 4);
							dataTCP.checkErrorCode = tmpUint;
						} break;

						default: {
							// Error?
						} break;
					}

					packetOffset += 4;
				}
			}

			std::lock_guard lock(_tcpReceivingMutex);
			_tcpReceivingBuffer.push(dataTCP);
		}
	}
}

std::vector<uint8_t> Ethernet::getTcpCommandPacket(const std::vector<std::pair<std::vector<uint32_t>, std::vector<uint32_t>>>& values) {
	std::vector<uint32_t> headers;
	std::vector<std::vector<uint32_t>> packetVals;
	uint32_t numValues = 0;

	for (int i = 0; i < values.size(); i++) {
		for (int v = 0; v < values[i].first.size(); v++) {
			if ((values[i].first[v] == TCP_DATA) ||
				(values[i].first[v] == TCP_COMMAND)) {
				throw RuntimeError("DATA/COMMAND header is added automatically.\n");
			}
		}

		if (values[i].first.size() != values[i].second.size()) {
			// Horrible error message.. idk.
			// Number of active header should must equal number of values.
			throw RuntimeError("Num values must equal header values.\n");
		}

		headers.emplace_back(0);
		packetVals.emplace_back();

		headers[i] |= TCP_COMMAND;
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

std::vector<uint8_t> Ethernet::getTcpDataPacket(const std::vector<std::vector<uint32_t>>& values) {
	std::vector<uint32_t> headers;

	for (int i = 0; i < values.size(); i++) {
		for (int v = 0; v < values[i].size(); v++) {
			if ((values[i][v] == TCP_DATA) ||
				(values[i][v] == TCP_COMMAND)) {
				throw RuntimeError("DATA/COMMAND header is added automatically.\n");
			}
		}

		headers.emplace_back(0);
		headers[i] |= TCP_DATA;
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
