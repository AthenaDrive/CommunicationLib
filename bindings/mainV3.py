"""AthenaDrive high-rate UDP sample monitor (console only).

No pygame, no pg_widgets, no rendering. The loop polls the Ethernet
backend as fast as Python allows so the GUI refresh rate can no longer
limit how many UDP telemetry frames are observed.

It prints one throughput line per second and a telemetry snapshot every
DETAIL_INTERVAL seconds. Ctrl+C stops the monitor and prints a summary.

By default this is a passive monitor: it sends no UDP setpoints and does
not change the driving mode. It only requests the telemetry fields listed
in UDP_DATA_CONFIG.

It uses the same UDP port as main.py and mainV2.py, so run only one of
them at a time.

Run from the bindings folder (next to EthernetPython.pyd):

    ..\\venv\\Scripts\\python.exe mainV3.py
"""

import math
import time
from time import perf_counter

import EthernetPython


# Everything the monitor wants the board to send.
UDP_DATA_CONFIG = (
    EthernetPython.UDP_DATA_TIMESTAMP
    | EthernetPython.UDP_DATA_POSITION
    | EthernetPython.UDP_DATA_VELOCITY
    | EthernetPython.UDP_DATA_ACCELERATION
    | EthernetPython.UDP_DATA_TORQUE
    | EthernetPython.UDP_DATA_PHASE_CURRENT_A
    | EthernetPython.UDP_DATA_PHASE_CURRENT_B
    | EthernetPython.UDP_DATA_PHASE_CURRENT_C
    | EthernetPython.UDP_DATA_BUS_CURRENT
    | EthernetPython.UDP_DATA_BUS_VOLTAGE
    | EthernetPython.UDP_DATA_ERROR_REGISTER
    | EthernetPython.UDP_DATA_LOOP_TIME_FOC
    | EthernetPython.UDP_DATA_LOOP_TIME_SECONDARY
    | EthernetPython.DEBUG_FLOAT_0
    | EthernetPython.DEBUG_FLOAT_1
    | EthernetPython.DEBUG_FLOAT_2
    | EthernetPython.DEBUG_FLOAT_3
)

REPORT_INTERVAL = 1.0        # seconds between throughput lines
DETAIL_INTERVAL = 5.0        # seconds between telemetry snapshots (0 = off)
TCP_POLL_INTERVAL = 0.1      # seconds between TCP status requests
TCP_QUEUE_LIMIT = 8          # stop queueing requests if TCP is offline

# Set to True if the board needs one UDP command before it starts
# streaming, or if you want a safe zero command. It is sent only once.
SEND_NEUTRAL_UDP = False


def isFinite(value):
    return value is not None and math.isfinite(value)


def formatValue(value, unit="", digits=2):
    if not isFinite(value):
        return "-"
    return f"{value:.{digits}f}{unit}"


def formatCompact(value, digits=2):
    if not isFinite(value):
        return "-"
    if value == 0:
        return "0"
    magnitude = abs(value)
    if 1e-3 <= magnitude < 1e5:
        return f"{value:.{digits}f}"
    return f"{value:.{digits}e}"


def udpSampleKey(data):
    """Payload identity used when the board does not provide timestamps."""
    return (
        data.timestamp,
        data.position,
        data.velocity,
        data.acceleration,
        data.torque,
        data.phaseCurrentA,
        data.phaseCurrentB,
        data.phaseCurrentC,
        data.busCurrent,
        data.busVoltage,
        data.errorRegister,
        data.loopTimeFOC,
        data.loopTimeSecondary,
        data.debugFloat0,
        data.debugFloat1,
        data.debugFloat2,
        data.debugFloat3,
    )


