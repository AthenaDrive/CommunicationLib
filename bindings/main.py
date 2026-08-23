from time import perf_counter, sleep
from math import sin, pi

import EthernetPython
import pg_widgets as pw

def main():
    eth = EthernetPython.Ethernet()
    controlManager = pw.ControlManager()
    t0 = perf_counter()

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [0b1111100011110])
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
    plotSec = pw.Plot((0.1, 0.60), (0.6, 0.15))
    slide = pw.Slider((0.1, 0.75), (0.6, 0.15))
    slide.changeValues(-10.0, 10.0, 0.0)

    plotNames = pw.TextBoxes((0.0, 0.0), (0.1, 0.9), labels=["Pos", "Vel", "Acc", "FOC", "Sec", "Torque"])
    plotValues = pw.TextBoxes((0.7, 0.0), (0.3, 0.9), labels = ["Pos: ", "Vel: ", "Acc: ", "FOC: ", "Sec: ", "Torque: "])

    prevValue = 0
    ledValue = 0
    led0 = pw.ToggleButton((0.0, 0.9), (0.2, 0.1))
    led1 = pw.ToggleButton((0.2, 0.9), (0.2, 0.1))
    led2 = pw.ToggleButton((0.4, 0.9), (0.2, 0.1))

    light1 = pw.ToggleButton((0.8, 0.9), (0.1, 0.1))
    light2 = pw.ToggleButton((0.9, 0.9), (0.1, 0.1))

    controlManager["plotPos"] = plotPos
    controlManager["plotVel"] = plotVel
    controlManager["plotAcc"] = plotAcc
    controlManager["plotFOC"] = plotFOC
    controlManager["plotSec"] = plotSec
    controlManager["slide"] = slide
    controlManager["plotNames"] = plotNames
    controlManager["plotValues"] = plotValues
    controlManager["led0"] = led0
    controlManager["led1"] = led1
    controlManager["led2"] = led2
    controlManager["light1"] = light1
    controlManager["light2"] = light2

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

            if (data.loopTimeSecondary is not None):
                plotSec.addValue(t, data.loopTimeSecondary, maxLength=maxPlotLenght)
                plotValues.setText(f"{data.loopTimeSecondary:.0f} us", 4)

        dataTCPList = eth.getTCPPacket()
        for packet in dataTCPList:

            if packet.buttonStatus is not None:
                b = packet.buttonStatus
                light1.setValue(b & 1)
                light2.setValue(b & 2)


        commandUDP.torqueSetpoint = slide.getValue()
        plotValues.setText(f"{commandUDP.torqueSetpoint:.2f} xNm", 5)
        eth.setUDPCommand([commandUDP])

        ledValue = led0.getValue() * 4 + led1.getValue() * 2 + led2.getValue() * 1
        if (ledValue != prevValue):
            packet = eth.getTcpCommandPacket([
                ([EthernetPython.TCP_COMMAND_LED_STATUS], [ledValue])
            ])
            eth.addTCPPacket(packet)
            prevValue = ledValue

        if (perf_counter() - prevTCPAsk) > 0.1:
            prevTCPAsk = perf_counter()
            packet = eth.getTcpDataPacket([
                ([EthernetPython.TCP_DATA_BUTTON_STATUS])
            ])
            eth.addTCPPacket(packet)

        print(controlManager.getRenderTime())
        controlManager.update()

    print("Finishing.")


if __name__ == "__main__":
    main()