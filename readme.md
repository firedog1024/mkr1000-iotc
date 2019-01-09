# Arduino MKR1000/1010 with Azure IoT Central

## About

Arduino MKR1000 or MKR1010 code sample to send temperature and humidity data to Azure IoT Central.

## Features

* Works with both the Arduino MKR1000 or MKR1010 devices
* Uses a DHT11 or DHT22 sensor for temperature and humidity (no sensor no-problem, temperature and humidity data can be simulated)
* Uses simple MQTT library to communicate to Azure IoT Central
* Simple code base designed to illustrate how the code works and encourage hacking (~400 lines of core code w/ comments)
* IoT Central features supported
  * Telemetry data - Temperature and Humidity
  * Properties - Device sends a die roll number every 15 seconds
  * Settings - Change the fan speed value and see it displayed in the serial moitor and acknowledged back to IoT Central
  * Commands - Send a message to the device and see it displayed as morse code on the device LED

## Installation

Run:

```
git clone https://github.com/firedog1024/mkr1000-iotc.git
```

## Prerequisite


Install the Arduino IDE and the necessary drivers for the Arduino MKR1000 series of boards and ensure that a simple LED blink sketch compiles and runs on the board.  Follow the guide here https://www.arduino.cc/en/Guide/MKR1000

This code requires a couple of libraries to be installed for it to compile.  Depending on if you are using a MKR1000 or MKR1010 board the Wi-Fi libraries are different.  To install an Arduino library open the Arduino IDE and click the "Sketch" menu and then "Include Library" -> "Manage Libraries".  In the dialog filter by the library name below and install the latest version.  For more information on installing libraries with Arduino see https://www.arduino.cc/en/guide/libraries. 

### MKR1000:

* Install library "Wifi101"
* Install library "SimpleDHT"
* Install library "RTCZero"
* Install library "PubSubClient"

### MKR1010:

* Install library "WiFiNINA"
* Install library "SimpleDHT"
* Install library "RTCZero"
* Install library "PubSubClient"

**Note** - We need to increase the payload size limit in PubSubClient to allow for the larger size of MQTT messages from the Azure IoT Hub.  Open the file at %HomePath%\Documents\Arduino\libraries\PubSubClient\src\PubSubClient.h in your favorite code editor.  Change the line (line 26 in current version):

``` C
#define MQTT_MAX_PACKET_SIZE 128
```

to:

``` C
#define MQTT_MAX_PACKET_SIZE 2048
```

Save the file and you have made the necessary fix.  The size probably does not need to be this large but I have not found the crossover point where the size causes a failure.  Fortunately the MKR1000/1010 has a pretty good amount of SRAM (32KB) so we should be ok.

To connect the device to Azure IoT Central you will need to provision an IoT Central application.  This is free for **seven days** but if you already have signed up for an Azure subscription and want to use pay as you go IoT Central is free as long as you have no more than **five devices** and do not exceed **1MB per month** of data.  

Go to https://apps.azureiotcentral.com/ to create an application (you will need to sign in with a Microsoft account identity you may already have one if you use Xbox, office365, Windows 10, or other Microsoft services).  

* Choose Trial or Pay-As-You-Go.
* Select the Sample DevKits template (middle box)
* Provide an application name and URL domain name 
* If you select Pay-As-You-Go you will need to select your Azure subscription and select a region to install the application into.  This information is not needed for Trial.
* Click "Create"

You should now have an IoT Central application provisioned so lets add a real device.  Click Device Explorer on the left.  You will now see three templates in the left hand panel (MXChip, Raspberry Pi, Windows 10 IoT Core).  We are going to use the MXChip template for this exercise to prevent having to create a new template.  Click "MXChip" and click the "+V" icon on the toolbar, this will present a drop down where we click "Real" to add a new physical device.  Give a name to your device and click "Create".  

You now have a device in IoT Central that can be connected to from the Arduino MKR1000/1010 device.  Proceed to wiring and configuration.

## Wiring

![wiring diagram for mkr1000/1010 and DHT11/22](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/mkr1000_dht.png)

From left to right pins on the DHT11/22 sensor:

1. VCC (voltage 3.3v or 5v, MKR uses 3.3V natively)
2. ~2 (data)
3. Not connected
4. GND (ground)

## Configuration

We need to copy some values from our new IoT Central device into the configure.h file so it can connect to IoT Central.  Currently the process is different for the MKR1000 and the MKR1010 devices but hopefully will be aligned soon.  Lets start with the MKR1010 device.  If you have a MKR1000 device jump to the next section titled MKR1000 configuration.

### MKR1010 configuration:
Click the device you created at the end of the Prerequisite step and click the "Connect" link to get the connection information for the device.  We are going to copy "Scope ID', Device ID", and "Primary Key" values into the respective positions in the configure.h file.

