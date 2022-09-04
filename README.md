# E9X-M-CAN-Integration-Module
 
Unlike my previous modules, this one has access to PTCAN and KCAN. While the code is quite specific for my particular car, much of the CAN message logic may be useful.
Included are also tools to allow full manipulation of the program section in the MSD81 bin file.
MDrive settings now configurable from iDrive!


Hardware used:
 
* CANBED V1.2c http://docs.longan-labs.cc/1030008/ (32U4+MCP2515+MCP2551, LEDs removed) 
* Generic 16MHz MCP2515 CAN shield
* Toshiba K2889 MOSFET
* 1N4007 diode
* 150ohm SMD resistor
* OSRAM LO M676-Q2S1
* KINGBRIGHT KM2520ZGC-G03
* Micro USB right-angle cable
* Old PDC module PCB and housing



I use it to:

* Control MDrive settings from iDrive.
* Control Throttle map with the M button.
* Control DTC with the M button.
* Control EDC with the M button.
* Control Servotronic with the M button - through SVT70 module.
* Control the exhaust flap with the M button.
* Display Shiftlights - including startup animation, sync with the variable redline.
* Display Launch Control flag.
* Control Centre console buttons and associated LED (POWER, DSC OFF).
* Display Front fog lights on.
* Enable FXX KCAN1 CIC controllers.
* Turn on heated seats below a set temperature.
See program notes: [here](E9X-M-CAN-Integration-Module/program-notes.ino)


![settings](img/idrive-settings.jpg "idrive-settings")

![kombi-m](img/kombi-m.jpg "kombi-m")

![shiftlights](img/shiftlight.jpg "shiftlights")

![launchcontrol](img/launch-control/kombi.jpg "launchcontrol")

![fog](Fog/indicatoron.jpg "fog")

![case](img/case.jpg "case")

