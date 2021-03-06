# alarm4pi v0.2
This project implements an application software to convert a Raspberry Pi
into an alarm system for your house.
It has been tested with a Raspberry Pi 3 model B and Raspbian 32 bits (now
called Raspberry Pi OS).

## Description
alarm4pi monitors the state of a presence-detection sensor (PIR sensor) and
when it is activated:
* a notification is sent to the user's mobile phone through the Internet
* some photographs are taken by means of the Raspberry Pi's camera and stored
in the 'captures' directory of alarm4pi
* the photos are then inmediately uploaded to the Owncloud server specified by
the user.
* if a lamp is connected to the Raspberry Pi through a relay, and it is night,
the lamp is switched on during the capture of images.

alarm4pi also implement a web streaming server so the user can remotelly watch
through the Raspberry Pi camera at any moment, manage the state of the alarm
system and activate/deactivate relays connected to the Raspberry Pi's GPIO pins.

### Required hardware
This alarm system is intended to be a do-it-yourself device and requires the
following hardware to be attached to your Raspberry Pi:
* PIR sensor which must be connected to the GPIO 17 (pin 11 in pin header)
* The Raspberry Pi camera connected to the camera port
* Optionally up to 4 realy switch can be connected to GPIO 8, 9, 10 and 11, for
example, to remotelly switch on/off house devices. If a lamp is connected, it
is assumed to be connected to GPIO 8 through an active-low realy.

### Required software
alarm4pi is composed of the following software components:
* mjpg-streamer: This is the used web streaming server from jacksonliam
(originally created by Tom Stöveken). It is already included
in the alarm4pi repository, but it must be compiled manually serparately.
* Pushover (optional): This app (available for iOS and Android) must be
(purchased and) instaled in the user's mobile phone in order to receibe the
intrusion and information notifications from alarm4pi.
* SocketXP (optional): if your Raspberry Pi is connected to the Internet
through an Internet service provider that uses CG-NAT (carrier-grade network
address translation), it means that it is not directly accesible from the
Internet: the Raspberry Pi can stablish outgoing connections but cannot
receive incomming connections. So, in order for the alarm4pi web server to be
accessible remotely when it is behind a CG-NAT a reverse tunneling mechanism
is implemented. alarm4pi uses a service of the SocketXP company. So, if this
is your case, you must create an account in SocketXP. Otherwise, you can
disable the reverse tunneling mechanism of alarm4pi as described below.
* Owncloud (optional): Apart from storing the photos taken locally, they are
uploaded to a Owncloud server where they can be accessed even if the Raspberry
Pi is disconnected.

## Software prerequisites and depencencies
Before running alarm4pi you must prepare and configure some software
components. The first step is downloading alatm4pi repository. Then:

### Pushover (optional)
You must manually configure the notification system so that a message is
sent to your mobile phone when activity is detected. For that, you must:
* Buy the Pushover application and install it in your phone so that you
get a user key.
* Create the file pushover_conf.txt in the project directory with the
following content:
```
server_url=http://api.pushover.net/1/messages.json
token=<token>
user=<your user key>
```
If this configuration file is not created, the notification mechanism is
disabled.

### SocketXP (optional)
You must manually setup the reverse tunneling mechanism (in case your
Raspberry Pi is connected to the Internet through a connection with CG-NAT).
alarm4pi uses SocketXP. So:
* Register in SocketXP to get an account and get the tunneling plan
* Download the socketxp agent and move it to a directory in the system path
as shown in your socketxp user portal: https://portal.socketxp.com/ when
you log in the web. Something like:
```
curl -O https://portal.socketxp.com/download/arm/socketxp && chmod +wx socketxp && sudo mv socketxp /usr/local/bin
```
* Execute the socketxp agent in your Raspberry Pi to specify the logging
information as shown in your user portal. Something like:
```
socketxp login "<your authentication token>"
```
If you do not need to use reverse tunneling, you can disable this mechanism
in alarm4pi.c by commenting the REVERSE_TUNNELING definition.

### Owncloud (optional)
You must have ac account in an Owncloud server in order for the photos to be
uploaded. Moreover, you must manually configure the Owncloud mechanism for
alarm4pi to upload the photos. For that, you must:
* Install the command-line Owncloud application in your Raspberry by typing:
``` sudo apt-get install owncloud-client-cmd ```
* Create a directory in your Owncloud server account called 'captures' or
whatever name you have used for the directory storing the photos in your
Raspberry.
* Create the file owncloud_conf.txt in the project directory with the
following content:
```
server_url=https://<your server name>/remote.php/webdav/captures/
user=<your owncloud user>
password=<your owncloud user password>
```

If this configuration file is not created, the upload mechanism is disabled.

### Camera
The Raspberry Pi camera interface must be activated in the current Raspbian
build. If it is not, run:
``` sudo raspi-config ```
to activate it.

### MJPG-streamer compilation (and test)
alarm4pi uses mjpg-streamer from https://github.com/jacksonliam/mjpg-streamer
which is already included in the alarm4pi project but must be compiled
manually. For that:
* You must install its software dependencies:
```
sudo apt-get install cmake libjpeg8-dev
sudo apt-get install gcc g++
```
Then compile mjpg-streamer by typing from the alarmn4pi project directory:
```
cd mjpg-streamer-master/mjpg-streamer-experimental
make
```
(The installation is not needed and the web server content can be kept in
its default location)
You can check the repository's install instructions if needed.

The operation of mjpg-streamer can be checked separately by typing:
```
cd mjpg-streamer-master/mjpg-streamer-experimental
./mjpg_streamer -i "input_raspicam.so" -o "output_http.so -w www -p 8008
```
and then browsing the site http://localhost:8008 in the Raspberry Pi browser.

## alarm4pi compilation
Before compiling alarm4pi you must install the following dependencies:
* libminiupnpc-dev
* gcc

Since gcc was installed before for mjpg-streamer, you just need to type:
``` sudo apt-get install libminiupnpc-dev ```
and then execute ```make``` in the project directory

## alarm4pi service install ###
The install_service.sh script can be executed to create a systemd service
that will automatically start alarm4pi on boot. This script can be executed typing:
``` sudo ./install_service.sh ```

## Log files
alarm4pi creates 2 log files in the log directory.

### daemon.log
In this file alarm4pi registers the actions taken, especially during program
initiallation and termination. If an error occurred, it is reported in this
file so that the user can address it.

### evets.log
In this file alarm4pi registers the alarm events, that is, the activations of
the PIR sensor.

