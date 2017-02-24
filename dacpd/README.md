# Shairport DACP Daemon

This component controls the audio streaming device.  For example, we can play 
the next song, or modify the volume.  This is done via DACP (Digital Access 
Control Protocol).  This is an HTTP protocol.

The component uses the shared IPC utilies and is both a UDP server and client.
Much of this code was leveraged from an article I read here...

* https://www.sugrsugr.com/index.php/airplay-prev-next/

It listens for messages from two sources:

1. The modified shairport server (DACP ID and Active Remote ID info) 
2. User messages like 'nextitem', 'previtem', etc.

It also sends messages to the GPIO daemon that indicate when a new AirPlay
client is attached or detached.

Example of how to send a user message...

    echo -ne nextitem | nc -u -4 localhost 3391

Commands via DACP:

| COMMAND      | DESCRIPTION                       |
|--------------|-----------------------------------|
|beginff       | begin fast forward                |
|beginrew      | begin rewind                      |
|mutetoggle    | toggle mute status                |
|nextitem      | play next item in playlist        |
|previtem      | play previous item in playlist    |
|pause         | pause playback                    |
|playpause     | toggle between play and pause     |
|play          | start playback                    |
|stop          | stop playback                     |
|playresume    | play after fast forward or rewind |
|shuffle_songs | shuffle playlist                  |
|volumedown    | turn audio volume down            |
|volumeup      | turn audio volume up              |

# Installation

If you just want to build and install the component, do this...

    sudo apt-get install libavahi-core-dev

    ./autogen.sh
    ./configure
    make
    sudo make install

    sudo cp dacpd.service /lib/systemd/system/
    sudo systemctl enable dacpd # Start after reboot
    sudo systemctl start  dacpd
    sudo systemctl status dacpd

