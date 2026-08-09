# alarm4pi v0.3
This project implements an application software to convert a Raspberry Pi
into an alarm system for your house.
It has been tested with a Raspberry Pi 3 model B and Raspberry Pi OS (64-bit).

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
* Optonally up to 2 contact sensors can be connected to GPIO 5 and 6 and should short-circuit any of these pens to GND when activated.

#### Connection resume:
Raspberry Pi 3 Model B has a single 40-pin expansion header. Header pins are numbered from pin 1 to pin 40.
It provides access to 28 general purpose input/output pins (GPIOs): GPIO 0...16 and GPIO 21..31.

Connections for alarm4pi:

| Header pin | =Function | -> Connected to |
| -------- | -------- | -------- |
| Pin 2 | = 5 V DC | -> PIR sensor input power |
| Pin 4 | = 5 V DC | -> Relays' input power |
| Pin 6 | = GND | -> PIR sensor GND |
| Pin 9 | = GND | -> PIR sensor GND |
| Pin 11 | = GPIO 17 | -> PIR sensor output signal |
| Pin 14 | = GND | -> Relays' GND |
| Pin 19 | = GPIO | 10 -> Relay 3 input control signal |
| Pin 20 | = GND | -> Relays' GND |
| Pin 21 | = GPIO 9 | -> Relay 2 input control signal |
| Pin 23 | = GPIO 11 | -> Relay 4 input control signal |
| Pin 24 | = GPIO 8 | -> Relay 1 input control signal |
| Pin 25 | = GND | -> Relays' GND |
| Pin 29 | = GPIO 5 | -> Contact sensor 1 |
| Pin 30 | = GND | -> Contact sensors' GND |
| Pin 31 | = GPIO 6 | -> Contact sensor 2 |

### Required software
alarm4pi is composed of the following software component:
* mjpg-streamer from ArduCAM: This is the used web streaming local server.
It is a version of the project from jacksonliam (originally created by Tom
Stöveken). It is already included in the alarm4pi repository, but it must be
compiled manually serparately.

The following software components can be integrated with alarm4pi:
* Means of communication to notify the user: When an alarm event is detected
the alarm4pi must notify the user. Two options are available for this:
  * Pushover (optional): This app (available for iOS and Android) can be
(purchased and) instaled in the user's mobile phone in order to receive the
intrusion and information notifications from alarm4pi.
  * Telegram (optional): This free app (available for iOS and Android) can be
instaled in the user's mobile phone in order to receive the intrusion and
information notifications from alarm4pi.
* Means of communication from the user to alarm4 pi: if your Raspberry Pi is
connected to the Internet through an Internet service provider that uses CG-NAT
(carrier-grade network address translation), it means that it is not directly
accesible from the Internet: the Raspberry Pi can stablish outgoing connections
but cannot receive incomming connections. So, in order for the alarm4pi web
server to be accessible remotely when it is behind a CG-NAT a reverse tunneling
or other mechanism must be implemented. So, if this is your case, you must
enable one of these options:
  * SocketXP (optional): alarm4pi can use the service of the SocketXP company.
For this, you must create an account in SocketXP. Otherwise, you can disable the
reverse tunneling mechanism of alarm4pi as described below.
  * Tailscale (optional): alarm4pi can use the service of the Tailscale company.
For this, you must create an account in Tailscale. Otherwise, you can disable the
reverse tunneling mechanism of alarm4pi as described below.
* Owncloud (optional): Apart from storing the photos taken locally, they can be
uploaded to a Owncloud server where they can be accessed even if the Raspberry
Pi is disconnected.

## Software prerequisites and depencencies
Before running alarm4pi you must prepare and configure some software
components. The first step is downloading alarm4pi repository.
Then, the GPIO control library is used by alarm4pi, so it must be installed:
``` sudo apt-get install libgpiod-dev ```

Depending on the features that you want to enable more packages should be installed and further configuration performed:

### Pushover (optional)
You must manually configure one notification system so that a message is
sent to your mobile phone when activity is detected. For that, you can
use Pushover (Pushover is a paid service. For a free alternative check
Telegram in the next section). In order to use Pushover:
* Buy the Pushover application and install it in your phone so that you
get a user key.
* Create the file pushover_conf.txt in the project directory with the
following content:
```
server_url=http://api.pushover.net/1/messages.json
token=<token>
user=<your user key>
```
If no notification configuration file is created, the notification
mechanism is disabled.
* Install the development library Curl on your Raspberry by typing:
``` sudo apt-get install libcurl4-openssl-dev ```

### Telegram (optional)
You must manually configure one notification system so that a message is
sent to your mobile phone when activity is detected. For that, you can
previously follow the these steps: 
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
   * You'll receive JSON text file
   * From that text copy the "id" in the "chat" section
   * Create the file telegram_conf.txt in the project directory with the
