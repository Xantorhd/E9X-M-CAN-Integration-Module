# E9X-M-CAN-Integration-Module
 
Unlike my previous modules, this one has access to PTCAN and KCAN. While the code is quite specific for my particular car, much of the CAN message logic may be useful.
Included are also tools to allow full manipulation of the program section in the MSD81 bin file.
MDrive settings now configurable from iDrive!


Hardware used:
 
* Skpang Triple CAN board https://copperhilltech.com/teensy-4-0-triple-can-bus-board-with-two-can-2-0b-and-one-can-fd-port/ (Teensy 4.0, MCP2562s).
* Small copper heatsink.
* N-channel MOSFET that can be driven with 3V3. I had some FQP50N06Ls.
* 1N4007 diode (exhaust solenoid flyback).
* 10K Ohm (pull-down), 600 Ohm (R-gate) and 10 Ohm (Fog LED) resistor.
* OSRAM LO M676-Q2S1 FOG button illumination LED.
* KINGBRIGHT KM2520ZGC-G03 FOG indicator LED.
* Micro USB right-angle cable.
* CR2450 620mAh battery. With a ~0.025 mA draw, the RTC should last around 3 years unpowered (terminal 30G OFF).
	-> https://www.nxp.com/docs/en/nxp/data-sheets/IMXRT1060CEC.pdf P.28 (SNVSS) + P.32 (RTC-OSC).
* Old PDC module PCB and housing. It has nice autmotive connectors.



I use it to:

* Control MDrive settings from iDrive.
	* Control DME throttle map with the M button.
	* Control DTC/DSC OFF with the M button.
	* Control EDC mode with the M button.
	* Control Servotronic mode with the M button - through SVT70 module.
	* Control the exhaust flap position with the M button.
* Display Shiftlights - including startup animation, sync with the M3 KOMBI variable redline.
* Display Launch Control flag.
* Control Centre console buttons and associated LED (POWER, DSC OFF).
* Display Front fog lights ON (for M3 clusters that lack the symbol).
* Enable FXX KCAN1 CIC controllers.
* Turn on heated seats below a set temperature.
* Keep time and date in RTC and set back if KOMBI is reset (30G_F, battery removed/flat, coding, etc.).
See program notes: [here](program-notes.txt)


![settings](img/idrive-settings.jpg "idrive-settings")

![kombi-m](img/kombi-m.jpg "kombi-m")

![shiftlights](img/shiftlight.jpg "shiftlights")

![launchcontrol](img/launch-control/kombi.jpg "launchcontrol")

![fog](img/Fog/indicatoron.jpg "fog")

![case](img/case.jpg "case")

![board](img/board/board-anotated.jpg "board")

