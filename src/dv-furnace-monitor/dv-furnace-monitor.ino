

/***************************************************
  Adafruit MQTT Library ESP8266 Example
  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino
  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>
#include <TimeLib.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP_EEPROM.h>

/*
// The DHT11 Elegoo module has 3 pins: viewed from top, pins down, left to right - Data, Power, Ground
// The data pin is connected to the Arduino D2
//NOTE for nodeMCU have to specify Dx for the pin, not just x
//#define DHTPIN D2  // modify to the pin we connected


// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT22   // DHT 21 (AM2301)
 */
 
// Only enable one config
#define DJW // Set for Dan's sensors
//#define VIC // Set for Vic's sensors

#ifdef DJW
#define DHTTYPE DHT22   // DHT 21 (AM2301)
#define DHTPIN D2  // modify to the pin we connected
#elif VIC
#define DHTTYPE DHT11
#define DHTPIN 2  // modify to the pin we connected <- remove the D in pin name for generic ESP8266 boards
#endif

DHT dht(DHTPIN, DHTTYPE);


//Name of sensor for  logs
const String SENSORNAME = "DHT11-1";


/************************* WiFi Access Point *********************************/

//#define WLAN_SSID       "...your"
//#define WLAN_PASS       "...your password..."
// WiFi credentials.
#ifdef DJW
const char* WLAN_SSID = "DickStorm";
const char* WLAN_PASS = "!!!3comuse409";
#elif VIC
const char* WLAN_SSID = "odryna";
const char* WLAN_PASS = "dead123456";
#endif

/************************* WiFi Access Point *********************************/
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Glocal Variables that will be used to stamp all logging events
String formattedDate;
String dayStamp;
String timeStamp;
String deviceID;

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
//#define AIO_USERNAME    "...your AIO username (see https://accounts.adafruit.com)..."
//#define AIO_KEY         "...your AIO key..."
#define AIO_USERNAME  "username"
#define AIO_KEY       "aio_key_value"



// The neatest way to access variables stored in EEPROM is using a structure
// NOTE this struct should be pre-populated by the setup script, stored in a seperate PRIVATE repo to hold sensitive info like API keys
struct MyEEPROMStruct {
  String EEPROM_AIO_KEY;
  String EEPROM_AIO_USERNAME;
} eepromVar1;


/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiClientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
//Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, eepromVar1.AIO_USERNAME, eepromVar1.AIO_KEY);
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup feeds for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish onoffbuttonpub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/onoff");
Adafruit_MQTT_Publish dhtpub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dht/json");

// Setup a feed for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbuttonsub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");




/*************************** Sketch Code ************************************/

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(sizeof(MyEEPROMStruct));


  /* Enable Arduino Watchdog Timer:
     valid timer optoins:
     WDTO_250MS, WDTO_500MS
     WDTO_1S, WDTO_2S,WDTO_4S, WDTO_8S
  */
  //  wdt_enable(WDTO_4S);

  Serial.println(); Serial.println();
  Serial.println("System intializing");
  Serial.println("Vic and Dan's HVAC Monitor - R1");

  // Connect to WiFi access point.
  Serial.print("Connecting to "); Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");  // Print dots on the serial console until a connection is made
  }
  Serial.println();
  Serial.println("WiFi connected");
  deviceID = WiFi.macAddress(); // This will be appended to every logging post
  Serial.print("MAC address: "); Serial.println(deviceID);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);  //We're going to post everything to the cloud with a GMT based timestamp
  Serial.println("Contacting NTP server for time update");
  timeClient.forceUpdate();

  // Setup MQTT subscription for onoff feed.
 // mqtt.subscribe(&onoffbuttonsub);

  //init DHT sensor
  Serial.println("Starting DHT Sensor");
  dht.begin();
  //  delay(2000);

  // Access the NTP server, and get Time


  //Added this mqtt_connect to setup, so we can pub the inital ON state
 // MQTT_connect();

  if (! onoffbuttonpub.publish(1)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));

  }

  Serial.println("----------------  End System Startup ---------------------");

}

uint32_t x = 0;

