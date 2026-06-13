
#ifndef COMPUTERCONTROLL_COMMUNICATIONCONFIG_HPP
#define COMPUTERCONTROLL_COMMUNICATIONCONFIG_HPP

#include <cstdint>

struct UDPData {
	uint32_t index;
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

constexpr size_t offsets[] = {
	offsetof(UDPData, index),
	offsetof(UDPData, timestamp),
	offsetof(UDPData, position),
	offsetof(UDPData, velocity),
	offsetof(UDPData, acceleration),
	offsetof(UDPData, torque),
	offsetof(UDPData, phaseCurrentA),
	offsetof(UDPData, phaseCurrentB),
	offsetof(UDPData, phaseCurrentC),
	offsetof(UDPData, busCurrent),
	offsetof(UDPData, busVoltage),
	offsetof(UDPData, errorRegister),
	offsetof(UDPData, loopTimeFOC),
	offsetof(UDPData, loopTimeSecondary)
};

#endif //COMPUTERCONTROLL_COMMUNICATIONCONFIG_HPP