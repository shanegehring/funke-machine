# Funke Machine


## Install Raspbian with Hifiberry Stuff

I used the 'lite' version.  

## Enable SSH

    sudo raspi-config
    Interfacing Options -> SSH -> Enable
    Change User Password
    Reboot

## SSH Login

At this point, log in remotely via ssh like this...

    ssh pi@192.168.1.64

## Enble wifi...

    sudo vi /etc/wpa_supplicant/wpa_supplicant.conf

    country=GB
    ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
    update_config=1
    
    network={
        ssid="Gehring"
        psk="**passwordhere**"
    }

## Install Sharepoint-Sync Build Dependencies

    sudo apt-get update
    sudo apt-get install build-essential git
    sudo apt-get install autoconf automake libtool libdaemon-dev libasound2-dev libpopt-dev libconfig-dev
    sudo apt-get install avahi-daemon libavahi-client-dev
    sudo apt-get install libssl-dev

## Build Sharepoint-Sync    

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
    getent group shairport-sync &>/dev/null || sudo groupadd -r shairport-sync >/dev/null
    getent passwd shairport-sync &> /dev/null || sudo useradd -r -M -g shairport-sync -s /usr/bin/nologin -G audio shairport-sync >/dev/null
    sudo make install
    # Enable on bootup
    sudo systemctl enable shairport-sync
    
## Configure

    sudo vi /etc/shairport-sync.conf
    
My settings...

    general = {
      name = "Funke Machine";
    };
    
    alsa = {
      output_device = "hw:0";
    };
    
## Run

    sudo systemctl start shairport-sync
    sudo service shairport-sync status
    
## Server log file

Note, I saw some messages about "pulse" which don't seem to matter.

    /var/log/syslog

## Test it out

At this point, if everything worked, you should see a "Funke Machine" available
from your iPhone.


# Notes

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


# Systemd Services

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
