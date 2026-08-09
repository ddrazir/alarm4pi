# alarm4pi v0.3
This project implements an application to convert a Raspberry Pi
into an alarm system for your house.
It has been tested with a Raspberry Pi 3 Model B and 64-bit Raspberry Pi OS.

## Description
alarm4pi monitors a presence-detection sensor (PIR sensor).
When the sensor is activated:
* A notification is sent to the user's mobile phone through the Internet
* A photograph is taken by means of the Raspberry Pi's camera and stored
in the 'captures' directory of alarm4pi
* The photos can then be immediately uploaded to the ownCloud server specified
by the user (if this storage component is anebled).
* If a lamp is connected to the Raspberry Pi through a relay, and it is night,
the lamp may be switched on during the capture of images (depending on
configuration. See below).

alarm4pi also implements a web streaming server so the user can remotely watch
through the Raspberry Pi camera at any moment, manage the state of the alarm
system and activate/deactivate relays connected to the Raspberry Pi's GPIO
pins. No credentials are required to access this interface, so be careful not
to share the URL or display compromising images through the camera.

### Required hardware
This alarm system is intended to be a do-it-yourself device and requires the
following hardware to be attached to your Raspberry Pi:
* PIR sensor which must be connected to the GPIO 17 (pin 11 in pin header). The
output of this PIR sensor must be 3.3-V compatible.
* The Raspberry Pi camera connected to the camera port
* Optionally up to 4 relay modules can be connected to GPIO 8, 9, 10 and 11, for
example, to remotely switch on/off house devices. If a lamp is connected, it
is assumed to be connected to GPIO 8 through an active-low relay.
* Optionally up to 2 contact sensors can be connected (to GPIO 5 and 6). These
inputs are configured internally with pull-up resistors, so externally
short-circuiting any of these pins to GND triggers an alarm event if they are
configured as monitored.

#### Connection summary:
Raspberry Pi 3 Model B has a single 40-pin expansion header. The pins of this
physical connector are numbered from pin 1 to pin 40.
Through this connector it provides access to 28 general purpose input/output
signals (GPIOs). From the software's point of view these signals are named:
GPIO 0-16 and GPIO 21-31.


Connections for alarm4pi:

| Header pin | =Function | -> Connected to |
| -------- | -------- | -------- |
| Pin 2 | = 5 V DC | -> PIR sensor input power |
| Pin 4 | = 5 V DC | -> Relays' input power |
| Pin 6 | = GND | -> PIR sensor GND |
| Pin 9 | = GND | -> PIR sensor GND |
| Pin 11 | = GPIO 17 | -> PIR sensor output signal |
| Pin 14 | = GND | -> Relays' GND |
| Pin 19 | = GPIO 10 | -> Relay 3 input control signal |
| Pin 20 | = GND | -> Relays' GND |
| Pin 21 | = GPIO 9 | -> Relay 2 input control signal |
| Pin 23 | = GPIO 11 | -> Relay 4 input control signal |
| Pin 24 | = GPIO 8 | -> Relay 1 input control signal |
| Pin 25 | = GND | -> Relays' GND |
| Pin 29 | = GPIO 5 | -> Contact sensor 1 |
| Pin 30 | = GND | -> Contact sensors' GND |
| Pin 31 | = GPIO 6 | -> Contact sensor 2 |

### Core dependencies and optional integrations
alarm4pi includes the following software component:
* mjpg-streamer from ArduCAM: This is the used web streaming local server.
It is a version of the project from jacksonliam (originally created by Tom
Stöveken). It is already included in the alarm4pi repository, but it must be
compiled manually separately.

Besides, the following software components can be integrated with alarm4pi:
* Means of communication to notify the user: When an alarm event is detected
alarm4pi must notify the user. Two options are available for this:
  * Pushover (optional): This app/service (available for iOS and Android) can
be (purchased and) installed in the user's mobile phone in order to receive the
intrusion and information notifications from alarm4pi.
  * Telegram (optional): This free app/service (available for iOS and Android)
can be installed in the user's mobile phone in order to receive the intrusion
and information notifications from alarm4pi.
* Means of communication from the user to alarm4pi: if your Raspberry Pi is
connected to the Internet through an Internet service provider that uses CG-NAT
(carrier-grade network address translation), it means that it is not directly
accessible from the Internet: the Raspberry Pi can establish outgoing connections
but cannot receive incoming connections. So, in order for the web server to be
remotely accessible when the Raspberry Pi is behind CG-NAT a reverse tunneling
or other mechanism must be implemented. So, if this is your case, you must
enable one of these options:
  * SocketXP (optional): alarm4pi can use the service of the SocketXP company.
