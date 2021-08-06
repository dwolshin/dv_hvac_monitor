//Based on example from:
//
// Mar 2018 by Earle F. Philhower, III
// Released to the public domain

#include <ESP8266WiFi.h>
#include <CertStoreBearSSL.h>
#include <time.h>
#include "LittleFS.h"
#include <PubSubClient.h>

//Set only ONE user at a time
#define DJW
//#define VIC
#include "secrets.h" //include MUST come after user definition

#define MAX_PEM_SIZE 4096 // max size of PEM certs

char clientKeyStr[MAX_PEM_SIZE];
char clientCertStr[MAX_PEM_SIZE];

/************************* WiFi Access Point *********************************/
//Set credentials in secrets.h or you can override and set them here
//const char* WLAN_SSID = "Your SSID";
//const char* WLAN_PASS = "Your Password";

//setup for MQTT
void msgReceived(char* topic, byte* payload, unsigned int len);

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

PubSubClient pubSubClient(awsEndpoint, 8883, msgReceived, wifiClient); 


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  LittleFS.begin();
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

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you unzip the connected device package from AWS and and upload the keys/certs to LittleFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }

  //open AWS cert and private key from littleFS and read them into the cert store

  File cert = LittleFS.open("/DJWESP8266.cert.pem", "r"); //can be .der file as well
  File key = LittleFS.open("/DJWESP8266.private.key", "r");
  //read in the client cert and private keys and set them on the client
  cert.readBytes(clientCertStr, cert.size()); //copy certificate from file to char array
  key.readBytes(clientKeyStr, key.size()); //same for private key
  clientCert = new BearSSL::X509List(clientCertStr);
  clientKey = new  BearSSL::PrivateKey(clientKeyStr);

  
}

//global for main loop
unsigned long lastPublish;
int msgCount;

void loop() {
// set the cert store and keys for this connection  
  wifiClient.setClientRSACert(clientCert,clientKey);
  wifiClient.setCertStore(&certStore);
  
  pubSubCheckConnect();

  //publish every 10 seconds
  if (millis() - lastPublish > 10000) {
    String msg = String("Hello from ESP8266: ") + ++msgCount;

    //this is the magic to publish to the feed
    pubSubClient.publish("outTopic", msg.c_str());
    
    Serial.print("Published: "); Serial.println(msg);
    lastPublish = millis();
  }

}



void msgReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on "); Serial.print(topic); Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

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

void setCurrentTime() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

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
  Serial.print("Current time: "); Serial.print(asctime(&timeinfo));
}

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

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
