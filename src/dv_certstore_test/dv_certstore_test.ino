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

/* Set up values for your repository and binary names */
#define GHOTA_USER "dwolshin"
#define GHOTA_REPO "dv_hvac_monitor"
#define GHOTA_CURRENT_TAG "1.0.2"
#define GHOTA_BIN_FILE "djw-ota-update-test.ino.nodemcu.bin"
#define GHOTA_ACCEPT_PRERELEASE 0
#include <ESP_OTA_GitHub.h>

// Initialise Update Code
ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

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
 File cert = LittleFS.open("/DJWESP8266.cert.pem", "r");
  clientCert = new X509List(cert, cert.size());
  cert.close();
  
   File key = LittleFS.open("/DJWESP8266.private.key", "r");
  clientKey =new BearSSL::PrivateKey(key, key.size());
  key.close();
  

/*
 //open AWS cert and private key from littleFS 
  File cert = LittleFS.open("/DJWESP8266.cert.pem", "r"); //can be .der file as well
  File key = LittleFS.open("/DJWESP8266.private.key", "r");
cert.readBytes(clientCertStr, cert.size()); //copy certificate from file to char array
  key.readBytes(clientKeyStr, key.size()); //same for private key
   clientCert = new BearSSL::X509List(clientCertStr);
  clientKey = new  BearSSL::PrivateKey(clientKeyStr);  
    
    BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);
  Serial.printf("Attempting to fetch https://github.com/...\n");
  fetchURL(bear, "github.com", 443, "/");  
  delete bear;
  */

  
}

//global for main loop
unsigned long lastPublish;
int msgCount;

void loop() {

  
  //  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  //bear->setCertStore(&certStore);
 
  //pubSubClient.setClient(bear);
  //read in the client cert and private keys and set them on the client
  
  
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
  configTime(3 * 3600, 0, "3.pool.ntp.org", "time.nist.gov");

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


// Try and connect using a WiFiClientBearSSL to specified host:port and dump URL
void fetchURL(BearSSL::WiFiClientSecure *client, const char *host, const uint16_t port, const char *path) {
  if (!path) {
    path = "/";
  }

  Serial.printf("Trying: %s:443...", host);
  client->connect(host, port);
  if (!client->connected()) {
    Serial.printf("*** Can't connect. ***\n-------\n");
    return;
  }
  Serial.printf("Connected!\n-------\n");
  client->write("GET ");
  client->write(path);
  client->write(" HTTP/1.0\r\nHost: ");
  client->write(host);
  client->write("\r\nUser-Agent: ESP8266\r\n");
  client->write("\r\n");
  uint32_t to = millis() + 5000;
  if (client->connected()) {
    do {
      char tmp[32];
      memset(tmp, 0, 32);
      int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
      yield();
      if (rlen < 0) {
        break;
      }
      // Only print out first line up to \r, then abort connection
      char *nl = strchr(tmp, '\r');
      if (nl) {
        *nl = 0;
        Serial.print(tmp);
        break;
      }
      Serial.print(tmp);
    } while (millis() < to);
  }
  client->stop();
  Serial.printf("\n-------\n");
}
