# Space API Control Panel
Arduino project for the control panel of the lab status.

## Evolutions (recent to old)
### 2024-05-01
This is currently a box in the entrance of the lab that contains two galvos as a display of how much time the lab is open and how many people are present, with old-school industrial levers to change those values and a few LEDs because LEDs are always nice.

It is running on an Arduino Uno with an Ethernet Shield and a PoE module since 2016.

A big clean up of the cabling has been done since the last major revision of 2014, and no picture of the previous cablng will be shown here for decency.

### 2016-01-??
As of January 2016, Ethernet shield support has been added to the Arduino sketch. This allow the arduino to be autonomous and do HTTP request on it's own. A simple POST request is used for updating the status.
For the server backend, see  *PTL-Status-API* project.

### 2014-??-??
The PTL Control Pannel Ardiono is connected by USB to a linux computer.
A python script retrieves data (serial) from the Arduino and sends a POST request to the PTL Status API on a remote server.

# Build steps
Prepare the `config.h` file by copying `config.example.h`, and edit the API key.
Ask your favorite sysadmin if you don't currently have access to it.

## arduino-cli
### Dependencies
Install the `Arduino-Bounce` library:
```
# Required because git lib installation is apparently "unsafe"
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/mpflaga/Arduino-Bounce.git
arduino-cli lib install Ethernet
``` 

### Build and flash
You can list the available serial ports on your computer to see which one is right with:
```
arduino-cli  board list
```

Then to compile and flash (update the port):
```
arduino-cli compile --fqbn arduino:avr:uno
# Replace the port by the corresponding one on your computer
arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyACM0
```

## Arduino IDE 2
### Dependencies
This project is using the `Arduino-Bounce` library which is a bit old, so it is not present in the library manager of Arduino. The easiest solution is to download and install it manually.

- In the IDE, go to File -> Preferences, retrieve the `Sketchbook location` path and navigate there.
- Go into the `libraries` folder
- Download the `Arduino-Bounce` library from https://github.com/mpflaga/Arduino-Bounce.git. Either use git to clone it there, or download the zip version and extract it in this folder. You should now have a `Arduino-Bounce` folder in the `Arduino/libraries/` folder.

### Build
Use the upload button to compile and flash, make sure you selected the right serial port on the top menu.
