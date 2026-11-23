
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Ethernet.hpp"
#include "Protocol.hpp"

namespace py = pybind11;

PYBIND11_MODULE(EthernetPython, m) {
	m.doc() = "Ethernet communication for AthenaDrive.";

	m.attr("PREFIX_IDENTIFIER") = PREFIX_IDENTIFIER;

	// DEBUG
	m.attr("DEBUG_FLOAT_0") = DEBUG_FLOAT_0;
	m.attr("DEBUG_FLOAT_1") = DEBUG_FLOAT_1;
	m.attr("DEBUG_FLOAT_2") = DEBUG_FLOAT_2;
	m.attr("DEBUG_FLOAT_3") = DEBUG_FLOAT_3;
	m.attr("DEBUG_INT_0") = DEBUG_INT_0;
	m.attr("DEBUG_INT_1") = DEBUG_INT_1;
	m.attr("DEBUG_UINT_0") = DEBUG_UINT_0;
	m.attr("DEBUG_UINT_1") = DEBUG_UINT_1;

	// UDP DATA
	m.attr("UDP_DATA_TIMESTAMP") = UDP_DATA_TIMESTAMP;
	m.attr("UDP_DATA_POSITION") = UDP_DATA_POSITION;
	m.attr("UDP_DATA_VELOCITY") = UDP_DATA_VELOCITY;
	m.attr("UDP_DATA_ACCELERATION") = UDP_DATA_ACCELERATION;
	m.attr("UDP_DATA_TORQUE") = UDP_DATA_TORQUE;
	m.attr("UDP_DATA_PHASE_CURRENT_A") = UDP_DATA_PHASE_CURRENT_A;
	m.attr("UDP_DATA_PHASE_CURRENT_B") = UDP_DATA_PHASE_CURRENT_B;
	m.attr("UDP_DATA_PHASE_CURRENT_C") = UDP_DATA_PHASE_CURRENT_C;
	m.attr("UDP_DATA_BUS_CURRENT") = UDP_DATA_BUS_CURRENT;
	m.attr("UDP_DATA_BUS_VOLTAGE") = UDP_DATA_BUS_VOLTAGE;
	m.attr("UDP_DATA_ERROR_REGISTER") = UDP_DATA_ERROR_REGISTER;
	m.attr("UDP_DATA_LOOP_TIME_FOC") = UDP_DATA_LOOP_TIME_FOC;
	m.attr("UDP_DATA_LOOP_TIME_SECONDARY") = UDP_DATA_LOOP_TIME_SECONDARY;
	m.attr("UDP_DATA_ACCELERATION_X") = UDP_DATA_ACCELERATION_X;
	m.attr("UDP_DATA_ACCELERATION_Y") = UDP_DATA_ACCELERATION_Y;
	m.attr("UDP_DATA_ACCELERATION_Z") = UDP_DATA_ACCELERATION_Z;
	m.attr("UDP_DATA_GYRO_X") = UDP_DATA_GYRO_X;
	m.attr("UDP_DATA_GYRO_Y") = UDP_DATA_GYRO_Y;
	m.attr("UDP_DATA_GYRO_Z") = UDP_DATA_GYRO_Z;

	// UDP COMMANDS
	m.attr("UDP_COMMAND_TORQUE_SETPOINT") = UDP_COMMAND_TORQUE_SETPOINT;
	m.attr("UDP_COMMAND_VELOCITY_SETPOINT") = UDP_COMMAND_VELOCITY_SETPOINT;
	m.attr("UDP_COMMAND_POSITION_SETPOINT") = UDP_COMMAND_POSITION_SETPOINT;
	m.attr("UDP_COMMAND_MAX_TORQUE") = UDP_COMMAND_MAX_TORQUE;
	m.attr("UDP_COMMAND_MAX_VELOCITY") = UDP_COMMAND_MAX_VELOCITY;
	m.attr("UDP_COMMAND_CURRENT_LIMIT_BUS") = UDP_COMMAND_CURRENT_LIMIT_BUS;
	m.attr("UDP_COMMAND_CURRENT_LIMIT_PHASE") = UDP_COMMAND_CURRENT_LIMIT_PHASE;
	m.attr("UDP_COMMAND_DRIVING_MODE") = UDP_COMMAND_DRIVING_MODE;

	// TCP DATA
	m.attr("TCP_DATA") = TCP_DATA;
	m.attr("TCP_DATA_BUS_VOLTAGE") = TCP_DATA_BUS_VOLTAGE;
	m.attr("TCP_DATA_BUS_CURRENT") = TCP_DATA_BUS_CURRENT;

	m.attr("TCP_DATA_BUTTON_STATUS") = TCP_DATA_BUTTON_STATUS;
	m.attr("TCP_DATA_CURRENT_LIMIT_BUS") = TCP_DATA_CURRENT_LIMIT_BUS;
	m.attr("TCP_DATA_CURRENT_LIMIT_PHASE") = TCP_DATA_CURRENT_LIMIT_PHASE;
	m.attr("TCP_DATA_BOARD_STATE") = TCP_DATA_BOARD_STATE;
	m.attr("TCP_DATA_DRIVING_MODE") = TCP_DATA_DRIVING_MODE;
	m.attr("TCP_DATA_POLE_PAIRS") = TCP_DATA_POLE_PAIRS;
	m.attr("TCP_DATA_PHASE_RMS_VOLTAGE") = TCP_DATA_PHASE_RMS_VOLTAGE;
	m.attr("TCP_DATA_ERROR_REGISTER") = TCP_DATA_ERROR_REGISTER;
	m.attr("TCP_DATA_TEMPERATURE") = TCP_DATA_TEMPERATURE;

	// TCP COMMANDS
	m.attr("TCP_COMMAND") = TCP_COMMAND;
	m.attr("TCP_COMMAND_TORQUE_SETPOINT") = TCP_COMMAND_TORQUE_SETPOINT;
	m.attr("TCP_COMMAND_TORQUE_KP") = TCP_COMMAND_TORQUE_KP;
	m.attr("TCP_COMMAND_TORQUE_KD") = TCP_COMMAND_TORQUE_KD;
	m.attr("TCP_COMMAND_TORQUE_KI") = TCP_COMMAND_TORQUE_KI;
	m.attr("TCP_COMMAND_TORQUE_LIMIT") = TCP_COMMAND_TORQUE_LIMIT;
	m.attr("TCP_COMMAND_VELOCITY_SETPOINT") = TCP_COMMAND_VELOCITY_SETPOINT;
	m.attr("TCP_COMMAND_VELOCITY_KP") = TCP_COMMAND_VELOCITY_KP;
	m.attr("TCP_COMMAND_VELOCITY_KD") = TCP_COMMAND_VELOCITY_KD;
	m.attr("TCP_COMMAND_VELOCITY_KI") = TCP_COMMAND_VELOCITY_KI;
	m.attr("TCP_COMMAND_VELOCITY_LIMIT") = TCP_COMMAND_VELOCITY_LIMIT;
	m.attr("TCP_COMMAND_POSITION_SETPOINT") = TCP_COMMAND_POSITION_SETPOINT;
	m.attr("TCP_COMMAND_POSITION_KP") = TCP_COMMAND_POSITION_KP;
	m.attr("TCP_COMMAND_POSITION_KD") = TCP_COMMAND_POSITION_KD;
	m.attr("TCP_COMMAND_POSITION_KI") = TCP_COMMAND_POSITION_KI;
	m.attr("TCP_COMMAND_DRIVING_MODE") = TCP_COMMAND_DRIVING_MODE;
	m.attr("TCP_COMMAND_CURRENT_LIMIT_BUS") = TCP_COMMAND_CURRENT_LIMIT_BUS;
	m.attr("TCP_COMMAND_CURRENT_LIMIT_PHASE") = TCP_COMMAND_CURRENT_LIMIT_PHASE;
	m.attr("TCP_COMMAND_POLE_PAIRS") = TCP_COMMAND_POLE_PAIRS;
	m.attr("TCP_COMMAND_UDP_DATA_CONFIG") = TCP_COMMAND_UDP_DATA_CONFIG;
	m.attr("TCP_COMMAND_CLEAR_ERROR_REGISTER") = TCP_COMMAND_CLEAR_ERROR_REGISTER;
	m.attr("TCP_COMMAND_LED_STATUS") = TCP_COMMAND_LED_STATUS;

	py::class_<DataUDP>(m, "DataUDP")
		.def(py::init<>())
		.def_readwrite("motorIx", &DataUDP::motorIx)
		.def_readwrite("timestamp", &DataUDP::timestamp)
		.def_readwrite("position", &DataUDP::position)
		.def_readwrite("velocity", &DataUDP::velocity)
		.def_readwrite("acceleration", &DataUDP::acceleration)
		.def_readwrite("torque", &DataUDP::torque)
		.def_readwrite("phaseCurrentA", &DataUDP::phaseCurrentA)
		.def_readwrite("phaseCurrentB", &DataUDP::phaseCurrentB)
		.def_readwrite("phaseCurrentC", &DataUDP::phaseCurrentC)
		.def_readwrite("busCurrent", &DataUDP::busCurrent)
		.def_readwrite("busVoltage", &DataUDP::busVoltage)
		.def_readwrite("errorRegister", &DataUDP::errorRegister)
		.def_readwrite("loopTimeFOC", &DataUDP::loopTimeFOC)
		.def_readwrite("loopTimeSecondary", &DataUDP::loopTimeSecondary)
		.def_readwrite("accX", &DataUDP::accX)
		.def_readwrite("accY", &DataUDP::accY)
		.def_readwrite("accZ", &DataUDP::accZ)
		.def_readwrite("gyroX", &DataUDP::gyroX)
		.def_readwrite("gyroY", &DataUDP::gyroY)
		.def_readwrite("gyroZ", &DataUDP::gyroZ)
		.def_readwrite("debugFloat0", &DataUDP::debugFloat0)
		.def_readwrite("debugFloat1", &DataUDP::debugFloat1)
		.def_readwrite("debugFloat2", &DataUDP::debugFloat2)
		.def_readwrite("debugFloat3", &DataUDP::debugFloat3)
		.def_readwrite("debugInt0", &DataUDP::debugInt0)
		.def_readwrite("debugInt1", &DataUDP::debugInt1)
		.def_readwrite("debugUint0", &DataUDP::debugUint0)
		.def_readwrite("debugUint1", &DataUDP::debugUint1)
	;

	py::class_<CommandUDP>(m, "CommandUDP")
		.def(py::init<>())
		.def_readwrite("motorIx", &CommandUDP::motorIx)
		.def_readwrite("torqueSetpoint", &CommandUDP::torqueSetpoint)
		.def_readwrite("velocitySetpoint", &CommandUDP::velocitySetpoint)
		.def_readwrite("positionSetpoint", &CommandUDP::positionSetpoint)
		.def_readwrite("drivingMode", &CommandUDP::drivingMode)
		.def_readwrite("maxTorque", &CommandUDP::maxTorque)
		.def_readwrite("maxVelocity", &CommandUDP::maxVelocity)
		.def_readwrite("currentLimitBus", &CommandUDP::currentLimitBus)
		.def_readwrite("currentLimitPhase", &CommandUDP::currentLimitPhase)
		.def_readwrite("debugFloat0", &CommandUDP::debugFloat0)
		.def_readwrite("debugFloat1", &CommandUDP::debugFloat1)
		.def_readwrite("debugFloat2", &CommandUDP::debugFloat2)
		.def_readwrite("debugFloat3", &CommandUDP::debugFloat3)
		.def_readwrite("debugInt0", &CommandUDP::debugInt0)
		.def_readwrite("debugInt1", &CommandUDP::debugInt1)
		.def_readwrite("debugUint0", &CommandUDP::debugUint0)
		.def_readwrite("debugUint1", &CommandUDP::debugUint1)
	;

	py::class_<DataTCP>(m, "DataTCP")
		.def(py::init<>())
		.def_readwrite("motorIx", &DataTCP::motorIx)
		.def_readwrite("busVoltage", &DataTCP::busVoltage)
		.def_readwrite("busCurrent", &DataTCP::busCurrent)
		.def_readwrite("buttonStatus", &DataTCP::buttonStatus)
		.def_readwrite("currentLimitBus", &DataTCP::currentLimitBus)
		.def_readwrite("currentLimitPhase", &DataTCP::currentLimitPhase)
		.def_readwrite("boardState", &DataTCP::boardState)
		.def_readwrite("drivingMode", &DataTCP::drivingMode)
		.def_readwrite("polePairs", &DataTCP::polePairs)
		.def_readwrite("phaseRmsVoltage", &DataTCP::phaseRmsVoltage)
		.def_readwrite("checkErrorCode", &DataTCP::checkErrorCode)
		.def_readwrite("temperature", &DataTCP::temperature)
		.def_readwrite("debugFloat0", &DataTCP::debugFloat0)
		.def_readwrite("debugFloat1", &DataTCP::debugFloat1)
		.def_readwrite("debugFloat2", &DataTCP::debugFloat2)
		.def_readwrite("debugFloat3", &DataTCP::debugFloat3)
		.def_readwrite("debugInt0", &DataTCP::debugInt0)
		.def_readwrite("debugInt1", &DataTCP::debugInt1)
		.def_readwrite("debugUint0", &DataTCP::debugUint0)
		.def_readwrite("debugUint1", &DataTCP::debugUint1)
	;

	py::class_<CommandTCP>(m, "CommandTCP")
		.def(py::init<>())
		.def_readwrite("motorIx", &CommandTCP::motorIx)
		.def_readwrite("torqueSetpoint", &CommandTCP::torqueSetpoint)
		.def_readwrite("torqueKp", &CommandTCP::torqueKp)
		.def_readwrite("torqueKd", &CommandTCP::torqueKd)
		.def_readwrite("torqueKi", &CommandTCP::torqueKi)
		.def_readwrite("torqueLimit", &CommandTCP::torqueLimit)
		.def_readwrite("velocitySetpoint", &CommandTCP::velocitySetpoint)
		.def_readwrite("velocityKp", &CommandTCP::velocityKp)
		.def_readwrite("velocityKd", &CommandTCP::velocityKd)
		.def_readwrite("velocityKi", &CommandTCP::velocityKi)
		.def_readwrite("velocityLimit", &CommandTCP::velocityLimit)
		.def_readwrite("positionSetpoint", &CommandTCP::positionSetpoint)
		.def_readwrite("positionKp", &CommandTCP::positionKp)
		.def_readwrite("positionKd", &CommandTCP::positionKd)
		.def_readwrite("positionKi", &CommandTCP::positionKi)
		.def_readwrite("drivingMode", &CommandTCP::drivingMode)
		.def_readwrite("currentLimitBus", &CommandTCP::currentLimitBus)
		.def_readwrite("currentLimitPhase", &CommandTCP::currentLimitPhase)
		.def_readwrite("polePairs", &CommandTCP::polePairs)
		.def_readwrite("udpDataConfig", &CommandTCP::udpDataConfig)
		.def_readwrite("confirmErrorCode", &CommandTCP::confirmErrorCode)
		.def_readwrite("ledStatus", &CommandTCP::ledStatus)
		.def_readwrite("debugFloat0", &CommandTCP::debugFloat0)
		.def_readwrite("debugFloat1", &CommandTCP::debugFloat1)
		.def_readwrite("debugFloat2", &CommandTCP::debugFloat2)
		.def_readwrite("debugFloat3", &CommandTCP::debugFloat3)
		.def_readwrite("debugInt0", &CommandTCP::debugInt0)
		.def_readwrite("debugInt1", &CommandTCP::debugInt1)
		.def_readwrite("debugUint0", &CommandTCP::debugUint0)
		.def_readwrite("debugUint1", &CommandTCP::debugUint1)
	;

	py::class_<Ethernet>(m, "Ethernet")
		.def(py::init<>())
		.def("setUDPCommand", &Ethernet::setUDPCommand, py::arg("data"))
		.def("getUDPData", &Ethernet::getUDPData)
		.def("packetsInQueueTCP", &Ethernet::packetsInQueueTCP)
		.def("addTCPPacket", &Ethernet::addTCPPacket, py::arg("packet"))
		.def("getTCPPacket", &Ethernet::getTCPPacket)
		.def_static("getTcpCommandPacket", &Ethernet::getTcpCommandPacket, py::arg("values"))
		.def_static("getTcpDataPacket", &Ethernet::getTcpDataPacket, py::arg("values"));
}