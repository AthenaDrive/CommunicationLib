
#pragma once
#include <cstdint>

struct UDPDataFromPeripheral {
	uint32_t timestamp;
	float position;
	float velocity;
	float acceleration;
	float torque;
	float phaseCurrentA;
	float phaseCurrentB;
	float phaseCurrentC;
	float busCurrent;
	float busVoltage;
	uint32_t errorRegister;
	uint32_t loopTimeFOC;
	uint32_t loopTimeSecondary;
};

constexpr size_t offsetsUDPFromPeripheral[] = {
	offsetof(UDPDataFromPeripheral, timestamp),
	offsetof(UDPDataFromPeripheral, position),
	offsetof(UDPDataFromPeripheral, velocity),
	offsetof(UDPDataFromPeripheral, acceleration),
	offsetof(UDPDataFromPeripheral, torque),
	offsetof(UDPDataFromPeripheral, phaseCurrentA),
	offsetof(UDPDataFromPeripheral, phaseCurrentB),
	offsetof(UDPDataFromPeripheral, phaseCurrentC),
	offsetof(UDPDataFromPeripheral, busCurrent),
	offsetof(UDPDataFromPeripheral, busVoltage),
	offsetof(UDPDataFromPeripheral, errorRegister),
	offsetof(UDPDataFromPeripheral, loopTimeFOC),
	offsetof(UDPDataFromPeripheral, loopTimeSecondary)
};

struct UDPDataFromController {
	float torqueSetpoint;
	float velocitySetpoint;
	float positionSetpoint;
};

constexpr size_t offsetsUDPFromController[] = {
	offsetof(UDPDataFromController, torqueSetpoint),
	offsetof(UDPDataFromController, velocitySetpoint),
	offsetof(UDPDataFromController, positionSetpoint)
};