For this, you must create an account in SocketXP. Otherwise, you can disable the
reverse tunneling mechanism of alarm4pi as described below.
  * Tailscale (optional): alarm4pi can use the service of the Tailscale company.
For this, you must create an account in Tailscale. Otherwise, you can disable
this VPN/mesh-network solution that provides remote access despite NAT as
described below.
* ownCloud (optional): Apart from storing the photos taken locally, they can be
uploaded to an ownCloud server where they can be accessed even if the Raspberry
Pi is disconnected.

## Software prerequisites and dependencies
Before running alarm4pi you must prepare and configure some software
components. The first step is downloading the alarm4pi repository.
Then, the GPIO control library is used by alarm4pi, so it must be installed:
``` sudo apt-get install libgpiod-dev ```

Depending on the features that you want to enable more packages should be installed and further configuration performed:

### Pushover (optional)
To receive notifications, you must configure one notification service, so that
a message is sent to your mobile phone when activity is detected. For that, you
can use Pushover (Pushover is a paid service. For a free alternative check
Telegram in the next section).
In order to use Pushover:
* Purchase the Pushover application/service and install the app on your phone so
that you get a user key.
* Create the file pushover_conf.txt in the project directory with the
following content:
```
server_url=http://api.pushover.net/1/messages.json
token=<token>
user=<your user key>
```
If no notification configuration file is created, the notification
mechanism is disabled.

### Telegram (optional)
To receive notifications (on your mobile phone) when activity is detected, you
must configure one notification service. For using Telegram, you can follow
these steps: 
* Install the Telegram app on your mobile phone. In the Telegram app:
* Create a Telegram bot
   * Search for @BotFather
   * Send: /newbot;
   * Follow the prompts: Choose a display name (e.g. "alarm4pi"), choose a username ending in bot (e.g. alarm4pi_notify_bot)
   * BotFather will reply with a token similar to: "123456789:AAHk3...your_token...". Keep this token secret
* Start the bot: In the Telegram app:
   * Search for your bot's username
   * Open the chat
   * Press Start (or send /start). This allows the bot to send you messages.
* Get your chat ID. After you have started the bot, open an Internet browser and:
   * Open this URL: https://api.telegram.org/bot<token>/getUpdates (replace <token>)
   * You'll receive a JSON text file
   * From that text copy the "id" in the "chat" section
   * Create the file telegram_conf.txt in the project directory with the
following content:
```
chat_id=<the ID of your chat with the bot>
api_token=<your bot token>
```
The bot token is a secret credential and should never be committed to the
repository.
If no notification configuration file is created, the notification
mechanism is disabled.

### SocketXP (optional)
In case your Raspberry Pi is connected to the Internet through a connection
with CG-NAT you must manually set up a reverse tunneling mechanism. For
example, alarm4pi can use SocketXP (SocketXP is a paid service. For a free
alternative check Tailscale).
To install SocketXP and setup on your Raspberry Pi:
* Register in SocketXP to create an account and select a tunneling plan
* Download the socketxp agent and move it to a directory in the system path
as shown in your socketxp user portal: https://portal.socketxp.com/ when
you log on the web. Something like:
```
curl -O https://portal.socketxp.com/download/arm/socketxp && chmod +wx socketxp && sudo mv socketxp /usr/local/bin
```
* Execute the socketxp agent in your Raspberry Pi to specify the login
information as shown in your user portal. Something like:
```
socketxp login "<your authentication token>"
```
If you do not need to use reverse tunneling, you can disable this mechanism
in alarm4pi.c by commenting the REVERSE_TUNNELING definition.

### Tailscale (optional)
In case your Raspberry Pi is connected to the Internet through a connection
with CG-NAT you need a mechanism that allows remote access despite CG-NAT.
For example, alarm4pi can use Tailscale. To install Tailscale and set up on
your Raspberry Pi:
* Open a terminal on your Raspberry Pi
* Update the Raspberry Pi OS package lists.
Run: ``` sudo apt-get update ```
* Run the installation script. Run: ``` curl -fsSL https://tailscale.com/install.sh | sh ```
* Authenticate your device. Run: ``` sudo tailscale up ```

