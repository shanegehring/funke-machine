# Funke Machine

This project upgrades an old [Model 1519 Bendix Radio](http://www.radiomuseum.org/r/bendix_1519.html)
console (circa 1947) into an AirPlay enabled stereo with local playback controls.
The console itself is a family heirloom handed down to my wife who's maiden name 
is *Funke* (hence Funke Machine).  My wife restored the wood and interior of the
console several years ago.  But, it hasn't been used for playing music in a very
long time.  

We were looking for something that was easy to use but retained the original look
and feel.  I chose to make it an AirPlay server because I feel this is the easiest 
way to play music from Spotify, Pandora, Apple Music, etc.  We've used blue tooth 
speakers in the past, but found them annoying from a setup standpoint and and 
limited in range.  In contrast, AirPlay connections are supported natively within
most apps and are only limited by the WiFi coverage in your home.

I used the following off the shelf hardware components:

* Brains: [Raspberry Pi 3](http://a.co/fCFclja) (built in WiFi)
* DAC: [Hifiberry DAC+ Standard RCA](http://a.co/i4NMN1Y)
* AMP: [SMSL SA50 50Wx2 TDA7492 Class D Amplifier](http://a.co/5eHXQt0)
* Speakers: [Micca MB42X Bookshelf Speakers](http://a.co/eMOfPOV)

I'm not an audiophile by any means.  But, I did spend some time researching how to 
get good sound quality.  The result is the DAC/AMP/Speaker combination you see
above.  I think it sounds great with plenty of volume to fill our large living room.

In addition, I added local playback controls by using the existing radio buttons (old
mechanical slide switches) and status LEDs.  This was all done via the pi GPIOs.  For
example, if someone is air playing music from their phone, I can walk up to the console
and adjust the volume or pause the music or skip to the next song.  When installing my
upgrades, I found that original console had a little indicator light centered in the 
bottom of front molding.  I routed a couple LEDs to the opening and used the GPIO outputs
on the pi to indicated status.  A white LED shines when the unit is active/ready and a
green LED shines when someone is playing content via AirPlay.

The heart of the system relies on the excellent work done in the `shairport-sync` project
by Mike Brady and James Laird and others.  I just hacked a small portion of their code to
enable the local playback controls via DACP (Digital Access Control Protocol).  See my 
fork of shairport-sync [here](https://github.com/shanegehring/shairport-sync).

Below are a combination of installation instructions and notes I kept during the 
process.

## Raspberry Pi 3 Setup

Install Raspbian with the hifiberry hardware pre-configured.  I used the 'lite' version 
from here: https://www.hifiberry.com/build/download/

### Enable SSH

    sudo raspi-config
    Interfacing Options -> SSH -> Enable
    Change User Password
    Reboot

### SSH Login

At this point, log in remotely via ssh like this...

    ssh pi@192.168.1.64

### Enble WiFi...

    sudo vi /etc/wpa_supplicant/wpa_supplicant.conf

    ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
    update_config=1
    
    network={
        ssid="Gehring"
        psk="password_goes_here"
    }

## Shairport Sync Setup

More complete notes on other options are available in the official shairport-sync README.
Below are the specific steps I took for my hardware setup.  

### Install sharepoint-sync build dependencies

    sudo apt-get update
    sudo apt-get install build-essential git
    sudo apt-get install autoconf automake libtool libdaemon-dev libasound2-dev libpopt-dev libconfig-dev
    sudo apt-get install avahi-daemon libavahi-client-dev
    sudo apt-get install libssl-dev

### Build sharepoint-sync

    mkdir git
    cd git
    git clone https://github.com/shanegehring/shairport-sync.git
    cd shairport-sync
    # Configure build
    autoreconf -i -f
    ./configure --sysconfdir=/etc --with-alsa --with-avahi --with-ssl=openssl --with-metadata --with-systemd
    # Do the build
    make
    # Add a sharepoint-sync user/group
    getent group  shairport-sync &>/dev/null  || sudo groupadd -r shairport-sync >/dev/null
    getent passwd shairport-sync &> /dev/null || sudo useradd  -r -M -g shairport-sync -s /usr/bin/nologin -G audio shairport-sync >/dev/null
    sudo make install
    # Enable on bootup
    sudo systemctl enable shairport-sync

### Configure

    sudo vi /etc/shairport-sync.conf

My settings...

    general = {
      name = "Funke Machine";
    };
    
    alsa = {
      output_device = "hw:0";
    };

### Run

    sudo systemctl start  shairport-sync
    sudo systemctl status shairport-sync

### Server log file

Note, I saw some messages about "pulse" which don't seem to matter.

    tail -f /var/log/syslog

## Test it out

At this point, if everything worked, you should see a "Funke Machine" available
from your iPhone.  If you've connected the hifiberry to a speaker, you should
be able to hear it play.  This is the basic (out of the box) behavior of shairport-sync
(which is great).  But, I wanted to add local playback and status features via GPIOs.  
See the following sub projects for more info...

* [DACPD](dacpd/README.md)
* [GPIOD](gpiod/README.md)

# General Notes

## RTSP - Real Time Streaming Protocol

Airplay uses RTSP - Real Time Streaming Protocol

See rtsp.c for an implementation.  In particular, see the `method_handlers` to get a feel
for how the server handles requests sent from the client.

The client will initially ask the server for "options" via a C->S OPTIONS command.
The server will responds with list of commands it supports.

Example from rstp.c...

    resp->respcode = 200;
    msg_add_header(resp, "Public", "ANNOUNCE, SETUP, RECORD, "
                                 "PAUSE, FLUSH, TEARDOWN, "
                                 "OPTIONS, GET_PARAMETER, SET_PARAMETER");

So, our shairport server supports these commands.

## DACP - Digital Access Control Protocol

https://nto.github.io/AirPlay.html#audio-remotecontrol

I'm interested in controlling the Airplay client from the server.  This is done
via HTTP requests to the DACP server on the iDevice.  The client advertises
the DACP-ID during the "SETUP" Command from the client.  To "see" the client
server interactions.  Stop the daemonized service, then launch from the 
command line with debug enabled....

    sudo systemctl stop shairport-sync
    sudo service shairport-sync status
    shairport-sync -vvv

Initially, just connect via your phone to the Airplay service (without playing
a song).  You will see this...

    New RTSP connection from [fe80::1cae:be97:c414:3a33]:57143 to self at [fe80::31e2:4ae1:1b4e:870d]:5000.
        CSeq: 0.
        DACP-ID: CB899B2DF4926949.
        Active-Remote: 338386500.
        User-Agent: AirPlay/310.17.
    RTSP Packet received of type "OPTIONS":
      Type: "CSeq", content: "0"
      Type: "DACP-ID", content: "CB899B2DF4926949"
      Type: "Active-Remote", content: "338386500"
      Type: "User-Agent", content: "AirPlay/310.17"
    RTSP Response:
      Type: "CSeq", content: "0"
      Type: "Server", content: "AirTunes/105.1"
      Type: "Public", content: "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER"

You can see a new connection and an OPTIONS request/response.  Take note of the DACP-ID and Active-Remote fields.

    shairport-sync -vvv 2>&1 | egrep -i 'dacp|daid|remote' | grep Type
    Type: "DACP-ID", content: "C5D6F116EEC74A5"
    Type: "Active-Remote", content: "2379330968"

The DACP-ID plus Active-Remote are what we need to talk (via HTTP to the client from the server).

See: https://www.sugrsugr.com/index.php/airplay-prev-next/

This dude basically did what I want to do.  He modified Shairport to support server initiated
song controls.  He did it in a cool way too.  He modified rtsp.c to publish the DACP-ID and 
Active-Remote IDs to another process running on the PI.  This other process is what receives
control messages from the user to change songs.  Pretty cool.  A very minor update to rtsp.c
and he provides a complete implementation of his control server.

Install 'avahi-browse' from avahi-utils..

    sudu apt-get install avahi-utils

So, as you are streaming a song, run this..

    avahi-browse -v -a --resolve -p | grep -i itunes
    +;eth0;IPv6;iTunes_Ctrl_22F6CF2AF21C9417;iTunes Remote Control;local
    +;eth0;IPv4;iTunes_Ctrl_22F6CF2AF21C9417;iTunes Remote Control;local
    =;eth0;IPv6;iTunes_Ctrl_22F6CF2AF21C9417;iTunes Remote Control;local;Shanes-iPhone-6.local;192.168.1.2;57221;
    =;eth0;IPv4;iTunes_Ctrl_22F6CF2AF21C9417;iTunes Remote Control;local;Shanes-iPhone-6.local;192.168.1.2;57221;

    avahi-browse -v -a -r -p -t | perl -lane 'if (($_ =~ /iTunes/)and($_ =~ /IPv4/)) { @c = split(/;/, $_); print "$c[3] $c[6] $c[7] $c[8]"; }'

Now combine the data from the browse command with curl to control the client...

    curl 'http://192.168.1.2:57221/ctrl-int/1/nextitem' -H 'Active-Remote: 415355085' -H 'Host: Shanes-iPhone-6.local.'
    curl 'http://192.168.1.2:57221/ctrl-int/1/previtem' -H 'Active-Remote: 415355085' -H 'Host: Shanes-iPhone-6.local.'
    curl 'http://192.168.1.2:57221/ctrl-int/1/volumeup' -H 'Active-Remote: 415355085' -H 'Host: Shanes-iPhone-6.local.'

The shairport daemon also advertises itself via mDNS.  Grep for the name of your server to see it.

I could just parse the avahi-browse output each time a button is pushed.  However, it takes some time (seconds) to 
get the results back.  So, it would be better to take the approach used by the author above here.   That is, to
make calls into the AVAHI library.  My implementation will also monitor the Raspbery PI GPIOs to trigger the 
events.

http://www.win.tue.nl/~johanl/educ/IoT-Course/mDNS-SD%20Tutorial.pdf

## Systemd Services

To launch gpiod and dacpd on bootup, I needed to create `unit` files for
systemd.

    sudo cp dacpd.service /lib/systemd/system/
    sudo systemctl enable dacpd # Start after reboot
    sudo systemctl start  dacpd
    sudo systemctl status dacpd

    sudo cp gpiod.service /lib/systemd/system/
    sudo systemctl enable gpiod # Start after reboot
    sudo systemctl start  gpiod
    sudo systemctl status gpiod

    tail -f /var/log/syslog

    sudo shutdown -r now
