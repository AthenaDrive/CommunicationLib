
#ifndef COMPUTERCONTROLL_PROTOCOL_HPP
#define COMPUTERCONTROLL_PROTOCOL_HPP

#define PREFIX_IDENTIFIER 0b1111100001111001u << 16
#define PREFIX(len) (PREFIX_IDENTIFIER | len)

#define DEBUG_FLOAT_0                         1u << 24
#define DEBUG_FLOAT_1                         1u << 25
#define DEBUG_FLOAT_2                         1u << 26
#define DEBUG_FLOAT_3                         1u << 27
#define DEBUG_INT_0                           1u << 28
#define DEBUG_INT_1                           1u << 29
#define DEBUG_UINT_0                          1u << 30
#define DEBUG_UINT_1                          1u << 31

// UDP DATA
#define UDP_DATA_TIMESTAMP                    1u << 0
#define UDP_DATA_POSITION                     1u << 1
#define UDP_DATA_VELOCITY                     1u << 2
#define UDP_DATA_ACCELERATION                 1u << 3
#define UDP_DATA_TORQUE                       1u << 4
#define UDP_DATA_PHASE_CURRENT_A              1u << 5
#define UDP_DATA_PHASE_CURRENT_B              1u << 6
#define UDP_DATA_PHASE_CURRENT_C              1u << 7
#define UDP_DATA_BUS_CURRENT                  1u << 8
#define UDP_DATA_BUS_VOLTAGE                  1u << 9
#define UDP_DATA_ERROR_REGISTER               1u << 10
#define UDP_DATA_LOOP_TIME_FOC                1u << 11
#define UDP_DATA_LOOP_TIME_SECONDARY          1u << 12
#define UDP_DATA_ACCELERATION_X               1u << 13
#define UDP_DATA_ACCELERATION_Y               1u << 14
#define UDP_DATA_ACCELERATION_Z               1u << 15
#define UDP_DATA_GYRO_X                       1u << 16
#define UDP_DATA_GYRO_Y                       1u << 17
#define UDP_DATA_GYRO_Z                       1u << 18

struct DataUDP {
	int32_t motorIx = -1;

	std::optional<uint32_t> timestamp;
	std::optional<float> position;
	std::optional<float> velocity;
	std::optional<float> acceleration;
	std::optional<float> torque;
	std::optional<float> phaseCurrentA;
	std::optional<float> phaseCurrentB;
	std::optional<float> phaseCurrentC;
	std::optional<float> busCurrent;
	std::optional<float> busVoltage;

	std::optional<uint32_t> errorRegister;
	std::optional<uint32_t> loopTimeFOC;
	std::optional<uint32_t> loopTimeSecondary;

	std::optional<float> accX;
	std::optional<float> accY;
	std::optional<float> accZ;
	std::optional<float> gyroX;
	std::optional<float> gyroY;
	std::optional<float> gyroZ;

	std::optional<float> debugFloat0;
	std::optional<float> debugFloat1;
	std::optional<float> debugFloat2;
	std::optional<float> debugFloat3;
	std::optional<int32_t> debugInt0;
	std::optional<int32_t> debugInt1;
	std::optional<uint32_t> debugUint0;
	std::optional<uint32_t> debugUint1;
};

// UDP COMMANDS
#define UDP_COMMAND_TORQUE_SETPOINT           1u << 0
#define UDP_COMMAND_VELOCITY_SETPOINT         1u << 1
#define UDP_COMMAND_POSITION_SETPOINT         1u << 2
#define UDP_COMMAND_MAX_TORQUE                1u << 3
#define UDP_COMMAND_MAX_VELOCITY              1u << 4
#define UDP_COMMAND_CURRENT_LIMIT_BUS         1u << 5
#define UDP_COMMAND_CURRENT_LIMIT_PHASE       1u << 6
#define UDP_COMMAND_DRIVING_MODE              1u << 7

struct CommandUDP {
	int32_t motorIx = -1;

	std::optional<float> torqueSetpoint;
	std::optional<float> velocitySetpoint;
	std::optional<float> positionSetpoint;

	std::optional<uint32_t> drivingMode;

	std::optional<float> maxTorque;
	std::optional<float> maxVelocity;
	std::optional<float> currentLimitBus;
	std::optional<float> currentLimitPhase;

	std::optional<float> debugFloat0;
	std::optional<float> debugFloat1;
	std::optional<float> debugFloat2;
	std::optional<float> debugFloat3;
	std::optional<int32_t> debugInt0;
	std::optional<int32_t> debugInt1;
	std::optional<uint32_t> debugUint0;
	std::optional<uint32_t> debugUint1;
};

