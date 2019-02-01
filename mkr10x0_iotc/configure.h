// Azure IoT Central device information
static char PROGMEM iotc_scopeId[] = "<replace with IoT Central scope id>";
static char PROGMEM iotc_deviceId[] = "<replace with IoT Central device id>";
static char PROGMEM iotc_deviceKey[] = "<replace with IoT Central device key>";

// Wi-Fi information
static char PROGMEM wifi_ssid[] = "<replace with Wi-Fi SSID>";
static char PROGMEM wifi_password[] = "<replace with Wi-Fi password>";

// comment / un-comment the correct sensor type being used
#define SIMULATE_DHT_TYPE
//#define DHT11_TYPE
//#define DHT22_TYPE

// for DHT11/22, 
//   VCC: 5V or 3V
//   GND: GND
//   DATA: 2
int pinDHT = 2;