def main():
    print("=" * 88)
    print("AthenaDrive UDP sample-rate monitor  (console only)")
    print("No rendering. No GUI. Polling is CPU-bound to avoid missing frames.")
    print(f"UDP telemetry config: 0x{UDP_DATA_CONFIG:08X}")
    if SEND_NEUTRAL_UDP:
        print("UDP setpoints: one neutral command will be sent.")
    else:
        print("UDP setpoints: disabled (passive monitor).")
    print("Press Ctrl+C to stop and print a summary.")
    print("=" * 88, flush=True)

    try:
        eth = EthernetPython.Ethernet()
    except Exception as error:
        print(f"Could not start the Ethernet backend: {error}")
        return

    try:
        configPacket = eth.getTcpCommandPacket([
            ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [UDP_DATA_CONFIG])
        ])
        eth.addTCPPacket(configPacket)
    except Exception as error:
        print(f"Could not queue the UDP telemetry config: {error}")
        return

    if SEND_NEUTRAL_UDP:
        try:
            command = EthernetPython.CommandUDP()
            command.motorIx = 0
            command.torqueSetpoint = 0.0
            command.velocitySetpoint = 0.0
            command.positionSetpoint = 0.0
            eth.setUDPCommand([command])
            print("Neutral UDP command queued (zero torque/velocity/position).",
                  flush=True)
        except Exception as error:
            print(f"Could not queue the neutral UDP command: {error}", flush=True)

    print("Telemetry config queued. Waiting for UDP samples...", flush=True)

    start = perf_counter()

    lastDataAt = None
    statsStart = None
    lastTimestamp = None
    lastKey = None
    timestampSupported = False
    noTimestampWarned = False
    motorCount = 0

    lastPosition = None
    lastVelocity = None
    lastAcceleration = None
    lastTorque = None
    lastBusVoltage = None
    lastBusCurrent = None
    lastPhaseA = None
    lastPhaseB = None
    lastPhaseC = None
    lastLoopFOC = None
    lastLoopSecondary = None
    lastDebug = [None, None, None, None]
    lastBoardState = None
    lastDrivingMode = None
    lastErrorCode = None
    lastTemperature = None

    totalFrames = 0
    totalSamples = 0
    intervalFrames = 0
    intervalSamples = 0
    intervalPolls = 0
    pollCount = 0
    rates = []

    lastReport = start
    lastDetail = start
    lastTcpPoll = start

    try:
        while True:
            now = perf_counter()
            pollCount += 1
            intervalPolls += 1

            # ---------------------------------------------------------------
            # UDP: get latest cached data and detect a genuinely new frame.
            # ---------------------------------------------------------------
            dataList = eth.getUDPData()
            if dataList:
                data = dataList[0]
                motorCount = len(dataList)
                key = udpSampleKey(data)
                timestamp = data.timestamp
                timestampKey = timestamp if timestamp not in (None, 0) else None

                if timestampKey is not None:
                    timestampSupported = True
                    isNew = (timestampKey != lastTimestamp) or (key != lastKey)
                    lastTimestamp = timestampKey
                else:
                    isNew = key != lastKey

                if isNew:
                    lastKey = key
                    if statsStart is None:
                        statsStart = now
                    lastDataAt = now

                    totalFrames += 1
                    totalSamples += len(dataList)
                    intervalFrames += 1
                    intervalSamples += len(dataList)

                    if isFinite(data.position):
                        lastPosition = data.position
                    if isFinite(data.velocity):
                        lastVelocity = data.velocity
                    if isFinite(data.acceleration):
                        lastAcceleration = data.acceleration
                    if isFinite(data.torque):
                        lastTorque = data.torque
                    if isFinite(data.busVoltage):
                        lastBusVoltage = data.busVoltage
                    if isFinite(data.busCurrent):
                        lastBusCurrent = data.busCurrent
                    if isFinite(data.phaseCurrentA):
                        lastPhaseA = data.phaseCurrentA
                    if isFinite(data.phaseCurrentB):
                        lastPhaseB = data.phaseCurrentB
                    if isFinite(data.phaseCurrentC):
                        lastPhaseC = data.phaseCurrentC
                    if isFinite(data.loopTimeFOC):
                        lastLoopFOC = data.loopTimeFOC
                    if isFinite(data.loopTimeSecondary):
                        lastLoopSecondary = data.loopTimeSecondary
                    if data.errorRegister is not None:
                        lastErrorCode = data.errorRegister

                    for index, debugValue in enumerate((
                            data.debugFloat0, data.debugFloat1,
                            data.debugFloat2, data.debugFloat3)):
                        if isFinite(debugValue):
                            lastDebug[index] = debugValue

                    if (
                        (not timestampSupported)
                        and (not noTimestampWarned)
                        and (statsStart is not None)
                        and ((now - statsStart) > 2.0)
                    ):
                        print("[note] No UDP timestamp received; duplicate "
                              "detection falls back to payload equality.",
                              flush=True)
                        noTimestampWarned = True

            # ---------------------------------------------------------------
            # TCP: board state / mode / error / temperature for the details.
            # ---------------------------------------------------------------
            if (now - lastTcpPoll) >= TCP_POLL_INTERVAL:
                lastTcpPoll = now
                if eth.packetsInQueueTCP() < TCP_QUEUE_LIMIT:
                    try:
                        packet = eth.getTcpDataPacket([[
                            EthernetPython.TCP_DATA_BOARD_STATE,
                            EthernetPython.TCP_DATA_DRIVING_MODE,
                            EthernetPython.TCP_DATA_ERROR_REGISTER,
                            EthernetPython.TCP_DATA_TEMPERATURE,
                        ]])
                        eth.addTCPPacket(packet)
                    except Exception:
                        pass

                try:
                    for packet in eth.getTCPPacket():
                        if packet.boardState is not None:
                            lastBoardState = packet.boardState
                        if packet.drivingMode is not None:
                            lastDrivingMode = packet.drivingMode
                        if packet.checkErrorCode is not None:
                            lastErrorCode = packet.checkErrorCode
                        if isFinite(packet.temperature):
                            lastTemperature = packet.temperature
                except Exception:
                    pass

            # ---------------------------------------------------------------
            # Throughput report, once per REPORT_INTERVAL.
            # ---------------------------------------------------------------
            if (now - lastReport) >= REPORT_INTERVAL:
                elapsed = max(1e-9, now - lastReport)
                frameRate = intervalFrames / elapsed
                sampleRate = intervalSamples / elapsed
                pollRate = intervalPolls / elapsed

                if statsStart is not None:
                    rates.append(frameRate)
                    dataElapsed = max(1e-9, now - statsStart)
                    averageRate = totalFrames / dataElapsed
                else:
                    dataElapsed = 0.0
                    averageRate = 0.0

                minimumRate = min(rates) if rates else 0.0
                maximumRate = max(rates) if rates else 0.0

                if lastDataAt is None:
                    ageText = "--"
                    linkText = "WAITING" if (now - start) < 2.0 else "NO DATA"
                else:
                    age = max(0.0, now - lastDataAt)
                    ageText = f"{age * 1000.0:.1f} ms"
                    if age < 0.25:
                        linkText = "ONLINE"
                    elif age < 1.0:
                        linkText = "STALE"
                    else:
                        linkText = "OFFLINE"

                timestampText = "yes" if timestampSupported else "no"
                motorText = ""
                if motorCount > 1:
                    motorText = f" motors={motorCount}"

                print(
                    f"[{time.strftime('%H:%M:%S')}] "
                    f"UDP {frameRate:7.1f} frames/s | "
                    f"samples {sampleRate:7.1f}/s | "
                    f"avg {averageRate:7.1f} | "
                    f"min {minimumRate:7.1f} | "
                    f"max {maximumRate:7.1f} | "
                    f"poll {pollRate:,.0f}/s | "
                    f"age {ageText} | "
                    f"total {totalFrames:,}{motorText} | "
                    f"ts={timestampText} | {linkText}",
                    flush=True)

                lastReport = now
                intervalFrames = 0
                intervalSamples = 0
                intervalPolls = 0

            # ---------------------------------------------------------------
            # Detailed telemetry snapshot, every DETAIL_INTERVAL seconds.
            # ---------------------------------------------------------------
            if (DETAIL_INTERVAL > 0.0) and ((now - lastDetail) >= DETAIL_INTERVAL):
                lastDetail = now

                if isFinite(lastBusVoltage) and isFinite(lastBusCurrent):
                    busPower = lastBusVoltage * lastBusCurrent
                else:
                    busPower = None

                boardText = "-" if lastBoardState is None else str(lastBoardState)
                modeText = "-" if lastDrivingMode is None else f"{lastDrivingMode:#b}"
                if lastErrorCode is None:
                    errorText = "-"
                elif lastErrorCode == 0:
                    errorText = "CLEAR"
                else:
                    errorText = f"0x{lastErrorCode:04X}"

                print(
                    f"    pos {formatValue(lastPosition, ' rad', 3)} | "
                    f"vel {formatValue(lastVelocity, ' rad/s', 3)} | "
                    f"acc {formatValue(lastAcceleration, ' rad/s2', 2)} | "
                    f"torque {formatValue(lastTorque, ' Nm', 3)} | "
                    f"bus {formatValue(lastBusVoltage, ' V', 2)} "
                    f"{formatValue(lastBusCurrent, ' A', 2)} "
                    f"{formatValue(busPower, ' W', 1)} | "
                    f"temp {formatValue(lastTemperature, ' C', 1)}",
                    flush=True)

                print(
                    f"    board {boardText} | mode {modeText} | error {errorText} | "
                    f"foc {formatValue(lastLoopFOC, ' us', 0)} | "
                    f"sec {formatValue(lastLoopSecondary, ' us', 0)} | "
                    f"phase {formatValue(lastPhaseA, '', 2)}/"
                    f"{formatValue(lastPhaseB, '', 2)}/"
                    f"{formatValue(lastPhaseC, '', 2)} A | "
                    f"debug {formatCompact(lastDebug[0])}/"
                    f"{formatCompact(lastDebug[1])}/"
                    f"{formatCompact(lastDebug[2])}/"
                    f"{formatCompact(lastDebug[3])} | "
                    f"tcpq {eth.packetsInQueueTCP()}",
                    flush=True)

    except KeyboardInterrupt:
        print("\nStopping on Ctrl+C...", flush=True)
    finally:
        end = perf_counter()
        elapsed = max(1e-9, end - start)

        print()
        print("-" * 88)
        print(f"Session time : {elapsed:,.2f} s")
        print(f"Polls        : {pollCount:,}  ({pollCount / elapsed:,.0f} /s)")
        print(f"UDP frames   : {totalFrames:,}  ({totalFrames / elapsed:,.2f} /s)")
        print(f"UDP samples  : {totalSamples:,}  ({totalSamples / elapsed:,.2f} /s)")

        if statsStart is not None:
            dataElapsed = max(1e-9, end - statsStart)
            print(f"Data window  : {dataElapsed:,.2f} s")
            print(f"During data  : {totalFrames / dataElapsed:,.2f} frames/s, "
                  f"{totalSamples / dataElapsed:,.2f} samples/s")

        if rates:
            print(f"1 s rates    : min {min(rates):.2f} / "
                  f"avg {sum(rates) / len(rates):.2f} / "
                  f"max {max(rates):.2f} frames/s")

        print(f"Timestamps   : {'yes' if timestampSupported else 'no'}")
        print("-" * 88, flush=True)


if __name__ == "__main__":
    main()
