/*
// Read temperature and humidity data from an Arduino MKR1000 or MKR1010 device using a DHT11/DHT22 sensor.
// The data is then sent to Azure IoT Central for visualizing via MQTT
//
// See the readme.md for details on connecting the sensor and setting up Azure IoT Central to recieve the data.
*/

#include <stdarg.h>
#include <time.h>
#include <SPI.h>

// are we compiling against the Arduino MKR1000
#if defined(ARDUINO_SAMD_MKR1000) && !defined(WIFI_101)
#include <WiFi101.h>
#define DEVICE_NAME "Arduino MKR1000"
#endif

// are we compiling against the Arduino MKR1010
#ifdef ARDUINO_SAMD_MKRWIFI1010
#include <WiFiNINA.h>
#define DEVICE_NAME "Arduino MKR1010"
#endif

#include <WiFiUdp.h>
#include <RTCZero.h>

/*  You need to go into this file and change this line from:
      #define MQTT_MAX_PACKET_SIZE 128
    to:
      #define MQTT_MAX_PACKET_SIZE 2048
*/
#include <PubSubClient.h>

// change the values for Wi-Fi, Azure IoT Central device, and DHT sensor in this file
#include "./configure.h"

// this is an easy to use NTP Arduino library by Stefan Staub - updates can be found here https://github.com/sstaub/NTP
#include "./ntp.h"
#include "./sha256.h"
#include "./base64.h"
#include "./parson.h"
#include "./morse_code.h"

#include <SimpleDHT.h>

enum dht_type {simulated, dht22, dht11}; 

#if defined DHT22_TYPE
SimpleDHT22 dhtSensor(pinDHT);
dht_type dhtType = dht22;
#elif defined DHT11_TYPE
SimpleDHT11 dhtSensor(pinDHT);
dht_type dhtType = dht11;
#else
dht_type dhtType = simulated;
#endif

String iothubHost;
String deviceId;
String sharedAccessKey;

WiFiSSLClient wifiClient;
PubSubClient *mqtt_client = NULL;

bool timeSet = false;
bool wifiConnected = false;
bool mqttConnected = false;

time_t this_second = 0;
time_t checkTime = 1300000000;

#define TELEMETRY_SEND_INTERVAL 5000  // telemetry data sent every 5 seconds
#define PROPERTY_SEND_INTERVAL  15000 // property data sent every 15 seconds
#define SENSOR_READ_INTERVAL  2500    // read sensors every 2.5 seconds

long lastTelemetryMillis = 0;
long lastPropertyMillis = 0;
long lastSensorReadMillis = 0;

float tempValue = 0.0;
float humidityValue = 0.0;
int dieNumberValue = 1;

// MQTT publish topics
static const char PROGMEM IOT_EVENT_TOPIC[] = "devices/{device_id}/messages/events/";
static const char PROGMEM IOT_TWIN_REPORTED_PROPERTY[] = "$iothub/twin/PATCH/properties/reported/?$rid={request_id}";
static const char PROGMEM IOT_TWIN_REQUEST_TWIN_TOPIC[] = "$iothub/twin/GET/?$rid={request_id}";
static const char PROGMEM IOT_DIRECT_METHOD_RESPONSE_TOPIC[] = "$iothub/methods/res/{status}/?$rid={request_id}";

// MQTT subscribe topics
static const char PROGMEM IOT_TWIN_RESULT_TOPIC[] = "$iothub/twin/res/#";
static const char PROGMEM IOT_TWIN_DESIRED_PATCH_TOPIC[] = "$iothub/twin/PATCH/properties/desired/#";
static const char PROGMEM IOT_C2D_TOPIC[] = "devices/{device_id}/messages/devicebound/#";
static const char PROGMEM IOT_DIRECT_MESSAGE_TOPIC[] = "$iothub/methods/POST/#";

int requestId = 0;
int twinRequestId = -1;

// create a WiFi UDP object for NTP to use
WiFiUDP wifiUdp;
// create an NTP object
NTP ntp(wifiUdp);
// Create an rtc object
RTCZero rtc;

