
#include "Ethernet.hpp"

#include <algorithm>
#include <bit>
#include <memory>
#include <string>

#include "RuntimeError.hpp"

namespace {

std::uint32_t readU32LE(const std::uint8_t* p) {
	return static_cast<std::uint32_t>(p[0])
		| (static_cast<std::uint32_t>(p[1]) << 8)
		| (static_cast<std::uint32_t>(p[2]) << 16)
		| (static_cast<std::uint32_t>(p[3]) << 24);
}

void writeU32LE(std::uint8_t* p, std::uint32_t value) {
	p[0] = static_cast<std::uint8_t>(value & 0xFFu);
	p[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
	p[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
	p[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

void appendU32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
	out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
	out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
	out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
	out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

float bitsToFloat(std::uint32_t bits) {
	return std::bit_cast<float>(bits);
}

std::uint32_t floatToBits(float value) {
	return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t buildHeaderBits(const std::vector<std::uint32_t>& bits, std::uint32_t base, const char* kind) {
	std::uint32_t header = base;
	std::uint32_t seen = 0;

	for (const std::uint32_t bit : bits) {
		// Bit 0 encodes DATA vs COMMAND and is added automatically.
		// Every other entry must be exactly one bit, otherwise the value
		// count no longer matches the header.
		if (bit == 0 || bit == static_cast<std::uint32_t>(TCP_COMMAND) || !std::has_single_bit(bit)) {
			throw RuntimeError(std::string("Invalid ") + kind + " header bit.\n");
		}

		if ((seen & bit) != 0) {
			throw RuntimeError(std::string("Duplicate ") + kind + " header bit.\n");
		}

		seen |= bit;
		header |= bit;
	}

	return header;
}

} // namespace

Ethernet::Ethernet()
	: _workGuard(asio::make_work_guard(_ioContext)),
	  _boardEndpointUDP(asio::ip::make_address_v4("192.168.0.2"), 5000),
	  _remoteEndpointTCP(asio::ip::make_address_v4("192.168.0.2"), 5001),
	  _socketUDP(_ioContext, asio::ip::udp::endpoint(asio::ip::udp::v4(), 5000)),
	  _socketTCP(_ioContext),
	  _udpSendTimer(_ioContext),
	  _udpReceiveRetryTimer(_ioContext),
	  _tcpReconnectTimer(_ioContext) {

	// All work is posted so it starts only once the object is fully constructed.
	asio::post(_ioContext, [this] {
		_startUdpReceive();
		_startUdpSendTimer();
		_startTcpConnect();
	});

	_ioThread = std::jthread([this] {
		// A thrown handler must never escape the thread (that would call
		// std::terminate and kill the whole process). Keep the loop alive.
		for (;;) {
			try {
				_ioContext.run();
				return;
			} catch (const std::exception&) {
				// Drop the failure and continue serving the remaining work.
			} catch (...) {
				// Same, for non-standard exceptions.
			}
		}
	});
}

Ethernet::~Ethernet() {
	_stopFlag.store(true);

	// stop() makes run() return immediately, even while the thread is
	// blocked waiting for network events. This is what prevents the old
	// blocking receive_from()/asio::read() + join() deadlock.
	_ioContext.stop();

	if (_ioThread.joinable()) {
		_ioThread.join();
	}

	// Safe to touch the sockets now that the event loop has been joined.
	asio::error_code ec;
	_socketUDP.close(ec);
	_socketTCP.close(ec);
}

// ---------------------------------------------------------------------------
// UDP
// ---------------------------------------------------------------------------

void Ethernet::setUDPCommand(const std::vector<CommandUDP>& data) {
	std::vector<CommandUDP> actualData;

	// Sort by motor index and discard duplicates (last one wins).
	for (const auto& d : data) {
		if (d.motorIx < 0) {
			// Negative indices, including the default -1, mean "unset".
			continue;
		}

		const auto index = static_cast<std::size_t>(d.motorIx);
		if (index >= kCommandBufferSize / 4) {
			throw RuntimeError(
				"UDP command motor index out of range: " + std::to_string(d.motorIx) + "\n");
		}

		if (actualData.size() <= index) {
			actualData.resize(index + 1);
		}
		actualData[index] = d;
	}

	// Build into a local buffer first so a failure cannot corrupt the packet
	// currently being sent.
	std::array<std::uint8_t, kCommandBufferSize> packet{};
	std::size_t outerOffset = 0;

	for (const auto& d : actualData) {
		if (d.motorIx < 0) {
			continue;
		}

		if (outerOffset + 4 > packet.size()) {
			throw RuntimeError("UDP command packet exceeds buffer size.\n");
		}

		std::size_t cursor = outerOffset + 4; // reserve space for this motor's header
		std::uint32_t header = 0;

		const auto add = [&](std::uint32_t bit, std::uint32_t raw) {
			if (cursor + 4 > packet.size()) {
				throw RuntimeError("UDP command packet exceeds buffer size.\n");
			}
			writeU32LE(packet.data() + cursor, raw);
			cursor += 4;
			header |= bit;
		};

		if (d.torqueSetpoint) {
			add(UDP_COMMAND_TORQUE_SETPOINT, floatToBits(*d.torqueSetpoint));
		}
		if (d.velocitySetpoint) {
			add(UDP_COMMAND_VELOCITY_SETPOINT, floatToBits(*d.velocitySetpoint));
		}
		if (d.positionSetpoint) {
			add(UDP_COMMAND_POSITION_SETPOINT, floatToBits(*d.positionSetpoint));
		}
		if (d.drivingMode) {
			add(UDP_COMMAND_DRIVING_MODE, *d.drivingMode);
		}
		if (d.maxTorque) {
			add(UDP_COMMAND_MAX_TORQUE, floatToBits(*d.maxTorque));
		}
		if (d.maxVelocity) {
			add(UDP_COMMAND_MAX_VELOCITY, floatToBits(*d.maxVelocity));
		}
		if (d.currentLimitBus) {
			add(UDP_COMMAND_CURRENT_LIMIT_BUS, floatToBits(*d.currentLimitBus));
		}
		if (d.currentLimitPhase) {
			add(UDP_COMMAND_CURRENT_LIMIT_PHASE, floatToBits(*d.currentLimitPhase));
		}
		if (d.debugFloat0) {
			add(DEBUG_FLOAT_0, floatToBits(*d.debugFloat0));
		}
		if (d.debugFloat1) {
			add(DEBUG_FLOAT_1, floatToBits(*d.debugFloat1));
		}
		if (d.debugFloat2) {
			add(DEBUG_FLOAT_2, floatToBits(*d.debugFloat2));
		}
		if (d.debugFloat3) {
			add(DEBUG_FLOAT_3, floatToBits(*d.debugFloat3));
		}
		if (d.debugInt0) {
			add(DEBUG_INT_0, static_cast<std::uint32_t>(*d.debugInt0));
		}
		if (d.debugInt1) {
			add(DEBUG_INT_1, static_cast<std::uint32_t>(*d.debugInt1));
		}
		if (d.debugUint0) {
			add(DEBUG_UINT_0, *d.debugUint0);
		}
		if (d.debugUint1) {
			// DEBUG_UINT_1 is defined as "1 << 31" in Protocol.hpp. Since 1
			// is a signed int, that macro evaluates to INT_MIN (a negative
			// mask) rather than 2147483648, so build the unsigned bit here.
			add(std::uint32_t{1} << 31, *d.debugUint1);
		}

		writeU32LE(packet.data() + outerOffset, header);
		outerOffset = cursor;
	}

	std::lock_guard lock(_bufferCommandUDPMutex);
	std::copy_n(packet.begin(), outerOffset, _bufferCommandUDP.begin());
	_bufferCommandUDPLen = outerOffset;
}

std::vector<DataUDP> Ethernet::getUDPData() {
	std::lock_guard lock(_udpDataMutex);
	return _dataUDP;
}

void Ethernet::_startUdpReceive() {
	if (_stopFlag.load()) {
		return;
	}

	_socketUDP.async_receive_from(
		asio::buffer(_bufferDataUDP),
		_senderEndpointUDP,
		[this](const asio::error_code& ec, const std::size_t len) {
			if (_stopFlag.load()) {
				return;
			}

			if (ec) {
				if (ec == asio::error::operation_aborted) {
					return;
				}

				// Retry after a short delay so a persistent socket error
				// cannot turn into a tight loop.
				_udpReceiveRetryTimer.expires_after(kUdpRetryDelay);
				_udpReceiveRetryTimer.async_wait([this](const asio::error_code& timerEc) {
					if (!timerEc && !_stopFlag.load()) {
						_startUdpReceive();
					}
				});
				return;
			}

			_handleUdpDatagram(len);
			_startUdpReceive();
		});
}

void Ethernet::_handleUdpDatagram(const std::size_t len) {
	if (len < 4) {
		return;
	}

	std::vector<DataUDP> packets;
	try {
		packets.reserve(8);

		std::size_t offset = 0;
		std::int32_t motorIx = 1; // Keep the original 1-based data numbering.

		while (offset < len) {
			DataUDP data;
			if (!_parseSingleUdp(offset, len, data)) {
				// Malformed/truncated datagram: drop the complete datagram
				// rather than publishing a partially decoded one.
				return;
			}
			data.motorIx = motorIx++;
			packets.push_back(std::move(data));
		}
	} catch (const std::exception&) {
		return;
	}

	std::lock_guard lock(_udpDataMutex);
	_dataUDP = std::move(packets);
}

bool Ethernet::_parseSingleUdp(std::size_t& offset, const std::size_t len, DataUDP& out) const {
	const std::uint8_t* data = _bufferDataUDP.data();

	if (offset + 4 > len) {
		return false;
	}

	const std::uint32_t header = readU32LE(data + offset);
	offset += 4;

	const auto takeU32 = [&](std::uint32_t& raw) -> bool {
		if (offset + 4 > len) {
			return false;
		}
		raw = readU32LE(data + offset);
		offset += 4;
		return true;
	};

	for (int bit = 0; bit < 32; ++bit) {
		if ((header & (std::uint32_t{1} << bit)) == 0) {
			continue;
		}

		std::uint32_t raw = 0;
		if (!takeU32(raw)) {
			return false;
		}

		switch (bit) {
			case 0: out.timestamp = raw; break;
			case 1: out.position = bitsToFloat(raw); break;
			case 2: out.velocity = bitsToFloat(raw); break;
			case 3: out.acceleration = bitsToFloat(raw); break;
			case 4: out.torque = bitsToFloat(raw); break;
			case 5: out.phaseCurrentA = bitsToFloat(raw); break;
			case 6: out.phaseCurrentB = bitsToFloat(raw); break;
			case 7: out.phaseCurrentC = bitsToFloat(raw); break;
			case 8: out.busCurrent = bitsToFloat(raw); break;
			case 9: out.busVoltage = bitsToFloat(raw); break;
			case 10: out.errorRegister = raw; break;
			case 11: out.loopTimeFOC = raw; break;
			case 12: out.loopTimeSecondary = raw; break;
			case 13: out.accX = bitsToFloat(raw); break;
			case 14: out.accY = bitsToFloat(raw); break;
			case 15: out.accZ = bitsToFloat(raw); break;
			case 16: out.gyroX = bitsToFloat(raw); break;
			case 17: out.gyroY = bitsToFloat(raw); break;
			case 18: out.gyroZ = bitsToFloat(raw); break;
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
				// Reserved/unused bits: value still consumed above.
				break;
			case 24: out.debugFloat0 = bitsToFloat(raw); break;
			case 25: out.debugFloat1 = bitsToFloat(raw); break;
			case 26: out.debugFloat2 = bitsToFloat(raw); break;
			case 27: out.debugFloat3 = bitsToFloat(raw); break;
			case 28: out.debugInt0 = static_cast<std::int32_t>(raw); break;
			case 29: out.debugInt1 = static_cast<std::int32_t>(raw); break;
			case 30: out.debugUint0 = raw; break;
			case 31: out.debugUint1 = raw; break;
			default: break;
		}
	}

	return true;
}

void Ethernet::_startUdpSendTimer() {
	_udpSendTimer.expires_after(kUdpSendInterval);
	_udpSendTimer.async_wait([this](const asio::error_code& ec) {
		if (ec || _stopFlag.load()) {
			return;
		}

		try {
			_sendUdp();
		} catch (const std::exception&) {
			// Never let a send allocation failure stop the timer chain.
		}

		_startUdpSendTimer();
	});
}

void Ethernet::_sendUdp() {
	std::shared_ptr<std::vector<std::uint8_t>> payload;
	{
		std::lock_guard lock(_bufferCommandUDPMutex);
		if (_bufferCommandUDPLen == 0) {
			return;
		}

		payload = std::make_shared<std::vector<std::uint8_t>>(
			_bufferCommandUDP.data(),
			_bufferCommandUDP.data() + _bufferCommandUDPLen);
	}

	// UDP is best-effort; completion errors are intentionally ignored.
	_socketUDP.async_send_to(
		asio::buffer(*payload),
		_boardEndpointUDP,
		[payload](const asio::error_code&, std::size_t) {});
}

// ---------------------------------------------------------------------------
// TCP
// ---------------------------------------------------------------------------

std::vector<DataTCP> Ethernet::getTCPPacket() {
	std::lock_guard lock(_tcpReceivingMutex);

	std::vector<DataTCP> out;
	out.reserve(_tcpReceivingBuffer.size());
	while (!_tcpReceivingBuffer.empty()) {
		out.push_back(std::move(_tcpReceivingBuffer.front()));
		_tcpReceivingBuffer.pop();
	}

	return out;
}

std::uint32_t Ethernet::packetsInQueueTCP() const {
	return _packetsInList.load(std::memory_order_relaxed);
}

void Ethernet::addTCPPacket(const std::vector<std::uint8_t>& packet) {
	{
		std::lock_guard lock(_tcpSendingPacketMutex);
		_tcpSendPacketBuffer.push(packet);
		_packetsInList.fetch_add(1, std::memory_order_relaxed);
	}

	asio::post(_ioContext, [this] {
		_sendNextTcpPacket();
	});
}

void Ethernet::_startTcpConnect() {
	if (_stopFlag.load() || _socketTCP.is_open()) {
		return;
	}

	_socketTCP.async_connect(_remoteEndpointTCP, [this](const asio::error_code& ec) {
		if (_stopFlag.load()) {
			return;
		}

		if (ec) {
			if (ec == asio::error::operation_aborted) {
				return;
			}
			asio::error_code closeEc;
			_socketTCP.close(closeEc);
			_scheduleTcpReconnect();
			return;
		}

		asio::error_code optionEc;
		_socketTCP.set_option(asio::ip::tcp::no_delay(true), optionEc);

		_tcpConnected = true;
		_tcpWriting = false;

		_startTcpReadHeader();
		_sendNextTcpPacket(); // Flush anything queued while connecting.
	});
}

void Ethernet::_scheduleTcpReconnect() {
	if (_stopFlag.load()) {
		return;
	}

	// Re-arming cancels any previous wait, whose handler exits on
	// operation_aborted.
	_tcpReconnectTimer.expires_after(kTcpReconnectDelay);
	_tcpReconnectTimer.async_wait([this](const asio::error_code& ec) {
		if (!ec && !_stopFlag.load()) {
			_startTcpConnect();
		}
	});
}

void Ethernet::_handleTcpDisconnect() {
	_tcpConnected = false;
	_tcpWriting = false;

	asio::error_code ec;
	_socketTCP.close(ec);

	_scheduleTcpReconnect();
}

void Ethernet::_startTcpReadHeader() {
	if (_stopFlag.load() || !_tcpConnected) {
		return;
	}

	// async_read reads exactly the requested number of bytes; a truncated
	// frame results in an error_code instead of an out-of-bounds read.
	asio::async_read(
		_socketTCP,
		asio::buffer(_tcpLengthPrefix),
		[this](const asio::error_code& ec, std::size_t) {
			if (_stopFlag.load()) {
				return;
			}

			if (ec) {
				if (ec == asio::error::operation_aborted) {
					return;
				}
				_handleTcpDisconnect();
				return;
			}

			const std::uint32_t prefix = readU32LE(_tcpLengthPrefix.data());
			const std::size_t payloadLen = prefix & 0xFFFFu;

			if (payloadLen == 0) {
				_startTcpReadHeader();
				return;
			}

			if (payloadLen > kMaxTcpPayloadSize) {
				_handleTcpDisconnect();
				return;
			}

			try {
				_tcpReadBuffer.resize(payloadLen);
			} catch (const std::exception&) {
				_handleTcpDisconnect();
				return;
			}

			_startTcpReadBody();
		});
}

void Ethernet::_startTcpReadBody() {
	asio::async_read(
		_socketTCP,
		asio::buffer(_tcpReadBuffer),
		[this](const asio::error_code& ec, std::size_t) {
			if (_stopFlag.load()) {
				return;
			}

			if (ec) {
				if (ec == asio::error::operation_aborted) {
					return;
				}
				_handleTcpDisconnect();
				return;
			}

			try {
				_parseTcpPacket(_tcpReadBuffer);
			} catch (const std::exception&) {
				// Malformed packet: drop it and keep the connection alive.
			}

			_startTcpReadHeader();
		});
}

void Ethernet::_sendNextTcpPacket() {
	if (_stopFlag.load() || !_tcpConnected || _tcpWriting || !_socketTCP.is_open()) {
		return;
	}

	std::shared_ptr<std::vector<std::uint8_t>> packet;
	{
		std::lock_guard lock(_tcpSendingPacketMutex);
		if (_tcpSendPacketBuffer.empty()) {
			return;
		}

		try {
			packet = std::make_shared<std::vector<std::uint8_t>>(_tcpSendPacketBuffer.front());
		} catch (const std::exception&) {
			// Leave the packet queued; it can be retried later.
			return;
		}

		_tcpSendPacketBuffer.pop();
		_packetsInList.fetch_sub(1, std::memory_order_relaxed);
	}

	_tcpWriting = true;
	asio::async_write(
		_socketTCP,
		asio::buffer(*packet),
		[this, packet](const asio::error_code& ec, std::size_t) {
			_tcpWriting = false;

			if (_stopFlag.load()) {
				return;
			}

			if (ec) {
				if (ec == asio::error::operation_aborted) {
					return;
				}
				_handleTcpDisconnect();
				return;
			}

			_sendNextTcpPacket();
		});
}

bool Ethernet::_parseTcpPacket(const std::vector<std::uint8_t>& packet) {
	if (packet.size() < 4) {
		return false;
	}

	const std::uint8_t* data = packet.data();
	const std::size_t size = packet.size();

	std::vector<DataTCP> received;
	std::size_t offset = 0;
	std::uint32_t motorIx = 0;

	// The packet builders put a length prefix in front of every motor's
	// header (nested framing). Some devices echo that format, while others
	// use one flat sequence of header/value pairs. A real TCP header can
	// never contain the prefix identifier: it would require reserved bits
	// 22 and 23 to be set, so this check is unambiguous.
	constexpr std::uint32_t kPrefixHigh =
		static_cast<std::uint32_t>(PREFIX_IDENTIFIER) >> 16;

	const auto takeU32 = [&](std::uint32_t& raw) -> bool {
		if (offset + 4 > size) {
			return false;
		}
		raw = readU32LE(data + offset);
		offset += 4;
		return true;
	};

	while (offset < size) {
		std::uint32_t header = 0;
		if (!takeU32(header)) {
			return false;
		}

		if ((header >> 16) == kPrefixHigh) {
			// Per-motor framing prefix: skip it and read the real header.
			continue;
		}

		if ((header & 1u) != 0) {
			// Command frame: parse/skip its values, but only DataTCP is
			// exposed by this class (same behaviour as before).
			for (int bit = 1; bit < 32; ++bit) {
				if ((header & (std::uint32_t{1} << bit)) != 0) {
					std::uint32_t ignored = 0;
					if (!takeU32(ignored)) {
						return false;
					}
				}
			}
			++motorIx;
			continue;
		}

		DataTCP dataTCP;
		dataTCP.motorIx = static_cast<std::int32_t>(motorIx++);

		for (int bit = 1; bit < 32; ++bit) {
			if ((header & (std::uint32_t{1} << bit)) == 0) {
				continue;
			}

			std::uint32_t raw = 0;
			if (!takeU32(raw)) {
				return false;
			}

			switch (bit) {
				case 1: dataTCP.busVoltage = bitsToFloat(raw); break;
				case 2: dataTCP.busCurrent = bitsToFloat(raw); break;
				case 4: dataTCP.buttonStatus = raw; break;
				case 5: dataTCP.currentLimitBus = bitsToFloat(raw); break;
				case 6: dataTCP.currentLimitPhase = bitsToFloat(raw); break;
				case 7: dataTCP.boardState = raw; break;
				case 8: dataTCP.drivingMode = raw; break;
				case 9: dataTCP.polePairs = raw; break;
				case 10: dataTCP.phaseRmsVoltage = bitsToFloat(raw); break;
				case 11: dataTCP.checkErrorCode = raw; break;
				case 12: dataTCP.temperature = bitsToFloat(raw); break;
				case 24: dataTCP.debugFloat0 = bitsToFloat(raw); break;
				case 25: dataTCP.debugFloat1 = bitsToFloat(raw); break;
				case 26: dataTCP.debugFloat2 = bitsToFloat(raw); break;
				case 27: dataTCP.debugFloat3 = bitsToFloat(raw); break;
				case 28: dataTCP.debugInt0 = static_cast<std::int32_t>(raw); break;
				case 29: dataTCP.debugInt1 = static_cast<std::int32_t>(raw); break;
				case 30: dataTCP.debugUint0 = raw; break;
				case 31: dataTCP.debugUint1 = raw; break;
				default: break;
			}
		}

		received.push_back(std::move(dataTCP));
	}

	// Commit only once the whole frame was decoded successfully.
	std::lock_guard lock(_tcpReceivingMutex);
	for (auto& dataTCP : received) {
		_tcpReceivingBuffer.push(std::move(dataTCP));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Packet builders
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> Ethernet::getTcpCommandPacket(
	const std::vector<std::pair<std::vector<std::uint32_t>, std::vector<std::uint32_t>>>& values) {

	constexpr std::size_t kMaxPrefixLength = 0xFFFF;

	std::vector<std::uint32_t> headers;
	headers.reserve(values.size());

	std::size_t totalValues = 0;
	for (const auto& [headerBits, fieldValues] : values) {
		if (headerBits.size() != fieldValues.size()) {
			// Number of active header bits must equal number of values.
			throw RuntimeError("Num values must equal header values.\n");
		}

		headers.push_back(buildHeaderBits(headerBits, TCP_COMMAND, "command"));
		totalValues += fieldValues.size();
	}

	std::vector<std::uint8_t> out;
	out.reserve(headers.size() * 8 + totalValues * 4);

	std::size_t remainingValues = totalValues;
	for (std::size_t i = 0; i < headers.size(); ++i) {
		const std::size_t prefixLength =
			(2 * (headers.size() - i) - 1 + remainingValues) * 4;

		if (prefixLength > kMaxPrefixLength) {
			throw RuntimeError("TCP command packet is too long for the 16-bit length prefix.\n");
		}

		appendU32LE(out, PREFIX(static_cast<std::uint32_t>(prefixLength)));
		appendU32LE(out, headers[i]);

		for (const auto value : values[i].second) {
			appendU32LE(out, value);
			--remainingValues;
		}
	}

	return out;
}

std::vector<std::uint8_t> Ethernet::getTcpDataPacket(
	const std::vector<std::vector<std::uint32_t>>& values) {

	constexpr std::size_t kMaxPrefixLength = 0xFFFF;

	std::vector<std::uint32_t> headers;
	headers.reserve(values.size());

	for (const auto& headerBits : values) {
		headers.push_back(buildHeaderBits(headerBits, TCP_DATA, "data"));
	}

	std::vector<std::uint8_t> out;
	out.reserve(headers.size() * 8);

	for (std::size_t i = 0; i < headers.size(); ++i) {
		const std::size_t prefixLength = (2 * (headers.size() - i) - 1) * 4;

		if (prefixLength > kMaxPrefixLength) {
			throw RuntimeError("TCP data packet is too long for the 16-bit length prefix.\n");
		}

		appendU32LE(out, PREFIX(static_cast<std::uint32_t>(prefixLength)));
		appendU32LE(out, headers[i]);
	}

	return out;
}
