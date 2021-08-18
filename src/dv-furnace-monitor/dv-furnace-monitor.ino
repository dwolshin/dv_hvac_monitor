//main sketch
//OTA update enabled
//Posting live DHT sensor data to AWS IOT

#define ARDUINOJSON_USE_LONG_LONG 1

#include <ESP8266WiFi.h> //ESP8266WiFi at version 1.0 from ESPcore 3.0.2
#include <LittleFS.h> //LittleFS at version 0.1.0 from core 3.0.2
#include <PubSubClient.h> // PubSubClient at version 2.8 
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include <NTPClient.h> // NTPClient at version 3.2.0
#include <WiFiUdp.h>  // part of ESP8266wifi 1.0 from ESP core 3.0.2
#include <ArduinoJson.h> //ArduinoJson at version 6.18.3
#include <OneWire.h> //Used by the Dallas Temp library
#include <DallasTemperature.h>  // Library for the OneWire Dallas Semi-based temp probe

//Set only ONE user at a time
//#define DJW
#define VIC
//Include MUST come after user definition - contains secrets for your local setup
#include "secrets.h" // <- do NOT check in this file!!! 

//This sets the max payload size of the MQTT messages
#define MAX_JSON_BUFFER_SIZE 1500
//Setting the max JSON DOC size to be the same as the MQTT message size, but they are NOT the same
#define MAX_JSON_DOC_SIZE 1500


/*******************************************************************
   DJW
*/
#ifdef DJW

const char thingLocation[] =  "mainStreet";
const char thingName[] = "djw-main-03";
const String certFileName = "/DJW-MAIN-01-certificate.pem.crt";
const String keyFileName = "/DJW-MAIN-01-private.pem.key";

//#define DHT_ENABLED
#ifdef DHT_ENABLED
#define DHTTYPE DHT22   // DHT 21 (AM2301)
#define DHTPIN D5  // modify to the pin we connected
#endif //DHT_ENABLED

/*************************************************************/
//Set local user/device config here
#define pollInterval 10000  //How often to check the sensors, in millseconds

#define ONEWIRE_ENABLED
#ifdef ONEWIRE_ENABLED
//Setup the OneWire connection to the thermal probes
/********************************************************************/
// Data wire from the DS18B20 is plugged into pin 2 on the Arduino
#define dallasTempProbeOneWireBus D2
/********************************************************************/
#define TEMPERATURE_PRECISION 9 // Lower resolution
int numberOfDTDevices = 0;
#endif //ONEWIRE_ENABLED

//#define MIC_ENABLED
#ifdef MIC_ENABLED
//Define the microphone pin
int micSensorPin = A0;    // select the input pin for microphone
#define aInName "microphone"
#endif //MIC_ENABLED

/*

  #define DHTTYPE DHT22   // DHT 21 (AM2301)
  #define DHTPIN D2  // modify to the pin we connected

  const String thingLocation = "mainStreet";
  const String thingName = "djwMain01";
  const String certFileName = "/DJW-MAIN-01-certificate.pem.crt";
  const String keyFileName = "/DJW-MAIN-01-private.pem.key";

*/
#endif //DJW

/*******************************************************************
   VIC
*/

#ifdef VIC
//Name of sensor and location for  logs
const char thingLocation[] = "GoldenEagle";
const char  thingName[] = "VicGE-1";
const String certFileName = "/VicGE-1-certificate.pem.crt";
const String keyFileName = "/VicGE-1-private.pem.key";


//#define DHT_ENABLED
#ifdef DHT_ENABLED
#define DHTTYPE DHT22   // DHT 21 (AM2301)
#define DHTPIN D5  // modify to the pin we connected
#endif //DHT_ENABLED

/*************************************************************/
//Set local user/device config here
#define pollInterval 10000  //How often to check the sensors, in millseconds