// TCP DATA
#define TCP_DATA                              0u << 0
#define TCP_DATA_BUS_VOLTAGE                  1u << 1
#define TCP_DATA_BUS_CURRENT                  1u << 2

#define TCP_DATA_BUTTON_STATUS                1u << 4
#define TCP_DATA_CURRENT_LIMIT_BUS            1u << 5
#define TCP_DATA_CURRENT_LIMIT_PHASE          1u << 6
#define TCP_DATA_BOARD_STATE                  1u << 7
#define TCP_DATA_DRIVING_MODE                 1u << 8
#define TCP_DATA_POLE_PAIRS                   1u << 9
#define TCP_DATA_PHASE_RMS_VOLTAGE            1u << 10
#define TCP_DATA_ERROR_REGISTER               1u << 11
#define TCP_DATA_TEMPERATURE                  1u << 12

struct DataTCP {
	int32_t motorIx = -1;

	std::optional<float> busVoltage;
	std::optional<float> busCurrent;

	std::optional<uint32_t> buttonStatus;
	std::optional<float> currentLimitBus;
	std::optional<float> currentLimitPhase;
	std::optional<uint32_t> boardState;
	std::optional<uint32_t> drivingMode;
	std::optional<uint32_t> polePairs;
	std::optional<float> phaseRmsVoltage;
	std::optional<uint32_t> checkErrorCode;
	std::optional<float> temperature;

	std::optional<float> debugFloat0;
	std::optional<float> debugFloat1;
	std::optional<float> debugFloat2;
	std::optional<float> debugFloat3;
	std::optional<int32_t> debugInt0;
	std::optional<int32_t> debugInt1;
	std::optional<uint32_t> debugUint0;
	std::optional<uint32_t> debugUint1;
};

// TCP COMMANDS
#define TCP_COMMAND                           1u << 0
#define TCP_COMMAND_TORQUE_SETPOINT           1u << 1
#define TCP_COMMAND_TORQUE_KP                 1u << 2
#define TCP_COMMAND_TORQUE_KD                 1u << 3
#define TCP_COMMAND_TORQUE_KI                 1u << 4
#define TCP_COMMAND_TORQUE_LIMIT              1u << 5
#define TCP_COMMAND_VELOCITY_SETPOINT         1u << 6
#define TCP_COMMAND_VELOCITY_KP               1u << 7
#define TCP_COMMAND_VELOCITY_KD               1u << 8
#define TCP_COMMAND_VELOCITY_KI               1u << 9
#define TCP_COMMAND_VELOCITY_LIMIT            1u << 10
#define TCP_COMMAND_POSITION_SETPOINT         1u << 11
#define TCP_COMMAND_POSITION_KP               1u << 12
#define TCP_COMMAND_POSITION_KD               1u << 13
#define TCP_COMMAND_POSITION_KI               1u << 14
#define TCP_COMMAND_DRIVING_MODE              1u << 15
#define TCP_COMMAND_CURRENT_LIMIT_BUS         1u << 16
#define TCP_COMMAND_CURRENT_LIMIT_PHASE       1u << 17
#define TCP_COMMAND_POLE_PAIRS                1u << 18
#define TCP_COMMAND_UDP_DATA_CONFIG           1u << 19
#define TCP_COMMAND_CLEAR_ERROR_REGISTER      1u << 20
#define TCP_COMMAND_LED_STATUS                1u << 21

struct CommandTCP {
	int32_t motorIx = -1;
	std::optional<float> torqueSetpoint;
	std::optional<float> torqueKp;
	std::optional<float> torqueKd;
	std::optional<float> torqueKi;
	std::optional<float> torqueLimit;

	std::optional<float> velocitySetpoint;
	std::optional<float> velocityKp;
	std::optional<float> velocityKd;
	std::optional<float> velocityKi;
	std::optional<float> velocityLimit;

	std::optional<float> positionSetpoint;
	std::optional<float> positionKp;
	std::optional<float> positionKd;
	std::optional<float> positionKi;

	std::optional<uint32_t> drivingMode;

	std::optional<float> currentLimitBus;
	std::optional<float> currentLimitPhase;

	std::optional<uint32_t> polePairs;
	std::optional<uint32_t> udpDataConfig;
	std::optional<uint32_t> confirmErrorCode;
	std::optional<uint32_t> ledStatus;

	std::optional<float> debugFloat0;
	std::optional<float> debugFloat1;
	std::optional<float> debugFloat2;
	std::optional<float> debugFloat3;
	std::optional<int32_t> debugInt0;
	std::optional<int32_t> debugInt1;
	std::optional<uint32_t> debugUint0;
	std::optional<uint32_t> debugUint1;
};

#endif //COMPUTERCONTROLL_PROTOCOL_HPP