### ownCloud (optional)
You must have an account in an ownCloud server in order for the photos to be
uploaded. Moreover, you must manually configure the ownCloud mechanism for
alarm4pi to upload the photos. For that, you must:
* Create a directory in your ownCloud server account called 'captures' or
whatever name you have specified in the source code (see below) for the
directory storing the photos in your Raspberry Pi.
* Create the file owncloud_conf.txt in the project directory with the
following content:
```
server_url=https://<your server name>/owncloud/remote.php/dav/files/
user=<your ownCloud user>
password=<your ownCloud user password>
```
It is worth verifying the specified server URL against the actual WebDAV
endpoint of your ownCloud server. The username in the final part of the URL
must bot be specified.
Be careful and protect this configuration file so that it is not shared with
other users. Do not commit this file containing credentials to Git.
If this configuration file is not created, the upload mechanism is disabled.

### Camera
alarm4pi uses the Raspberry Pi camera (connected via a flat cable).
The current version of alarm4pi uses Raspberry Pi's modern libcamera-based
camera stack, which is used by current 64-bit Raspberry Pi OS releases. The
reason for this is that alarm4pi includes the mjpg-streamer's
input_libcamera.so.

### MJPG-streamer compilation (and test)
alarm4pi uses mjpg-streamer from https://github.com/ArduCAM/mjpg-streamer

As of August 2026 the version of ArduCAM mjpg-streamer in its GitHub repository
(Commit 99097c8) has some bugs in the input_libcamera pluggin. These bugs make
mjpg-streamer to crash and prevent it from working on current 64-bit Raspberry
Pi OS.
However, these bugs have been fixed in the version included in the alarm4pi
project (the two .cpp files of the plugin have been patched).

Anyway, mjpg-streamer must be compiled manually. For that:
* You must install its software dependencies:
```
sudo apt-get install cmake libjpeg62-turbo-dev libcamera-dev
sudo apt-get install gcc g++
```
Then compile mjpg-streamer by typing from the alarm4pi project directory:
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
./mjpg_streamer -i "input_libcamera.so" -o "output_http.so -w www -p 8008"
```
and then browsing the site http://localhost:8008 in the Raspberry Pi browser.

## alarm4pi configuration
As stated previously owncloud_conf.txt, pushover_conf.txt or telegram_conf.txt
file must be created to enable the corresponding component.
Moreover, some configurations must be performed in the source code:
* In alarm4pi.c, you may want to change the following line:
```
#define REVERSE_TUNNELING <number>
```
where <number> is 1 to use SocketXP tunneling, or 2 to use Tailscale  unneling.
* In gpio_polling.c, you may want to change the following line:
```
const int Alarm_gpios[]={<list_of_signals_to_monitor>};
```
where <list_of_signals_to_monitor> is a comma-separated list of the following
labels depending on what input pins you want alarm4pi to continuously check:
PIR_GPIO (for the passive infrared sensor (PIR sensor)), CONTACT1_GPIO (for the
1st electric contact to GND), CONTACT2_GPIO (for the 2nd electric contact to
GND).

* In gpio_polling.c, you may want to uncomment the following line:
```
// #define RELAY1_ON_ALARM_EVENT
```
to switch on the relay 1 (usually connected to a light switch) when an alarm
sensor is activated.

* In gpio_polling.c, you may want to change the following line:
```
#define OWNCLOUD_REMOTE_DIRECTORY "captures"
```
To 

## alarm4pi compilation
Before compiling alarm4pi you must install the following dependencies:
* libminiupnpc-dev
* gcc

Since gcc was installed before for mjpg-streamer, you just need to type:
``` sudo apt-get install libminiupnpc-dev ```
and then execute ```make``` in the project directory.
Also install the libcurl development library on your Raspberry Pi by typing:
``` sudo apt-get install libcurl4-openssl-dev ```

## alarm4pi service install
The install_services.sh script can be executed to create a systemd service
that will automatically start alarm4pi on boot.
Before running the script open it with a text editor and modify the value
of the ExecStart parameter to indicate the directory where alarm4pi project is.
This script can be executed by typing:
``` sudo ./install_services.sh ```
To uninstall the service, run:
``` sudo ./uninstall_services.sh ```

## Log files
alarm4pi creates two log files in the log directory.

### daemon.log
In this file alarm4pi registers the actions taken, especially during program
initialization and termination. If an error occurred, it is reported in this
file so that the user can address it.

### events.log
In this file alarm4pi registers the alarm events, that is, the activations of
the PIR sensor.