//#define OPTOCOUPLER_ENABLED
#ifdef OPTOCOUPLER_ENABLED
//These first six sensors are optocouplers that are sensing the state of 26VAC HVAC calls
//The optocoupers provide an active low digital signal, connected directly to an I/O pin
#define dInApin  15
#define dInAName "zonesDemand"      //Active when ANY of the 9 zones are calling
#define dInBpin  14
#define dInBName "boilerDemand"     //Active when the mixer controller says we need heat
#define dInCpin  13
#define dInCName "DHWDemand"        //Active when we need to heat water
#define dInDpin  12
#define dInDName "snowMeltDemand"   //Active when the front walk needs melting
#define dInEpin  5
#define dInEName "boiler1Activate"   //Active when Boiler 1 is asked to fire
#define dInFpin  4
#define dInFName "boiler2Activate"   //Active when Boiler 2 is asked to fire
//initialize the I/O pins that we're using.  These all read a boolean value
pinMode(LED_BUILTIN, OUTPUT);
pinMode(dInApin, INPUT);
pinMode(dInBpin, INPUT);
pinMode(dInCpin, INPUT);
pinMode(dInDpin, INPUT);
pinMode(dInEpin, INPUT);
pinMode(dInFpin, INPUT);
#endif //OPTOCOUPLER_ENABLED

#define ONEWIRE_ENABLED
#ifdef ONEWIRE_ENABLED
//Setup the OneWire connection to the thermal probe attached to the boiler loop pipes
/********************************************************************/
// Data wire from the DS18B20 is plugged into pin 2 on the Arduino
#define dallasTempProbeOneWireBus D2
/********************************************************************/
#define TEMPERATURE_PRECISION 9 // Lower resolution
int numberOfDTDevices = 0;

#endif //ONEWIRE_ENABLED

//Define the microphone pin
int micSensorPin = A0;    // select the input pin for microphone
#define aInName "microphone"


//initialize the OneWire pin that we'll use for the temp probe
//initialize the Analog input pin that we're using for the microphone

#endif //VIC

DHTesp dht;


//these are for holding the AWS private key and cert, stored in flash
#define MAX_PEM_SIZE 4096 // max size of PEM certs
char clientKeyStr[MAX_PEM_SIZE];
char clientCertStr[MAX_PEM_SIZE];

/************************* WiFi Access Point *********************************/
//Set credentials in secrets.h or you can override and set them here
//const char* WLAN_SSID = "Your SSID";
//const char* WLAN_PASS = "Your Password";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.pool.ntp.org");


// Find this awsEndpoint in the AWS Console: Manage - Things, choose your thing
// choose Interact, its the HTTPS Rest endpoint
// This was Dan's const char* awsEndpoint = "an1v1kdoueaep-ats.iot.us-west-2.amazonaws.com";
// Vic's endpoint for VicGE-1
const char* awsEndpoint = "a2kuhsw8cpcus8-ats.iot.us-west-2.amazonaws.com";

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
BearSSL::CertStore certStore;
BearSSL::WiFiClientSecure wifiClient;
BearSSL::X509List *clientCert;
BearSSL::PrivateKey *clientKey;
//setup to listen for MQTT messages
void msgReceived(char* topic, byte* payload, unsigned int len);
//MQTT pubsub client
PubSubClient pubSubClient(awsEndpoint, 8883, msgReceived, wifiClient);

#ifdef ONEWIRE_ENABLED
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire dallasTempOneWire(dallasTempProbeOneWireBus);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature dallasTempSensors(&dallasTempOneWire);

#endif // ONEWIRE_ENABLED

/* Set up values for your repository and binary names */
#define GHOTA_USER "dwolshin"
#define GHOTA_REPO "dv_hvac_monitor"
#define GHOTA_CURRENT_TAG "1.0.4"
#define GHOTA_BIN_FILE "djw-ota-update-test.ino.nodemcu.bin"
#define GHOTA_ACCEPT_PRERELEASE 0
#include <ESP_OTA_GitHub.h> // ESP_OTA_GitHub at version 0.0.3




