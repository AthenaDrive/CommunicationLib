
#ifndef COMPUTERCONTROLL_PROTOCOL_HPP
#define COMPUTERCONTROLL_PROTOCOL_HPP

#define PREFIX_IDENTIFIER 0b1111100001111001 << 16
#define PREFIX(len) (PREFIX_IDENTIFIER | len)

// UDP DATA
#define UDP_DATA_TIMESTAMP                    1 << 0
#define UDP_DATA_POSITION                     1 << 1
#define UDP_DATA_VELOCITY                     1 << 2
#define UDP_DATA_ACCELERATION                 1 << 3
#define UDP_DATA_TORQUE                       1 << 4
#define UDP_DATA_PHASE_CURRENT_A              1 << 5
#define UDP_DATA_PHASE_CURRENT_B              1 << 6
#define UDP_DATA_PHASE_CURRENT_C              1 << 7
#define UDP_DATA_BUS_CURRENT                  1 << 8
#define UDP_DATA_BUS_VOLTAGE                  1 << 9
#define UDP_DATA_ERROR_REGISTER               1 << 10
#define UDP_DATA_LOOP_TIME_FOC                1 << 11
#define UDP_DATA_LOOP_TIME_SECONDARY          1 << 12

// UDP COMMANDS
#define UDP_COMMAND_TORQUE_SETPOINT           1 << 0
#define UDP_COMMAND_VELOCITY_SETPOINT         1 << 1
#define UDP_COMMAND_POSITION_SETPOINT         1 << 2
#define UDP_COMMAND_MAX_TORQUE                1 << 3
#define UDP_COMMAND_MAX_VELOCITY              1 << 4
#define UDP_COMMAND_CURRENT_LIMIT_BUS         1 << 5
#define UDP_COMMAND_CURRENT_LIMIT_PHASE       1 << 6
#define UDP_COMMAND_DRIVING_MODE              1 << 7

// TCP DATA
#define TCP_DATA                              0 << 0
#define TCP_DATA_BUS_VOLTAGE                  1 << 1
#define TCP_DATA_BUS_CURRENT                  1 << 2
#define TCP_DATA_LED_STATUS                   1 << 3
#define TCP_DATA_BUTTON_STATUS                1 << 4
#define TCP_DATA_CURRENT_LIMIT_BUS            1 << 5
#define TCP_DATA_CURRENT_LIMIT_PHASE          1 << 6
#define TCP_DATA_BOARD_STATE                  1 << 7
#define TCP_DATA_DRIVING_MODE                 1 << 8
#define TCP_DATA_POLE_PAIRS                   1 << 9
#define TCP_DATA_PHASE_RMS_VOLTAGE            1 << 10
#define TCP_DATA_ERROR_REGISTER               1 << 11

// TCP COMMANDS
#define TCP_COMMAND                           1 << 0
#define TCP_COMMAND_TORQUE_SETPOINT           1 << 1
#define TCP_COMMAND_TORQUE_KP                 1 << 2
#define TCP_COMMAND_TORQUE_KD                 1 << 3
#define TCP_COMMAND_TORQUE_KI                 1 << 4
#define TCP_COMMAND_TORQUE_LIMIT              1 << 5
#define TCP_COMMAND_VELOCITY_SETPOINT         1 << 6
#define TCP_COMMAND_VELOCITY_KP               1 << 7
#define TCP_COMMAND_VELOCITY_KD               1 << 8
#define TCP_COMMAND_VELOCITY_KI               1 << 9
#define TCP_COMMAND_VELOCITY_LIMIT            1 << 10
#define TCP_COMMAND_POSITION_SETPOINT         1 << 11
#define TCP_COMMAND_POSITION_KP               1 << 12
#define TCP_COMMAND_POSITION_KD               1 << 13
#define TCP_COMMAND_POSITION_KI               1 << 14
#define TCP_COMMAND_DRIVING_MODE              1 << 15
#define TCP_COMMAND_CURRENT_LIMIT_BUS         1 << 16
#define TCP_COMMAND_CURRENT_LIMIT_PHASE       1 << 17
#define TCP_COMMAND_POLE_PAIRS                1 << 18
#define TCP_COMMAND_UDP_DATA_CONFIG           1 << 19
#define TCP_COMMAND_CLEAR_ERROR_REGISTER      1 << 20


#endif //COMPUTERCONTROLL_PROTOCOL_HPP