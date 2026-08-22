from time import perf_counter, sleep
from math import sin, pi

import EthernetPython
import pg_widgets as pw

def main():
    eth = EthernetPython.Ethernet()
    controlManager = pw.ControlManager()
    t0 = perf_counter()

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [0b111000000000000000000000000])
    ])
    eth.addTCPPacket(packet)

    commandUDP = EthernetPython.CommandUDP()
    commandUDP.motorIx = 0
    commandUDP.debugFloat0 = 0.0
    commandUDP.debugFloat3 = 0.0
    eth.setUDPCommand([commandUDP])

    offsetSlider = pw.Slider((0.8, 0.0), (0.1, 1.0))
    offsetSlider.changeValues(0.0, 6.28, 0.0)
    controlManager["offsetSlider"] = offsetSlider

    polePairSlider = pw.Slider((0.9, 0.0), (0.1, 1.0))
    polePairSlider.changeValues(-30.0, 30.0, 0.0)
    controlManager["polePairSlider"] = polePairSlider

    slider = pw.Slider((0.0, 0.0), (0.8, 0.33))
    slider.changeValues(-10.0, 10.0, 0.0)
    controlManager["slider"] = slider

    length = 500
    plotComputed = pw.Plot((0.0, 0.33), (0.8, 0.33))
    controlManager["plotComputed"] = plotComputed

    plotWanted = pw.Plot((0.0, 0.66), (0.8, 0.33))
    controlManager["plotWanted"] = plotWanted

    while (controlManager.isRunning()):
        t = perf_counter() - t0

        dataUDPList = eth.getUDPData()
        if (len(dataUDPList) != 0):
            data = dataUDPList[0]

            x = data.debugFloat0
            y = data.debugFloat1
            z = data.debugFloat2

            if (x and y and z):
                plotComputed.addValue(t, x, 0, length)
                plotComputed.addValue(t, y, 1, length)
                plotComputed.addValue(t, z, 2, length)

        v = slider.getValue()
        x = 50.0 + 2.0 * sin(v)
        y = 50.0 + 2.0 * sin(v + 2.0 * pi / 3.0)
        z = 50.0 + 2.0 * sin(v + 4.0 * pi / 3.0)
        plotWanted.addValue(t, x, 0, length)
        plotWanted.addValue(t, y, 1, length)
        plotWanted.addValue(t, z, 2, length)

        commandUDP.debugFloat0 = v
        commandUDP.debugFloat3 = offsetSlider.getValue()
        print(commandUDP.debugFloat0, commandUDP.debugFloat3)
        eth.setUDPCommand([commandUDP])

        # print(controlManager.getRenderTime())
        controlManager.update()

    print("Finishing.")


if __name__ == "__main__":
    main()