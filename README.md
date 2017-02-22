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

# TODO

* Create services for dacpd and gpiod
* Install services via autotools make install
* Get Pi 3 model B device + Power Supply
* Get HiFi Berry DAC+ RCA
* Test