/* SKETCH SETUP SSSSSSSSSSSSSSSSSS
  S
  S
*/
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  //init the CA cert store from flash
  LittleFS.begin();
  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("ERROR: No certs found. Did you unzip the connected device package from AWS and and upload the keys/certs to LittleFS directory before running?\n");
    delay(50000);
    ESP.restart();// Can't connect to anything w/o certs!
  }

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  setClock(); // Required for X.509 validation
  
  // Initialise Update Code
  //We do this locally so that the memory used is freed when the function exists.
  ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);

  /* This is the actual code to check and upgrade */
  Serial.println("Checking for update...");
  if (ESPOTAGitHub.checkUpgrade()) {
    Serial.print("Upgrade found at: ");
    Serial.println(ESPOTAGitHub.getUpgradeURL());
    if (ESPOTAGitHub.doUpgrade()) {
      Serial.println("Upgrade complete."); //This should never be seen as the device should restart on successful upgrade.
    } else {
      Serial.print("Unable to upgrade: ");
      Serial.println(ESPOTAGitHub.getLastError());
    }
  } else {
    Serial.print("Not proceeding to upgrade: ");
    Serial.println(ESPOTAGitHub.getLastError());
  }
  /* End of check and upgrade code */

    //load the certificates and private key for talking to AWS
    File cert = LittleFS.open( certFileName, "r");
    int certSize = cert.size();
    clientCert = new X509List(cert, certSize);
    Serial.print("Cert size is:");
    Serial.println(certSize);
    cert.close();
  
    File key = LittleFS.open(keyFileName, "r");
    int keySize = key.size();
    clientKey = new BearSSL::PrivateKey(key, keySize);
    Serial.print("Client Key size is:");
    Serial.println(keySize);
    key.close();
  
    if (keySize == 0 || certSize == 0 ) {
      Serial.printf("ERROR: Cert or Key size is zero - did you upload the right keys/certs to LittleFS directory before running?\n");
      delay(50000);
  
      ESP.restart();// Can't connect to anything w/o certs!
    }

  #ifdef DHT_ENABLED
      /*****DISABLED - no sensor */
      dht.setup(DHTPIN, DHTesp::DHT22);
  #endif //DHT_ENABLED

  #ifdef ONEWIRE_ENABLED
      // Start up the library
      dallasTempSensors.begin();
    
      // Grab a count of devices on the wire
     numberOfDTDevices = dallasTempSensors.getDeviceCount();
      DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address
    
      // locate devices on the bus
      Serial.print("Locating devices...");
      Serial.print("Found ");
      Serial.print(dallasTempSensors.getDeviceCount(), DEC);
      Serial.println(" devices.");
    
      // report parasite power requirements
      Serial.print("Parasite power is: ");
      if (dallasTempSensors.isParasitePowerMode()) Serial.println("ON");
      else Serial.println("OFF");
    


    // Loop through each device, print out address
    for (int i = 0; i < numberOfDTDevices; i++)
    {
      // Search the wire for address
      if (dallasTempSensors.getAddress(tempDeviceAddress, i))
      {
        Serial.print("Found device ");
        Serial.print(i, DEC);
        Serial.print(" with address: ");
        printAddress(tempDeviceAddress);
        Serial.println();
    
        Serial.print("Setting resolution to ");
        Serial.println(TEMPERATURE_PRECISION, DEC);
    
        // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
        dallasTempSensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
    
        Serial.print("Resolution actually set to: ");
        Serial.print(dallasTempSensors.getResolution(tempDeviceAddress), DEC);
        Serial.println();
      } else {
        Serial.print("Found ghost device at ");
        Serial.print(i, DEC);
        Serial.print(" but could not detect address. Check power and cabling");
      }
    }
  #endif //ONEWIRE_ENABLED
}

//global for main loop
unsigned long lastLoopDelay;
// Variable to save current epoch time
unsigned long epochTime;