// convert a float to a string as Arduino lacks an ftoa function
char *dtostrf(double value, int width, unsigned int precision, char *result)
{
    int decpt, sign, reqd, pad;
    const char *s, *e;
    char *p;
    s = fcvt(value, precision, &decpt, &sign);
    if (precision == 0 && decpt == 0) {
        s = (*s < '5') ? "0" : "1";
        reqd = 1;
    } else {
        reqd = strlen(s);
        if (reqd > decpt) reqd++;
        if (decpt == 0) reqd++;
    }
    if (sign) reqd++;
    p = result;
    e = p + reqd;
    pad = width - reqd;
    if (pad > 0) {
        e += pad;
        while (pad-- > 0) *p++ = ' ';
    }
    if (sign) *p++ = '-';
    if (decpt <= 0 && precision > 0) {
        *p++ = '0';
        *p++ = '.';
        e++;
        while ( decpt < 0 ) {
            decpt++;
            *p++ = '0';
        }
    }    
    while (p < e) {
        *p++ = *s++;
        if (p == e) break;
        if (--decpt == 0) *p++ = '.';
    }
    if (width < 0) {
        pad = (reqd + width) * -1;
        while (pad-- > 0) *p++ = ' ';
    }
    *p = 0;
    return result;
}

// implementation of printf for use in Arduino sketch
void Serial_printf(char* fmt, ...) {
    char buf[256]; // resulting string limited to 128 chars
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 256, fmt, args);
    va_end (args);
    Serial.print(buf);
}

