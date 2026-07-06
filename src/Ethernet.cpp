
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

std::vector<UDPDataFromPeripheral> Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _udpData;
}

void Ethernet::_updateUDP() {

	while (!_stopFlag) {
		int offset = 0;
		size_t len = _socketUDP.receive_from(asio::buffer(_bufferUDP), _senderEndpointUDP);
		if (len < 4) {return;}

		std::vector<UDPDataFromPeripheral> newPackets;
		while (len > offset) {
			newPackets.emplace_back(readSingleUDP(offset, len));
		}

		std::lock_guard lock(_udpDataMutex);
		_udpData = newPackets;
	}
}

UDPDataFromPeripheral Ethernet::readSingleUDP(int &offset, int len) {
	if (len <= offset) {
		throw std::runtime_error("Invalid Packet read.\n");
	}

	uint32_t header;
	std::memcpy(&header, _bufferUDP.data() + offset, 4);
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

	UDPDataFromPeripheral udpData{};
	for (int i = 0; i < numValues; i++) {
		int packetIx = positions[i];
		auto* base = reinterpret_cast<std::byte*>(&udpData);
		std::memcpy(base + offsetsUDPFromPeripheral[packetIx], _bufferUDP.data() + offset, 4);
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


void Ethernet::addTCPPacket(const std::vector<uint8_t>& packet) {
	std::lock_guard lock(_tcpSendingPacketMutex);
	_tcpSendPacketBuffer.push(packet);
	packetsInList.fetch_add(1);
}

void Ethernet::_updateTCP() {

	while (!_stopFlag) {
		if (!_socketTCP.is_open()) {
			try {
				_socketTCP.connect(_remoteEndpointTCP);
				asio::ip::tcp::no_delay option(true);
				_socketTCP.set_option(option);
				std::cout << "TCP connected" << std::endl;
			} catch (const std::system_error&) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		while (true) {
			std::lock_guard lock(_tcpSendingPacketMutex);
			if (_tcpSendPacketBuffer.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			} else {
				break;
			}
		}

		bool haveCommand = false;
		std::vector<uint8_t> tcpSend;
		{
			std::lock_guard lock(_tcpSendingPacketMutex);
			if (!_tcpSendPacketBuffer.empty()) {
				haveCommand = true;
				tcpSend = _tcpSendPacketBuffer.front();
				_tcpSendPacketBuffer.pop();
				packetsInList.fetch_sub(1);
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
						}

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
