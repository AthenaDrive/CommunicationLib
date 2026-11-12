import struct
from time import perf_counter

import EthernetPython
import pg_widgets as pw


MAX_VAL_TUNING_ROWS = 10
I_TERM_MULT = 0.001
# Each row on the tuning tab: (row name, [(label, TCP command bit, min, max, initial value)])
TUNING_ROWS = [
    ("Torque", [
        ("Torque Kp", EthernetPython.TCP_COMMAND_TORQUE_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Torque Kd", EthernetPython.TCP_COMMAND_TORQUE_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Torque Ki", EthernetPython.TCP_COMMAND_TORQUE_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
        ("Torque limit", EthernetPython.TCP_COMMAND_TORQUE_LIMIT, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
    ]),
    ("Velocity", [
        ("Velocity Kp", EthernetPython.TCP_COMMAND_VELOCITY_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Velocity Kd", EthernetPython.TCP_COMMAND_VELOCITY_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Velocity Ki", EthernetPython.TCP_COMMAND_VELOCITY_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
        ("Velocity limit", EthernetPython.TCP_COMMAND_VELOCITY_LIMIT, 0.0, 100.0, 0.0),
    ]),
    ("Position", [
        ("Position Kp", EthernetPython.TCP_COMMAND_POSITION_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Position Kd", EthernetPython.TCP_COMMAND_POSITION_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Position Ki", EthernetPython.TCP_COMMAND_POSITION_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
    ]),
    ("Limits", [
        ("Bus current limit", EthernetPython.TCP_COMMAND_CURRENT_LIMIT_BUS, 0.0, 100.0, 0.0),
        ("Phase current limit", EthernetPython.TCP_COMMAND_CURRENT_LIMIT_PHASE, 0.0, 100.0, 0.0),
    ]),
]

MAXIMA_VAL_SETPOINTS = 30
# UDP setpoints on the tuning tab: (label, slider min, slider max, initial value)
UDP_SETPOINTS = [
    ("Torque setpoint", -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS, 0.0),
    ("Velocity setpoint", -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS, 0.0),
    ("Position setpoint", -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS, 0.0),
]

TUNING_TOP = 0.05
TUNING_BOTTOM = 0.85

INITIAL_DRIVING_MODE = 0b101


class TuningRow(pw.TuningSliders):
    """TuningSliders with larger labels and recovery from invalid text."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # The stock widget reserves only 10% of the row for each of the name
        # and value boxes, which makes them hard to read. Rebalance the row.
        count = len(self._labels)
        for i, label in enumerate(self._labels):
            posX = i / count
            width = 1.0 / count

            self[f"text{label}"]._pos = (posX, 0.0)
            self[f"text{label}"]._size = (width, 0.24)
            self[f"value{label}"]._pos = (posX, 0.24)
            self[f"value{label}"]._size = (width, 0.24)
            self[f"slider{label}"]._pos = (posX, 0.48)
            self[f"slider{label}"]._size = (width, 0.52)

    def update(self, mousePress, mousePos):
        for label in self._labels:
            valueBox = self[f"value{label}"]
            if valueBox._isSelected:
                continue
            try:
                float(valueBox.getText())
            except ValueError:
                valueBox.setText(f"{self[f'slider{label}'].getValue():.2f}")

        super().update(mousePress, mousePos)


def _floatToBits(value):
    """Convert a Python float to the raw uint32 sent over the wire."""
    return struct.unpack("<I", struct.pack("<f", float(value)))[0]


def main():
    eth = EthernetPython.Ethernet()
    controlManager = pw.ControlManager()
    t0 = perf_counter()

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [0b1111111111110])
    ])
    eth.addTCPPacket(packet)

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_DRIVING_MODE], [INITIAL_DRIVING_MODE])
    ])
    eth.addTCPPacket(packet)

    commandUDP = EthernetPython.CommandUDP()
    commandUDP.motorIx = 0
    commandUDP.torqueSetpoint = 0.0
    eth.setUDPCommand([commandUDP])

    # -----------------------------------------------------------------------
    # Plots tab
    # -----------------------------------------------------------------------
    dashboard = pw.UIGroup((0.0, 0.0), (1.0, 1.0))

    maxPlotLength = 500
    plotHeight = 0.145
    plotTop = 0.01

    plotPos = pw.Plot((0.1, plotTop + 0.0 * plotHeight), (0.6, plotHeight))
    plotVel = pw.Plot((0.1, plotTop + 1.0 * plotHeight), (0.6, plotHeight))
    plotAcc = pw.Plot((0.1, plotTop + 2.0 * plotHeight), (0.6, plotHeight))
    plotFOC = pw.Plot((0.1, plotTop + 3.0 * plotHeight), (0.6, plotHeight))
    plotBusPower = pw.Plot((0.1, plotTop + 4.0 * plotHeight), (0.6, plotHeight))
    plotCurrent = pw.Plot((0.1, plotTop + 5.0 * plotHeight), (0.6, plotHeight))
    slide = pw.Slider((0.1, 0.885), (0.6, 0.06))
    slide.changeValues(-50.0, 50.0, 0.0)

    plotNames = pw.TextBoxes((0.0, plotTop), (0.1, 6 * plotHeight),
        labels=["Pos", "Vel", "Acc", "FOC", "Bus Power", "Phase Current"])
    plotValues = pw.TextBoxes((0.7, plotTop), (0.3, 6 * plotHeight),
        labels=["Pos: ", "Vel: ", "Acc: ", "FOC: ", "Watts: ", "Phase A: "])

    torqueName = pw.TextBox((0.0, 0.885), (0.1, 0.06))
    torqueValue = pw.TextBox((0.7, 0.885), (0.3, 0.06))
    torqueName.setText("Torque: ")

    dashboard["plotPos"] = plotPos
    dashboard["plotVel"] = plotVel
    dashboard["plotAcc"] = plotAcc
    dashboard["plotFOC"] = plotFOC
    dashboard["plotBus"] = plotBusPower
    dashboard["plotCurrent"] = plotCurrent
    dashboard["slide"] = slide
    dashboard["plotNames"] = plotNames
    dashboard["plotValues"] = plotValues
    dashboard["torqueName"] = torqueName
    dashboard["torqueValue"] = torqueValue

    # -----------------------------------------------------------------------
    # Tuning tab
    # -----------------------------------------------------------------------
    tuning = pw.UIGroup((0.0, 0.0), (1.0, 1.0))

    statusBoard = pw.TextBox((0.0, 0.0), (0.40, 0.05))
    statusBoard.setText("Board state: -")
    statusBoard.disableSelection()

    statusMode = pw.TextBox((0.40, 0.0), (0.25, 0.05))
    statusMode.setText("Mode: -")
    statusMode.disableSelection()

    statusError = pw.TextBox((0.65, 0.0), (0.35, 0.05))
    statusError.setText("Error: -")
    statusError.disableSelection()

    tuning["statusBoard"] = statusBoard
    tuning["statusMode"] = statusMode
    tuning["statusError"] = statusError

    rowHeight = (TUNING_BOTTOM - TUNING_TOP) / (len(TUNING_ROWS) + 1)

    setpointRow = TuningRow((0.0, TUNING_TOP), (1.0, rowHeight),
        labels=[p[0] for p in UDP_SETPOINTS],
        lower_bounds=[p[1] for p in UDP_SETPOINTS],
        upper_bounds=[p[2] for p in UDP_SETPOINTS],
        current_values=[p[3] for p in UDP_SETPOINTS])

    tuning["rowSetpoints"] = setpointRow
    setpointSliders = {
        label: setpointRow[f"slider{label}"]
        for label, *_ in UDP_SETPOINTS
    }
    torqueSetpointLabel = UDP_SETPOINTS[0][0]
    velocitySetpointLabel = UDP_SETPOINTS[1][0]
    positionSetpointLabel = UDP_SETPOINTS[2][0]

    tuningRows = {}
    for i, (rowName, params) in enumerate(TUNING_ROWS):
        row = TuningRow((0.0, TUNING_TOP + (i + 1) * rowHeight), (1.0, rowHeight),
            labels=[p[0] for p in params],
            lower_bounds=[p[2] for p in params],
            upper_bounds=[p[3] for p in params],
            current_values=[p[4] for p in params])

        tuning[f"row{rowName}"] = row
        tuningRows[rowName] = (row, params)

    buttonApplyTuning = pw.Button((0.00, 0.86), (0.15, 0.08))
    buttonApplyTuning.setText("Apply tuning")

    buttonSendAllTuning = pw.Button((0.15, 0.86), (0.15, 0.08))
    buttonSendAllTuning.setText("Send all")

    buttonSendMode = pw.Button((0.30, 0.86), (0.15, 0.08))
    buttonSendMode.setText("Send mode")

    modeLabel = pw.TextBox((0.45, 0.86), (0.11, 0.08))
    modeLabel.setText("Driving mode:")
    modeLabel.disableSelection()

    modeInput = pw.TextBox((0.56, 0.86), (0.09, 0.08))
    modeInput.setText(f"{INITIAL_DRIVING_MODE:#b}")

    commandStatus = pw.TextBox((0.65, 0.86), (0.17, 0.08))
    commandStatus.setText("Set values, then apply")
    commandStatus.disableSelection()

    buttonClearErrors = pw.Button((0.82, 0.86), (0.18, 0.08))
    buttonClearErrors.setText("Clear errors")

    tuning["buttonApplyTuning"] = buttonApplyTuning
    tuning["buttonSendAllTuning"] = buttonSendAllTuning
    tuning["buttonSendMode"] = buttonSendMode
    tuning["modeLabel"] = modeLabel
    tuning["modeInput"] = modeInput
    tuning["commandStatus"] = commandStatus
    tuning["buttonClearErrors"] = buttonClearErrors

    lastErrorCode = 0
    lastAppliedTuning = {}

    for row, params in tuningRows.values():
        for label, bit, *_ in params:
            lastAppliedTuning[bit] = row[f"slider{label}"].getValue()

    def sendTuning(onlyChanged, _=None):
        headerBits = []
        values = []
        newValues = {}

        for row, params in tuningRows.values():
            for label, bit, *_ in params:
                value = row[f"slider{label}"].getValue()

                if onlyChanged and value == lastAppliedTuning[bit]:
                    continue

                headerBits.append(bit)
                values.append(_floatToBits(value))
                newValues[bit] = value

        if not headerBits:
            commandStatus.setText("No tuning changes")
            return

        packet = eth.getTcpCommandPacket([(headerBits, values)])
        eth.addTCPPacket(packet)
        lastAppliedTuning.update(newValues)

        if onlyChanged:
            commandStatus.setText(f"Sent {len(values)} tuning parameters")
        else:
            commandStatus.setText(f"Sent all {len(values)} tuning parameters")

    def sendDrivingMode(_=None):
        try:
            mode = int(modeInput.getText(), 0)
        except ValueError:
            commandStatus.setText("Invalid driving mode value")
            return

        if not 0 <= mode <= 0xFFFFFFFF:
            commandStatus.setText("Driving mode out of range")
            return

        packet = eth.getTcpCommandPacket([
            ([EthernetPython.TCP_COMMAND_DRIVING_MODE], [mode])
        ])
        eth.addTCPPacket(packet)
        commandStatus.setText(f"Sent driving mode {mode} ({mode:#b})")

    def clearErrors(_=None):
        packet = eth.getTcpCommandPacket([
            ([EthernetPython.TCP_COMMAND_CLEAR_ERROR_REGISTER], [lastErrorCode])
        ])
        eth.addTCPPacket(packet)
        commandStatus.setText(f"Cleared error code {lastErrorCode}")

    buttonApplyTuning.setFunction(lambda _: sendTuning(onlyChanged=True))
    buttonSendAllTuning.setFunction(lambda _: sendTuning(onlyChanged=False))
    buttonSendMode.setFunction(sendDrivingMode)
    buttonClearErrors.setFunction(clearErrors)

    # -----------------------------------------------------------------------
    # Tabs
    # -----------------------------------------------------------------------
    tabs = pw.Tab((0.0, 0.0), (1.0, 1.0), [dashboard, tuning], names=["Plots", "Tuning"])

    # The stock Tab uses 5%-wide buttons. Widen them so the labels stay legible.
    for i in range(2):
        button = tabs[f"button{i + 1}"]
        button._pos = (0.80 + 0.10 * i, 0.95)
        button._size = (0.10, 0.05)

    controlManager["tabs"] = tabs

    prevDashboardTorque = slide.getValue()
    prevTuningTorque = setpointSliders[torqueSetpointLabel].getValue()

    prevTCPAsk = t0
    while (controlManager.isRunning()):
        t = perf_counter() - t0

        dataUDPList = eth.getUDPData()
        if (len(dataUDPList) != 0):
            data = dataUDPList[0]

            if (data.position is not None):
                plotPos.addValue(t, data.position, maxLength=maxPlotLength)
                plotValues.setText(f"{data.position:.2f} rad", 0)

            if (data.velocity is not None):
                plotVel.addValue(t, data.velocity, maxLength=maxPlotLength)
                plotValues.setText(f"{data.velocity:.2f} rad/s", 1)

            if (data.acceleration is not None):
                plotAcc.addValue(t, data.acceleration, maxLength=maxPlotLength)
                plotValues.setText(f"{data.acceleration:.2f} rad/s^2", 2)

            if (data.loopTimeFOC is not None):
                plotFOC.addValue(t, data.loopTimeFOC, maxLength=maxPlotLength)
                plotValues.setText(f"{data.loopTimeFOC:.0f} us", 3)

            if (data.busVoltage is not None) and (data.busCurrent is not None):
                plotBusPower.addValue(t, data.busVoltage * data.busCurrent, maxLength=maxPlotLength)
                plotValues.setText(f"{data.busVoltage * data.busCurrent:.3f} W", 4)

            if (data.phaseCurrentA is not None) and (data.phaseCurrentB is not None) and (data.phaseCurrentC is not None):
                plotCurrent.addValue(t, data.phaseCurrentA, plotIx=0, maxLength=100)
                plotCurrent.addValue(t, data.phaseCurrentB, plotIx=1, maxLength=100)
                plotCurrent.addValue(t, data.phaseCurrentC, plotIx=2, maxLength=100)
                plotValues.setText(f"{data.phaseCurrentA:.2f} | {data.phaseCurrentB:.2f} | {data.phaseCurrentC:.2f}", 5)

        for packet in eth.getTCPPacket():
            if (packet.boardState is not None):
                statusBoard.setText(f"Board state: {packet.boardState}")

            if (packet.drivingMode is not None):
                statusMode.setText(f"Mode: {packet.drivingMode} ({packet.drivingMode:#b})")

            if (packet.checkErrorCode is not None):
                lastErrorCode = packet.checkErrorCode
                statusError.setText(f"Error: {packet.checkErrorCode}")

        # Keep the dashboard torque slider and the tuning-tab torque setpoint
        # slider in sync, whichever one the user is moving.
        dashboardTorque = slide.getValue()
        tuningTorque = setpointSliders[torqueSetpointLabel].getValue()

        if dashboardTorque != prevDashboardTorque:
            setpointSliders[torqueSetpointLabel].setValue(dashboardTorque)
            setpointRow[f"value{torqueSetpointLabel}"].setText(f"{dashboardTorque:.2f}")
            tuningTorque = dashboardTorque
        elif tuningTorque != prevTuningTorque:
            slide.setValue(tuningTorque)
            dashboardTorque = tuningTorque

        prevDashboardTorque = dashboardTorque
        prevTuningTorque = tuningTorque

        commandUDP.torqueSetpoint = dashboardTorque
        commandUDP.velocitySetpoint = setpointSliders[velocitySetpointLabel].getValue()
        commandUDP.positionSetpoint = setpointSliders[positionSetpointLabel].getValue()

        torqueValue.setText(f"{commandUDP.torqueSetpoint:.2f} xNm")
        eth.setUDPCommand([commandUDP])

        if (perf_counter() - prevTCPAsk) > 0.1:
            prevTCPAsk = perf_counter()
            packet = eth.getTcpDataPacket([[
                EthernetPython.TCP_DATA_BUTTON_STATUS,
                EthernetPython.TCP_DATA_BOARD_STATE,
                EthernetPython.TCP_DATA_DRIVING_MODE,
                EthernetPython.TCP_DATA_ERROR_REGISTER,
                EthernetPython.TCP_DATA_CURRENT_LIMIT_BUS,
                EthernetPython.TCP_DATA_CURRENT_LIMIT_PHASE,
            ]])
            eth.addTCPPacket(packet)

        controlManager.update()

    print("Finishing.")


if __name__ == "__main__":
    main()