// simple URL encoder
String urlEncode(const char* msg)
{
    static const char *hex PROGMEM = "0123456789abcdef";
    String encodedMsg = "";

    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
            || ('A' <= *msg && *msg <= 'Z')
            || ('0' <= *msg && *msg <= '9') ) {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

// split the Azure IoT Hub connection string into it's component pieces
void splitConnectionString() {
    String connStr = (String)iotConnStr;
    int hostIndex = connStr.indexOf(F("HostName="));
    int deviceIdIndex = connStr.indexOf(F(";DeviceId="));
    int sharedAccessKeyIndex = connStr.indexOf(F(";SharedAccessKey="));
    iothubHost = connStr.substring(hostIndex + 9, deviceIdIndex);
    deviceId = connStr.substring(deviceIdIndex + 10, sharedAccessKeyIndex);
    sharedAccessKey = connStr.substring(sharedAccessKeyIndex + 17);
}

// get the time from NTP and set the real-time clock on the MKR10x0
void getTime() {
    Serial.println(F("Getting the time from time service: "));

    ntp.begin();
    ntp.update();
    Serial.print(F("Current time: "));
    Serial.print(ntp.formattedTime("%d. %B %Y - "));
    Serial.println(ntp.formattedTime("%A %T"));

    rtc.begin();
    rtc.setEpoch(ntp.epoch());
    timeSet = true;
}

void acknowledgeSetting(const char* propertyKey, const char* propertyValue, int version) {
        // for IoT Central need to return acknowledgement
        const static char* PROGMEM responseTemplate = "{\"%s\":{\"value\":%s,\"statusCode\":%d,\"status\":\"%s\",\"desiredVersion\":%d}}";
        char payload[1024];
        sprintf(payload, responseTemplate, propertyKey, propertyValue, 200, F("completed"), version);
        Serial.println(payload);
        String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
        char buff[20];
        topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
        mqtt_client->publish(topic.c_str(), payload);
        requestId++;
}

void handleDirectMethod(String topicStr, String payloadStr) {
    String msgId = topicStr.substring(topicStr.indexOf("$RID=") + 5);
    String methodName = topicStr.substring(topicStr.indexOf(F("$IOTHUB/METHODS/POST/")) + 21, topicStr.indexOf("/?$"));
    Serial_printf((char*)F("Direct method call:\n\tMethod Name: %s\n\tParameters: %s\n"), methodName.c_str(), payloadStr.c_str());
    if (strcmp(methodName.c_str(), "ECHO") == 0) {
        // acknowledge receipt of the command
        String response_topic = (String)IOT_DIRECT_METHOD_RESPONSE_TOPIC;
        char buff[20];
        response_topic.replace(F("{request_id}"), msgId);
        response_topic.replace(F("{status}"), F("200"));  //OK
        mqtt_client->publish(response_topic.c_str(), "");

        // output the message as morse code
        JSON_Value *root_value = json_parse_string(payloadStr.c_str());
        JSON_Object *root_obj = json_value_get_object(root_value);
        const char* msg = json_object_get_string(root_obj, "displayedValue");
        morse_encodeAndFlash(msg);
        json_value_free(root_value);
    }
}

void handleCloud2DeviceMessage(String topicStr, String payloadStr) {
    Serial_printf((char*)F("Cloud to device call:\n\tPayload: %s\n"), payloadStr.c_str());
}

void handleTwinPropertyChange(String topicStr, String payloadStr) {
    // read the property values sent using JSON parser
    JSON_Value *root_value = json_parse_string(payloadStr.c_str());
    JSON_Object *root_obj = json_value_get_object(root_value);
    const char* propertyKey = json_object_get_name(root_obj, 0);
    double propertyValue;
    double version;
    if (strcmp(propertyKey, "fanSpeed") == 0) {
        JSON_Object* valObj = json_object_get_object(root_obj, propertyKey);
        propertyValue = json_object_get_number(valObj, "value");
        version = json_object_get_number(root_obj, "$version");
        char propertyValueStr[8];
        itoa(propertyValue, propertyValueStr, 10);
        Serial_printf("Fan Speed setting change received with value: %s\n", propertyValueStr);
        acknowledgeSetting(propertyKey, propertyValueStr, version);
    }
    json_value_free(root_value);
}

// callback for MQTT subscriptions
void callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = (String)topic;
    topicStr.toUpperCase();
    String payloadStr = (String)((char*)payload);
    payloadStr.remove(length);

    if (topicStr.startsWith(F("$IOTHUB/METHODS/POST/"))) { // direct method callback
        handleDirectMethod(topicStr, payloadStr);
    } else if (topicStr.indexOf(F("/MESSAGES/DEVICEBOUND/")) > -1) { // cloud to device message
        handleCloud2DeviceMessage(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/PATCH/PROPERTIES/DESIRED"))) {  // digital twin desired property change
        handleTwinPropertyChange(topicStr, payloadStr);
    } else if (topicStr.startsWith(F("$IOTHUB/TWIN/RES"))) { // digital twin response
        int result = atoi(topicStr.substring(topicStr.indexOf(F("/RES/")) + 5, topicStr.indexOf(F("/?$"))).c_str());
        int msgId = atoi(topicStr.substring(topicStr.indexOf(F("$RID=")) + 5, topicStr.indexOf(F("$VERSION=")) - 1).c_str());
        if (msgId == twinRequestId) {
            // twin request processing
            twinRequestId = -1;
            // output limited to 128 bytes so this output may be truncated
            Serial_printf((char*)F("Current state of device twin:\n\t%s"), payloadStr.c_str());
            Serial.println();
        } else {
            if (result >= 200 && result < 300) {
                Serial_printf((char*)F("--> IoT Hub acknowledges successful receipt of twin property: %d\n"), msgId);
            } else {
                Serial_printf((char*)F("--> IoT Hub could not process twin property: %d, error: %d\n"), msgId, result);
            }
        }
    } else { // unknown message
        Serial_printf((char*)F("Unknown message arrived [%s]\nPayload contains: %s"), topic, payloadStr.c_str());
    }
}

// connect to Azure IoT Hub via MQTT
void connectMQTT(String deviceId, String username, String password) {
    mqtt_client->disconnect();

    Serial.println(F("Starting IoT Hub connection"));
    int retry = 0;
    while(retry < 10 && !mqtt_client->connected()) {     
        if (mqtt_client->connect(deviceId.c_str(), username.c_str(), password.c_str())) {
                Serial.println(F("===> mqtt connected"));
                mqttConnected = true;
        } else {
            Serial.print(F("---> mqtt failed, rc="));
            Serial.println(mqtt_client->state());
            delay(2000);
            retry++;
        }
    }
}

// create an IoT Hub SAS token for authentication
String createIotHubSASToken(char *key, String url, long expire){
    url.toLowerCase();
    String stringToSign = url + "\n" + String(expire);
    int keyLength = strlen(key);

    int decodedKeyLength = base64_dec_len(key, keyLength);
    char decodedKey[decodedKeyLength];

    base64_decode(decodedKey, key, keyLength);

    Sha256 *sha256 = new Sha256();
    sha256->initHmac((const uint8_t*)decodedKey, (size_t)decodedKeyLength);
    sha256->print(stringToSign);
    char* sign = (char*) sha256->resultHmac();
    int encodedSignLen = base64_enc_len(HASH_LENGTH);
    char encodedSign[encodedSignLen];
    base64_encode(encodedSign, sign, HASH_LENGTH);
    delete(sha256);

    return (char*)F("SharedAccessSignature sr=") + url + (char*)F("&sig=") + urlEncode((const char*)encodedSign) + (char*)F("&se=") + String(expire);
}

// reads the value from the DHT sensor if present else generates a random value
void readSensors() {
    dieNumberValue = random(1, 7);

    #if defined DHT11_TYPE || defined DHT22_TYPE
    int err = SimpleDHTErrSuccess;
    if ((err = dhtSensor.read2(&tempValue, &humidityValue, NULL)) != SimpleDHTErrSuccess) {
        Serial_printf("Read DHT sensor failed (Error:%d)", err); 
        tempValue = -999.99;
        humidityValue = -999.99;
    }
    #else
    tempValue = random(0, 7500) / 100.0;
    humidityValue = random(0, 9999) / 100.0;
    #endif
}

void setup() {
    Serial.begin(115200);
 
    Serial_printf((char*)F("Hello, starting up the %s device\n"), DEVICE_NAME);

    // seed pseudo-random number generator for die roll and simulated sensor values
    randomSeed(millis());

    // get the hostname, deviceId, sharedAccessKey from the connection string
    splitConnectionString();

    // attempt to connect to Wifi network:
    Serial.print((char*)F("WiFi Firmware version is "));
    Serial.println(WiFi.firmwareVersion());
    int status = WL_IDLE_STATUS;
    while ( status != WL_CONNECTED) {
        Serial_printf((char*)F("Attempting to connect to Wi-Fi SSID: %s \n"), wifi_ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(wifi_ssid, wifi_password);
        delay(1000);
    }

    // get current UTC time
    getTime();

    // create SAS token and user name for connecting to MQTT broker
    String url = iothubHost + urlEncode(String((char*)F("/devices/") + deviceId).c_str());
    char *devKey = (char *)sharedAccessKey.c_str();
    long expire = rtc.getEpoch() + 864000;
    String sasToken = createIotHubSASToken(devKey, url, expire);
    String username = iothubHost + "/" + deviceId + (char*)F("/api-version=2016-11-14");

    // connect to the IoT Hub MQTT broker
    wifiClient.connect(iothubHost.c_str(), 8883);
    mqtt_client = new PubSubClient(iothubHost.c_str(), 8883, wifiClient);
    connectMQTT(deviceId, username, sasToken);
    mqtt_client->setCallback(callback);

    // add subscriptions
    mqtt_client->subscribe(IOT_TWIN_RESULT_TOPIC);  // twin results
    mqtt_client->subscribe(IOT_TWIN_DESIRED_PATCH_TOPIC);  // twin desired properties
    String c2dMessageTopic = IOT_C2D_TOPIC;
    c2dMessageTopic.replace(F("{device_id}"), deviceId);
    mqtt_client->subscribe(c2dMessageTopic.c_str());  // cloud to device messages
    mqtt_client->subscribe(IOT_DIRECT_MESSAGE_TOPIC); // direct messages

    // request full digital twin update
    String topic = (String)IOT_TWIN_REQUEST_TWIN_TOPIC;
    char buff[20];
    topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
    twinRequestId = requestId;
    requestId++;
    mqtt_client->publish(topic.c_str(), "");

    // initialize timers
    lastTelemetryMillis = millis();
    lastPropertyMillis = millis();
}

// main processing loop
void loop() {
    // give the MQTT handler time to do it's thing
    mqtt_client->loop();

    // read the sensor values
    if (millis() - lastSensorReadMillis > SENSOR_READ_INTERVAL) {
        readSensors();
        lastSensorReadMillis = millis();
    }
    
    // send telemetry values every 5 seconds
    if (mqtt_client->connected() && millis() - lastTelemetryMillis > TELEMETRY_SEND_INTERVAL) {
        Serial.println(F("Sending telemetry ..."));
        String topic = (String)IOT_EVENT_TOPIC;
        topic.replace(F("{device_id}"), deviceId);
        char buff[10];
        String payload = F("{\"temp\": {temp}, \"humidity\": {humidity}}");
        payload.replace(F("{temp}"), dtostrf(tempValue, 7, 2, buff));
        payload.replace(F("{humidity}"), dtostrf(humidityValue, 7, 2, buff));
        Serial_printf("\t%s\n", payload.c_str());
        mqtt_client->publish(topic.c_str(), payload.c_str());

        lastTelemetryMillis = millis();
    }

    // send a property update every 15 seconds
    if (mqtt_client->connected() && millis() - lastPropertyMillis > PROPERTY_SEND_INTERVAL) {
        Serial.println(F("Sending digital twin property ..."));

        String topic = (String)IOT_TWIN_REPORTED_PROPERTY;
        char buff[20];
        topic.replace(F("{request_id}"), itoa(requestId, buff, 10));
        String payload = F("{\"dieNumber\": {dieNumberValue}}");
        payload.replace(F("{dieNumberValue}"), itoa(dieNumberValue, buff, 10));

        mqtt_client->publish(topic.c_str(), payload.c_str());
        requestId++;

        lastPropertyMillis = millis();
    }
}