void loop() {
  // Make sure TIME is properly intialized, and kept up to date
  if (timeClient.update()) {
    Serial.println("Time Client Updated");
    setTime(timeClient.getEpochTime());
  }
  else {
    Serial.println("Time Client Update Failed");

  }
  Serial.println("------  New Data Post  --------------");
  Serial.print("Epoch Time (GMT): ");
  Serial.println(timeClient.getEpochTime());  // Note that now() does not work.  The NTP library keeps its own time
  Serial.print("Formatted Time (GMT): ");
  Serial.println(timeClient.getFormattedTime());
  Serial.print(hour()); Serial.print(":"); Serial.print(minute()); Serial.print(":"); Serial.print(second());
  Serial.print(" "); Serial.print(month()); Serial.print("/"); Serial.print(day()); Serial.print("/"); Serial.print(year());
  Serial.println();


  //DynamicJsonBuffer jsonBuffer;
  DynamicJsonDocument jsonDoc(512);

  //add the sensorname, host and time to the JSON doc
  jsonDoc["sensorname"] = SENSORNAME;
  jsonDoc["host"] = deviceID;
  jsonDoc["time"] = timeClient.getEpochTime();

  float ambientT=0, ambientH=0;

  readSensors(ambientT, ambientH);    // Read all of the sensors, each appending to the JSON
  jsonDoc["value"] = ambientT;


  /* Use a JSON lib insead of creating manually
    // We're going to start making a JSON string of a ton of data that we're going to post to a data lake
    String JSON = "{";
    JSON += "\"host\":" + deviceID + "\",\"time\":\"" + now() + "\"";
    readSensors(JSON);    // Read all of the sensors, each appending to the JSON
    JSON += "}";  // Close the string.
    Serial.println(JSON);

    //Convert the string to a char array for publishing

    // Length (with one extra character for the null terminator)
    int str_len = JSON.length() + 1;

    // Prepare the character array (the buffer)
    char char_array[str_len];

    // Copy it over
    JSON.toCharArray(char_array, str_len);
  */

  delay(5000); // Wait 5 seconds between sensor reads

  //pet watchdog
  ////  wdt_reset();

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  ////  MQTT_connect();


  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

/*
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbuttonsub) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbuttonsub.lastread);
    }
  }


*/
  // Now we can publish stuff!

  //Print out the JSON doc to serial
  serializeJson(jsonDoc, Serial);

  //need to convert the doc to a char array for publish to MQTT feed
  char publishData[128];
  int b = serializeJson(jsonDoc, publishData);
  Serial.print("JSON doc in bytes = ");
  Serial.println(b, DEC);

  //Check if onoff feed is ON:

/*
  if (strcmp((char *)onoffbuttonsub.lastread, "ON") == 0) { // check if feed value matchs text string "ON"

    //Publish the JSON to the feed
    if (! dhtpub.publish(publishData)) {
      Serial.println(F("Publish Failed"));
    } else {
      Serial.println(F("Publish OK!"));
    }
    delay(5000);  // wait 5 seconds


  } else if ( (atoi((char *)onoffbuttonsub.lastread)) == 1) { // convert feed value to a number with atoi and test

    //Publish the JSON to the feed
    if (! dhtpub.publish(publishData)) {
      Serial.println(F("Publish Failed"));
    } else {
      Serial.println(F("Publish OK!"));
    }


    delay(5000);  // wait 5 seconds
  } else {
    Serial.println("Feed is off server side - value is:");
    Serial.print((char *)onoffbuttonsub.lastread);
  }
*/

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
    if(! mqtt.ping()) {
    mqtt.disconnect();
    }
  */
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}



// Subroutine that collects the values of all of the sensors. The return is formatted as a JSON string
// The format will look like: {"severity":"info","deviceType":"android"}
void readSensors( float& ambientT, float& ambientH) {

  // Read Ambient Temp and Humidity and add them to the JSON
  ambientT = dht.readTemperature() * 9 / 5 + 32; // Convert to Farenheight

  ambientH = dht.readHumidity();
  Serial.println("Temp: " );
  Serial.print(ambientT);


  //JSON += ",\"ambientT\":\"" + String(ambientT, 1) + "\"";
  //float ambientH = dht.readHumidity();
  //JSON += ",\"ambientH\":\"" + String(ambientH, 0) + "%\"";

}
