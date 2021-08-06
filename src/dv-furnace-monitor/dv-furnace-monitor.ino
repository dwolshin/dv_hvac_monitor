//main sketch
//OTA update enabled
//Posting live DHT sensor data to AWS IOT

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ESP8266WiFi.h>
#include <CertStoreBearSSL.h>
#include <time.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"
//Set only ONE user at a time
#define DJW
//#define VIC
#include "secrets.h" //include MUST come after user definition


/*********************** DHT Setup*********************************/
  
// The DHT11 Elegoo module has 3 pins: viewed from top, pins down, left to right - Data, Power, Ground
// The data pin is connected to the Arduino D2
//NOTE for nodeMCU have to specify Dx for the pin, not just x
//#define DHTPIN D2  // modify to the pin we connected
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT22   // DHT 21 (AM2301)

 
#ifdef DJW
  #define DHTTYPE DHT22   // DHT 21 (AM2301)
  #define DHTPIN D2  // modify to the pin we connected
  const String SENSORNAME = "DHT22";
#elif VIC
  #define DHTTYPE DHT11
  #define DHTPIN 2  // modify to the pin we connected <- remove the D in pin name for generic ESP8266 boards
  //Name of sensor for  logs
  const String SENSORNAME = "DHT11-1";
#endif
DHT dht(DHTPIN, DHTTYPE);


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
const char* awsEndpoint = "an1v1kdoueaep-ats.iot.us-west-2.amazonaws.com";

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


/* Set up values for your repository and binary names */
#define GHOTA_USER "dwolshin"
#define GHOTA_REPO "dv_hvac_monitor"
#define GHOTA_CURRENT_TAG "1.0.2"
#define GHOTA_BIN_FILE "djw-ota-update-test.ino.nodemcu.bin"
#define GHOTA_ACCEPT_PRERELEASE 0
#include <ESP_OTA_GitHub.h>

// Initialise OTA Update Code
ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);



/* SKETCH SETUP SSSSSSSSSSSSSSSSSS
 S  
 S  
 */
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  //init the CA cert store from flash
  LittleFS.begin();
   int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you unzip the connected device package from AWS and and upload the keys/certs to LittleFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
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
 File cert = LittleFS.open("/DJWESP8266.cert.pem", "r");
  clientCert = new X509List(cert, cert.size());
  cert.close();
  
   File key = LittleFS.open("/DJWESP8266.private.key", "r");
  clientKey =new BearSSL::PrivateKey(key, key.size());
  key.close();

  //init DHT sensor
  Serial.println("Starting DHT Sensor");
  dht.begin();
    
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
  if (millis() - lastLoopDelay > 10000) {
        
    // set the cert store and keys for this connection  
     wifiClient.setClientRSACert(clientCert,clientKey);
     wifiClient.setCertStore(&certStore);
      
      pubSubCheckConnect();
    
       //DynamicJsonBuffer jsonBuffer;
      DynamicJsonDocument jsonDoc(512);
    
      //add the sensorname, host and time to the JSON doc
      jsonDoc["sensorname"] = SENSORNAME;
      jsonDoc["host"] = "DJWESP8266";
      
      epochTime = getTime();
      
      jsonDoc["time"] = epochTime;
     JsonObject jsonObj = jsonDoc.as<JsonObject>(); //jsonObj is a reference for passing around jsonDoc to write sensor data



      /****************
       * 
       * Add in new snsor readings here
       */
      //read a sensor, pass in the jsonObj 

      readDHT(jsonObj); //read a DHT sensor



     //Print out the jsonDoc to Serial before publishing     
     //serializeJson(jsonDoc, Serial);
    
      //need to convert the doc to a char array for publish to MQTT feed
      char publishData[128];
      int b = serializeJson(jsonDoc, publishData);
      Serial.print("JSON doc in bytes = ");
      Serial.println(b, DEC);
        
      //this is the magic to publish to the feed
      pubSubClient.publish("/sensors/dht", publishData);
     Serial.print("Published: "); Serial.println(publishData);
     
     //update the last publish time
     lastLoopDelay = millis();
  }
  

}


/* readSensors ************************
 *  
 *  
 */
// Subroutine that collects the values of all of the sensors.
//Pass in the json Object by ref
void readDHT(JsonObject &jsonObj) {

  //local vars to hold sensor data
  float ambientT, ambientH;
      
  // DHT Read Ambient Temp and Humidity and add them to the JSON
  ambientT = dht.readTemperature() * 9 / 5 + 32; // Convert to Farenheight

  ambientH = dht.readHumidity();

  
//store data in the json doc in the key = value format below
      jsonObj["temp"] = ambientT;
      jsonObj["humidty"] = ambientH;
     
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
 *  
 *   
 */
void msgReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on "); Serial.print(topic); Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/* pubSubCheckConnect ***********************
 *   
 *  
 */
void pubSubCheckConnect() {
  if ( ! pubSubClient.connected()) {
    Serial.print("PubSubClient connecting to: "); Serial.print(awsEndpoint);
    while ( ! pubSubClient.connected()) {
      Serial.print(".");
      pubSubClient.connect("DJWESP8266");
    }
    Serial.println(" connected");
    pubSubClient.subscribe("/");
  }
  pubSubClient.loop();
}

/*setClock
 * Set time via NTP, as required for x.509 validation
 * 
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

/****
*Includes code from example:
* Mar 2018 by Earle F. Philhower, III
* Released to the public domain
*
*
 * Includes code from example
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