following content:
```
chat_id=<the ID of your chat with the bot>
api_token=<your bot token>
```
* Install the development library Curl on your Raspberry by typing:
``` sudo apt-get install libcurl4-openssl-dev ```
If no notification configuration file is created, the notification
mechanism is disabled.

### SocketXP (optional)
In case your Raspberry Pi is connected to the Internet through a connection
with CG-NAT you must manually setup a reverse tunneling mechanism. For
example, alarm4pi can use SocketXP (SocketXP is a paid service. For a free
alternative check Tailscale).
To install SocketXP and setup on your Raspberry Pi:
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

### Tailscale (optional)
In case your Raspberry Pi is connected to the Internet through a connection
with CG-NAT you must manually setup a reverse tunneling mechanism. For
example, alarm4pi can use Tailscale. To install Tailscale and setup on your
Raspberry Pi:
* Open a terminal on your Raspberry Pi
* Ensure your Raspberry Pi OS package list is completely fresh.
Run: sudo apt update
* Run the installation script. Run: curl -fsSL https://tailscale.com/install.sh | sh
* Authenticate your device. Run: sudo tailscale up

### Owncloud (optional)
You must have ac account in an Owncloud server in order for the photos to be
uploaded. Moreover, you must manually configure the Owncloud mechanism for
alarm4pi to upload the photos. For that, you must:
* Install the command-line Owncloud application in your Raspberry by typing:
``` sudo apt-get install owncloud-client-cmd ```
sudo apt install cadaver
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
* Install the development library Curl on your Raspberry by typing:
``` sudo apt-get install libcurl4-openssl-dev ```

### Camera
alarm4pi uses the Raspberry Pi camera (connect through a flat cable).
This new version of alarm4pi uses the new camera interface provided by
Raspberry OS 64 bits.

### MJPG-streamer compilation (and test)
alarm4pi uses mjpg-streamer from https://github.com/ArduCAM/mjpg-streamer
As to August 2026 the mjpg-streamer has some bugs in its GitHub repository
version that impedes that it works on current Rasberry OS 64 bits.
However, these bugs has been fixed in the version included in the alarm4pi
project. Anyway mjpg-streamer but must be compiled manually. For that:
* You must install its software dependencies:
```
sudo apt-get install cmake libjpeg62-turbo-dev libcamera-dev
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
./mjpg_streamer -i "input_libcamera.so" -o "output_http.so -w www -p 8008
```
and then browsing the site http://localhost:8008 in the Raspberry Pi browser.

## alarm4pi configuration
As stated previously owncloud_conf.txt, pushover_conf.txt or telegram_conf.txt
file must be created to enable the corresponding component.
Moreover, some configurations must be performed in the source code:
* In alarm4pi.c, change the following line:
```
#define REVERSE_TUNNELING <number>
```
so that <number> is 1 to use SocketXP tunneling, or 2 to use Tailscale
tunneling.
* In gpio_polling.c, change the following line:
```
const int Alarm_gpios[]={<list_of_signals_to_monitor>};
```
where <list_of_signals_to_monitor> is a comma separated list of the follwing
labels depending on what input pins you whant alarm4pi to contuosly check:
PIR_GPIO (for the passive infrared sensor (PIR sensor)), CONTACT1_GPIO (for the
1st electric contact to GND), CONTACT2_GPIO (for the 1st electric contact to
GND).

* In gpio_polling.c, you may want to change the following line:
```
#define OWNCLOUD_REMOTE_DIRECTORY "captures"
```
To set a different remove directory in ownCloud the the captured images through
the Raspberry Pi camera will be uploaded.

## alarm4pi compilation
Before compiling alarm4pi you must install the following dependencies:
* libminiupnpc-dev
* gcc

Since gcc was installed before for mjpg-streamer, you just need to type:
``` sudo apt-get install libminiupnpc-dev ```
and then execute ```make``` in the project directory

## alarm4pi service install ###
The install_service.sh script can be executed to create a systemd service
that will automatically start alarm4pi on boot.
Before running the script open it with a text editor and modify the value
of the ExecStart parameter to indicate the directory where alarm4pi project is.
This script can be executed typing:
``` sudo ./install_services.sh ```
To uninstall the service, run:
``` sudo ./uninstall_services.sh ```

## Log files
alarm4pi creates 2 log files in the log directory.

### daemon.log
In this file alarm4pi registers the actions taken, especially during program
initiallation and termination. If an error occurred, it is reported in this
file so that the user can address it.

### evets.log
In this file alarm4pi registers the alarm events, that is, the activations of
the PIR sensor.