``` C
// Azure IoT Central device information
static char PROGMEM iotc_scopeId[] = "<replace with IoT Central scope-id>";
static char PROGMEM iotc_deviceId[] = "<replace with IoT Central device id>";
static char PROGMEM iotc_deviceKey[] = "<replace with IoT Central device key>";
```
Thats it for configuring the MKR1010 specifics, proceed past the MKR1000 configuration section to Common device configuration section.


### MKR1000 configuration:
Due to current issues using the Azure Device Provisioning Service with this device we need to generate an IoT Hub connection string using an external process.  We have a tool called DPS KeyGen that given device and application information can generate a connection string to the IoT Hub.  Lets grab the tool:

```
git clone https://github.com/Azure/dps-keygen.git
```
in the cloned directory there is a bin folder and inside that three folders for the OS's Windows, OSX, and Linux.  Go into the correct folder for your operating system (for Windows you will need to unzip the .zip file in the folder).  Using the command line UX type:

```
cd dps-keygen\bin\windows\dps_cstr
dps_cstr <scope_id> <device_id> <primary_key>
```

for the values in <> substitute in values taken from clicking the device you created at the end of the Prerequisite step and click the "Connect" link to get the connection information for the device.  We can then copy "Scope ID', Device ID", and "Primary Key" values into the respective positions in the command line.

After executing the command above with the substituted values you should see a connection string displayed on the command line.

```
.\dps_cstr 0ne0003D8B4 mkr1000 zyGZtz6r5mqta6p7QXOhlxR1ltgHS0quZPgIYiKb9aE=
...
Registration Information received from service: iotc-fc41f2e1-fc58-40c0-ac56-f7b05c53f70e.azure-devices.net!
Connection String:
HostName=iotc-fc41f2e1-fc58-40c0-ac56-f7b05c53f70e.azure-devices.net;DeviceId=mkr1000;SharedAccessKey=zyGZtz6r5mqta6p7QXOhlxR1ltgHS0quZPgIYiKb9aE=
```

We need to copy this value from the command line to the configure.h file and paste it into the iotConnStr[] line, the resulting line will look something like this.

``` C
static char PROGMEM iotConnStr[] = "HostName=iotc-fc41f2e1-fc58-40c0-ac56-f7b05c53f70e.azure-devices.net;DeviceId=mkr1000;SharedAccessKey=zyGZtz6r5mqta6p7QXOhlxR1ltgHS0quZPgIYiKb9aE=";
```

### Common device configuration:

For both devices you will need to provide the Wi-Fi SSID (Wi-Fi name) and password in the configure.h

``` C
// Wi-Fi information
static char PROGMEM wifi_ssid[] = "<replace with Wi-Fi SSID>";
static char PROGMEM wifi_password[] = "<replace with Wi-Fi password>";
```

Finally we need to tell the code what DHT sensor we are using.  This can be the DHT22 (white), DHT11 (blue), or none and have the code simulate the values.  Comment and uncomment the appropriate lines in configure.h

``` C
// comment / un-comment the correct sensor type being used
//#define SIMULATE_DHT_TYPE
//#define DHT11_TYPE
#define DHT22_TYPE
```

If for any reason you use a different GPIO pin for the data you can also change the pin number in the configure.h file

``` C
// for DHT11/22, 
//   VCC: 5V or 3V
//   GND: GND
//   DATA: 2
int pinDHT = 2;
```

## Compiling and running

Now that you have configured the code with IoT Central, Wi-Fi, and DHT sensor information we are ready to compile and run the code on the device.

Load the mkr10x0_iotc\mkr10x0_iotc.ino file into the Arduino IDE and click the Upload button on the toolbar.  The code should compile and be uploaded to the device.  In the output window you should see:


```
Sketch uses 71816 bytes (27%) of program storage space. Maximum is 262144 bytes.
Atmel SMART device 0x10010005 found
Device       : ATSAMD21G18A
Chip ID      : 10010005
Version      : v2.0 [Arduino:XYZ] Dec 20 2016 15:36:43
Address      : 8192
Pages        : 3968
Page Size    : 64 bytes
Total Size   : 248KB
Planes       : 1
Lock Regions : 16
Locked       : none
Security     : false
Boot Flash   : true
BOD          : true
BOR          : true
Arduino      : FAST_CHIP_ERASE
Arduino      : FAST_MULTI_PAGE_WRITE
Arduino      : CAN_CHECKSUM_MEMORY_BUFFER
Erase flash
done in 0.846 seconds

Write 72472 bytes to flash (1133 pages)

[=                             ] 5% (64/1133 pages)
[===                           ] 11% (128/1133 pages)
[=====                         ] 16% (192/1133 pages)
[======                        ] 22% (256/1133 pages)
[========                      ] 28% (320/1133 pages)
[==========                    ] 33% (384/1133 pages)
[===========                   ] 39% (448/1133 pages)
[=============                 ] 45% (512/1133 pages)
[===============               ] 50% (576/1133 pages)
[================              ] 56% (640/1133 pages)
[==================            ] 62% (704/1133 pages)
[====================          ] 67% (768/1133 pages)
[======================        ] 73% (832/1133 pages)
[=======================       ] 79% (896/1133 pages)
[=========================     ] 84% (960/1133 pages)
[===========================   ] 90% (1024/1133 pages)
[============================  ] 96% (1088/1133 pages)
[==============================] 100% (1133/1133 pages)
done in 0.482 seconds

Verify 72472 bytes of flash with checksum.
Verify successful
done in 0.060 seconds
CPU reset.
```
The code is now running on the device and should be sending data to IoT Central.  We can look at the serial port monitor by clicking the Tool menu -> Serial Monitor (you may need to change the baud rate to 115200).  You should start to see output displayed in the window.  If you are using a MKR1000 and see the following messages output then there is an issue connecting to the IoT Hub via MQTT due to invalid certificates:

```
---> mqtt failed, rc=-2
```

To fix this we need to update the Wi-Fi firmware on the device to the latest version (19.5.4 for the MKR1000).  Follow the instructions here https://www.arduino.cc/en/Tutorial/FirmwareUpdater to update the firmware to the latest version (currently 19.5.4).  Then start this section from the beginning.

### Telemetry:
If the device is working correctly you should see output like this in the serial monitor that indicates data is successfully being transmitted to Azure IoT Central:

```
===> mqtt connected
Current state of device twin:
	{
  "desired": {
    "fanSpeed": {
      "value": 200
    },
    "$version": 16
  },
  "reported": {
    "dieNumber": 1,
    "fanSpeed": {
      "value": 200,
      "statusCode": 200,
      "status": "completed",
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.70}
Sending telemetry ...
	{"temp":   25.10, "humidity":   27.10}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.10, "humidity":   29.60}
--> IoT Hub acknowledges successful receipt of twin property: 1
Sending telemetry ...
	{"temp":   25.60, "humidity":   29.20}
Sending telemetry ...
	{"temp":   25.10, "humidity":   28.20}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.20, "humidity":   28.50}
--> IoT Hub acknowledges successful receipt of twin property: 2
Sending telemetry ...
	{"temp":   25.30, "humidity":   28.40}
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.70}
Sending digital twin property ...
Sending telemetry ...
	{"temp":   25.30, "humidity":   27.30}
--> IoT Hub acknowledges successful receipt of twin property: 3
Sending telemetry ...
	{"temp":   25.20, "humidity":   28.00}
```

Now that we have data being sent lets look at the data in our IoT Central application.  Click the device you created and then select the temperature and humidity telemetry values in the Telemetry column.  You can turn on and off telemetry values by clicking on the eyeballs.  We are only sending temperature and humidity so no other telemetry items will be active.  You should see a screen similar to this:

![telemetry screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/telemetry.png)

### Properties:
The device is also updating the property "Die Number", click on the "Properties" link at the top and you should see the value in the Die Number change about ever 15 seconds.

![properties screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/properties.png)

### Settings:
The device will accept settings and acknowledge the receipt of the setting back to IoT Central.  Go to the "Settings" link at the top and change the value for Fan Speed (RPM), then click the "Update" button the text below the input box will briefly turn red then go green when the device acknowledges receipt of the setting.  In the serial monitor the following should be observed:

```
Fan Speed setting change received with value: 200
{"fanSpeed":{"value":200,"statusCode":200,"status":"completed","desiredVersion":9}}
--> IoT Hub acknowledges successful receipt of twin property: 1
```

The settings screen should look something like this:

![settings screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/settings.png)

### Commands:
We can send a message to the device from IoT Central.  Go to the "Commands" link at the top and enter a message into the Echo - Value to display text box.  The message should consist of only alpha characters (a - z) and spaces, all other characters will be ignored.  Click the "Run" button and watch your device.  You should see the LED blink morse code.  If you enter SOS the led should blink back ...---... where dots are short blinks and dashes slightly longer :-)

![commands screen shot](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/commands.png)

The morse code blinking LED is here on the MKR1000

![morse code blinking LED location](https://github.com/firedog1024/mkr1000-iotc/raw/master/assets/blink_led.png)

## What Now?

You have the basics now go play and hack this code to send other sensor data to Azure IoT Central.  If you want to create a new device template for this you can learn how to do that with this documentation https://docs.microsoft.com/en-us/azure/iot-central/howto-set-up-template.

How about creating a rule to alert when the temperature or humidity exceed a certain value.  Learn about creating rules here https://docs.microsoft.com/en-us/azure/iot-central/tutorial-configure-rules.

For general documentation about Azure IoT Central you can go here https://docs.microsoft.com/en-us/azure/iot-central/.

Have fun!

