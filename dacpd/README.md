# Shairport DACP Daemon

This code is how we control the iDevice streaming the audio.  For example,
we can play the next song, or modify the volume.  This is done via the
DACP protocol (Digital Access Control Protocol).  This is an HTTP protocol.

This is a UDP server on port 3391.  This code was copied from an article
I read here...

* https://www.sugrsugr.com/index.php/airplay-prev-next/

It listens for messages from two sources:

 1) The modified shairport server (DACP ID and Active Remote ID info) 
 2) User messages like 'nextitem', 'previtem', etc.

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

Using autotools...

    ./autogen.sh
    ./configure
    make
    sudo make install