/* MAIN LOOP MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  M
  M
*/
void loop() {

  //Don't block the main loop, but check elapsed time and only run the contense of this block ever 10s
  if (millis() - lastLoopDelay > pollInterval) {

    Serial.print("--->Free Memeory top of loop: "); Serial.println(ESP.getFreeHeap(), DEC);

    // set the cert store and keys for this connection
    wifiClient.setClientRSACert(clientCert, clientKey);
    wifiClient.setCertStore(&certStore);

    //Check/maintain status of the MQTT connection
    pubSubCheckConnect();

    Serial.print("--->Free Memeory after pubsub connect:"); Serial.println(ESP.getFreeHeap(), DEC);



    //Create the JSON document for holding sensor data
    DynamicJsonDocument jsonDoc(MAX_JSON_DOC_SIZE);
    //jsonObj is a reference for passing around jsonDoc to write sensor data
    //JsonObject jsonObj = jsonDoc.as<JsonObject>();
    // Create the header that goes into every MQTT post
    jsonDoc["eventType"] = "scheduledPoll";
    jsonDoc["location"] = thingLocation;
    jsonDoc["thingName"] = thingName;
    jsonDoc["severity"] = "info";
    epochTime = getTime();
    jsonDoc["time"] = epochTime;
    jsonDoc["pollInterval"] = pollInterval;
    jsonDoc["version"] = GHOTA_CURRENT_TAG;

    Serial.print("--->Free Memeory after JSON Doc: "); Serial.println(ESP.getFreeHeap(), DEC);

    /****************

       Add in new sensor readings here
    */
    //read a sensor, pass in the jsonObj
#ifdef DHT_ENABLED
    readDHT(jsonDoc); //read a DHT sensor
#endif //DHT_ENABLED

#ifdef ONEWIRE_ENABLED
    // Read the Dallas temp based probe connected to the circ pipes
    DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

    dallasTempSensors.requestTemperatures(); 

     // Loop through each device, print out address
    for (int i = 0; i < numberOfDTDevices; i++)
    {
      // Search the wire for address
      if (dallasTempSensors.getAddress(tempDeviceAddress, i))
      {
        Serial.print("Found device ");
        Serial.print(i, DEC);
        Serial.print(" with address: ");
        printAddress(tempDeviceAddress);
        Serial.println();
    
    // Send the command to get temperature readings
    // You can have more than one DS18B20 on the same bus.
    // 0 refers to the first IC on the wire
    
   // float dtempf = DallasTemperature::toFahrenheit(dallasTempSensors.getTempCByIndex(0));

    float dtempF = dallasTempSensors.getTempF(tempDeviceAddress);
    Serial.print("Temp in F: ");
    Serial.println(dtempF);
    
     char ftempName[128];
    sprintf(ftempName,"dallasTempSensor%d", i );
   Serial.print("Sensor Name is: ");
    Serial.println(ftempName);

    jsonDoc[ftempName] = dtempF;
      }
    }
    
#endif // ONEWIRE_ENABLED

#ifdef MIC_ENABLED
    // Read the microphone value, connected to the Analog Input pin
    jsonDoc[aInName] = analogRead(micSensorPin);
#endif //MIC_ENABLED
    //Check for errors and publish the sensor data
    publishToFeed(jsonDoc);

    //update the last publish time
    lastLoopDelay = millis();
    Serial.print("--->Free Memeory end of loop: "); Serial.println(ESP.getFreeHeap(), DEC);
  }


}

/*publishToFeed *************************

   MQTT post function with size validation and error checking
*/

