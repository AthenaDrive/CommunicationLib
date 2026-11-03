from time import perf_counter, sleep
from math import sin, pi

import EthernetPython
import pg_widgets as pw

def main():
    eth = EthernetPython.Ethernet()
    controlManager = pw.ControlManager()
    t0 = perf_counter()

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [0b1111111111110])
    ])
    eth.addTCPPacket(packet)

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_DRIVING_MODE], [0b101])
    ])
    eth.addTCPPacket(packet)

    commandUDP = EthernetPython.CommandUDP()
    commandUDP.motorIx = 0
    commandUDP.torqueSetpoint = 0.0
    eth.setUDPCommand([commandUDP])

    maxPlotLenght = 500
    plotPos = pw.Plot((0.1, 0.0), (0.6, 0.15))
    plotVel = pw.Plot((0.1, 0.15), (0.6, 0.15))
    plotAcc = pw.Plot((0.1, 0.30), (0.6, 0.15))
    plotFOC = pw.Plot((0.1, 0.45), (0.6, 0.15))
    plotBusPower = pw.Plot((0.1, 0.60), (0.6, 0.15))
    plotCurrent = pw.Plot((0.1, 0.75), (0.6, 0.15))
    slide = pw.Slider((0.1, 0.9), (0.6, 0.10))
    slide.changeValues(-50.0, 50.0, 0.0)

    plotNames = pw.TextBoxes((0.0, 0.0), (0.1, 0.9), labels=["Pos", "Vel", "Acc", "FOC", "Bus Power", "Phase Current"])
    plotValues = pw.TextBoxes((0.7, 0.0), (0.3, 0.9), labels = ["Pos: ", "Vel: ", "Acc: ", "FOC: ", "Watts: ", "Phase A: "])

    torqueName = pw.TextBox((0.0, 0.9), (0.1, 0.1))
    torqueValue = pw.TextBox((0.7, 0.9), (0.3, 0.1))
    torqueName.setText("Torque: ")

    controlManager["plotPos"] = plotPos
    controlManager["plotVel"] = plotVel
    controlManager["plotAcc"] = plotAcc
    controlManager["plotFOC"] = plotFOC
    controlManager["plotBus"] = plotBusPower
    controlManager["plotCurrent"] = plotCurrent
    controlManager["slide"] = slide
    controlManager["plotNames"] = plotNames
    controlManager["plotValues"] = plotValues

    controlManager["torqueName"] = torqueName
    controlManager["torqueValue"] = torqueValue

    prevTCPAsk = t0
    while (controlManager.isRunning()):
        t = perf_counter() - t0

        dataUDPList = eth.getUDPData()
        if (len(dataUDPList) != 0):
            data = dataUDPList[0]

            if (data.position is not None):
                plotPos.addValue(t, data.position, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.position:.2f} rad", 0)

            if (data.velocity is not None):
                plotVel.addValue(t, data.velocity, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.velocity:.2f} rad/s", 1)

            if (data.acceleration is not None):
                plotAcc.addValue(t, data.acceleration, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.acceleration:.2f} rad/s^2", 2)

            if (data.loopTimeFOC is not None):
                plotFOC.addValue(t, data.loopTimeFOC, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.loopTimeFOC:.0f} us", 3)

            if (data.busVoltage is not None) and (data.busCurrent is not None):
                plotBusPower.addValue(t, data.busVoltage * data.busCurrent, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.busVoltage * data.busCurrent:.3f} W", 4)

                print(f"Voltage: {data.busVoltage} | Current: {data.busCurrent}")

            if (data.phaseCurrentA is not None) and (data.phaseCurrentB is not None) and (data.phaseCurrentC is not None):
                plotCurrent.addValue(t, data.phaseCurrentA, plotIx=0, maxLength=100)
                plotCurrent.addValue(t, data.phaseCurrentB, plotIx=1, maxLength=100)
                plotCurrent.addValue(t, data.phaseCurrentC, plotIx=2, maxLength=100)
                plotValues.setText(f"{data.phaseCurrentA:.0f} A", 5)

        dataTCPList = eth.getTCPPacket()
        for packet in dataTCPList:
            pass

        commandUDP.torqueSetpoint = slide.getValue()
        torqueValue.setText(f"{commandUDP.torqueSetpoint:.2f} xNm")
        eth.setUDPCommand([commandUDP])

        if (perf_counter() - prevTCPAsk) > 0.1:
            prevTCPAsk = perf_counter()
            packet = eth.getTcpDataPacket([
                ([EthernetPython.TCP_DATA_BUTTON_STATUS])
            ])
            eth.addTCPPacket(packet)

        # print(controlManager.getRenderTime())
        controlManager.update()

    print("Finishing.")


if __name__ == "__main__":
    main()