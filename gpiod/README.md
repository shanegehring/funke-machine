# GPIO Daemon 

This component uses the `wiringPi` library to control AirPlay playback
options (next, prev, mute, etc.) via monitoring GPIO pin changes caused
by physical push buttons.  Additionally, it receives IPC messages from
the shairport-dacpd deaemon indicating when an AirPlay device is 
active/connected.  We use this information to change output GPIO pins 
driving indicator LEDs on the front of the console.

The shairport-dacpd daemon receives messages generated from this component
when a button is pushed.  It then handles the DACP portion of communicating
to the AirPlay client.

# The wiringPi Library

On my version of raspbian, `wiringPi` was already installed.  You can test
for it like this...

    gpio -v
    gpio version: 2.32
    Copyright (c) 2012-2015 Gordon Henderson
    This is free software with ABSOLUTELY NO WARRANTY.
    For details type: gpio -warranty
    
    Raspberry Pi Details:
      Type: Model B, Revision: 03, Memory: 512MB, Maker: Egoman
      * Device tree is enabled.
      * This Raspberry Pi supports user-level GPIO access.
        -> See the man-page for more details
        -> ie. export WIRINGPI_GPIOMEM=1

I also saw `wiringPi.h` in `/usr/include` and `libwiringPi.so` and 
`libwiringPiDev.so` in `/usr/lib`.  So, this allowed me to build a GPIO
based application using the library without installing anything else.

# Without the wiringPi Library

    git clone git://git.drogon.net/wiringPi
    cd wiringPi
    ./build

    gpio -v

# Wiring pi GPIO outputs from command line

    gpio -g mode  24 out
    gpio -g write 24 1
    gpio -g write 24 0

# Design

The external GPIOs are each connected to simple momentary push button 
switches without hardware filters.  The main thread just sets things up
and goes to sleep.  The setup involves configuring each GPIO as an input 
with a pullup resistor (when the switch is open/not pressed, the input is 
pulled high by the resistor).  We then register a negative edge interrupt
handler for each button.  A software based debouncing implementation takes 
care of the transition noise from the mechanical switch.  Communication to 
the DACP daemon is just a simple UDP socket write of a string like "volumeup".

# Installation

If you just want to build and install the component, do this.

    ./autogen.sh
    ./configure
    make
    sudo make install

    sudo cp gpiod.service /lib/systemd/system/
    sudo systemctl enable gpiod # Start after reboot
    sudo systemctl start  gpiod
    sudo systemctl status gpiod