void publishToFeed(DynamicJsonDocument &jsonDoc) {

  /* DEBUG*****************************
    // Fill up the doc with some test data
    for (int i = 0; i < 40; i++) {
      String s = "Value" + String(i);
      //jsonDoc[s] = "short value";
       jsonDoc[s] = epochTime;
    }
  */

  //holds the size of the Serialized JSON doc in bytes
  int jdocSize = measureJson(jsonDoc);

  /*DEBUG*****************************
    //Print out the jsonDoc to Serial before publishing
    serializeJson(jsonDoc, Serial);
  */

  Serial.print("JSON buffer size:");
  Serial.println(jsonDoc.memoryUsage());
  Serial.print("Size of Serialized JSON data: ");
  Serial.println(jdocSize);

  if ( jsonDoc.overflowed()) {
    Serial.print("ERROR: jsonDoc memory pool was too small - some values are missing from the JsonDocument!!!");
  }

  //check if the Serialized JSON doc is larger than the MAX MQTT payload size
  if (jdocSize < MAX_JSON_BUFFER_SIZE) {
    //need to convert the doc to a char array for publish to MQTT feed
    char publishData[MAX_JSON_BUFFER_SIZE];
    int b = serializeJson(jsonDoc, publishData);
    Serial.print("JSON doc in bytes = ");
    Serial.println(b, DEC);

    //this is the magic to publish to the feed

    char feedName[128];
    sprintf(feedName, "/%s/%s", thingLocation , thingName );
    Serial.print("Topic name is:");
    Serial.println(feedName);

    bool rc  = pubSubClient.publish(feedName , publishData);

    //check if publish was successfull or not
    if (rc = true) {
      Serial.print("Published: "); Serial.println(publishData);
    } else {
      Serial.println("ERROR: MQTT publish failed, either connection lost or message too large!!!");
    }
  } else {
    Serial.print("ERROR: Size of Serialized JSON:");
    Serial.print(jdocSize);
    Serial.print("b exceeds max MQTT message size: ");
    Serial.print(MAX_JSON_BUFFER_SIZE);
  }
}

/* readSensors ************************
   THIS IS COMMENTED OUT ALTOGETHER. IT SEEMED CLEANER (in my case) TO SIMPLY DO IT IN MAIN

*/

// Subroutine that collects the values of all of the sensors.
//Pass in the json Object by ref
void readDHT(DynamicJsonDocument &jsonDoc) {

  //local vars to hold sensor data
  float ambientT, ambientH;

  // DHT Read Ambient Temp and Humidity and add them to the JSON
  //ambientT = dht.readTemperature() * 9 / 5 + 32; // Convert to Farenheight

  ambientT = dht.getTemperature() * 9 / 5 + 32; // Convert to Farenheight
  ambientH = dht.getHumidity();


  //store data in the json doc in the key = value format below
  jsonDoc["temp"] = ambientT;
  jsonDoc["humidty"] = ambientH;

  Serial.print("Temp: " );
  Serial.println(ambientT);

  Serial.print("Humidity: " );
  Serial.println(ambientH);

}


// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

/* msgReceived ***********************


*/
void msgReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on "); Serial.print(topic); Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  if (strncmp(topic, "/forceupdate", sizeof(topic)) == 0) {

    Serial.println();
    Serial.println("!!!Force update message received!!!");
    Serial.println();
    Serial.print("--->Free Memeory During Force Update: "); Serial.println(ESP.getFreeHeap(), DEC);

    pubSubClient.disconnect();

    ESP.restart();// Force update check

  }

}

/* pubSubCheckConnect ***********************


*/
void pubSubCheckConnect() {
  pubSubClient.setBufferSize(MAX_JSON_BUFFER_SIZE); //set the client buffer size, default of 128b is too small

  if ( ! pubSubClient.connected()) {
    Serial.print("PubSubClient connecting to: "); Serial.print(awsEndpoint);
    while ( ! pubSubClient.connected()) {
      Serial.print(".");
      pubSubClient.connect(thingName);
    }
    Serial.println(" connected");
    pubSubClient.subscribe("/forceupdate");
  }
  pubSubClient.loop();
}

/*setClock
   Set time via NTP, as required for x.509 validation

*/
void setClock() {
  configTime(3 * 3600, 0, "0.pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

/****
  Includes code from example:
  Mar 2018 by Earle F. Philhower, III
  Released to the public domain


   Includes code from example
  WiFiClientBearSSL- SSL client/server for esp8266 using BearSSL libraries
  - Mostly compatible with Arduino WiFi shield library and standard
    WiFiClient/ServerSecure (except for certificate handling).
  Copyright (c) 2018 Earle F. Philhower, III
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
