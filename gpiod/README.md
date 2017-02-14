# GPIO Based Volume Controls

This component uses the `wiringPi` library to control AirPlay playback
options via monitoring GPIO pin changes caused by push buttons.

The shairport-dacpd daemon receives messages generated from this component
when a button is pushed.

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

I also see `wiringPi.h` in `/usr/include` and `libwiringPi.so` and 
`libwiringPiDev.so` in `/usr/lib`.  So, this will allow me to build a GPIO
based application using the library without installing anythin else.

# 


