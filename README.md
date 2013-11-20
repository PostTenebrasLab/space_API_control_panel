# Space_API_control_panel

sketch for the control panel of the lab status (a box in the lab with buttons that control open time of the lab...)

# Build steps
## Requirements
To use gcc-avr you need to set:

    export BOARD=nan0328        #in this case but make boards will show you all board available
    export ARDUINODIR=~/your_arduino_install_dir

and install the following package under ubuntu 12.04

    sudo apt-get install avrdude binutils-avr gcc-avr avr-libc gdb-avr arduino-mk

## Compilation

You can simply run

    make
    make upload

And voila !
