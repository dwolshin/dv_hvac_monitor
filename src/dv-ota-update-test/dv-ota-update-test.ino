#include <ESP8266WiFi.h>
#include <FS.h>
#include "secrets.h"
#include <LittleFS.h>
/************************* WiFi Access Point *********************************/

//#define WLAN_SSID       "...your"
//#define WLAN_PASS       "...your password..."
// WiFi credentials.
const char* WLAN_SSID = "DickStorm";
const char* WLAN_PASS = "!!!3comuse409";

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;

/* Set up values for your repository and binary names */
#define GHOTA_USER "dwolshin"
#define GHOTA_REPO "dv_hvac_monitor"
#define GHOTA_CURRENT_TAG "1.0.0"
#define GHOTA_BIN_FILE "djw-ota-update-test.ino.nodemcu.bin"
#define GHOTA_ACCEPT_PRERELEASE 0

#include <ESP_OTA_GitHub.h>
// Initialise Update Code
ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);

void setup() {
  // Start serial for debugging (not used by library, just this sketch).
  Serial.begin(115200);

  Serial.println();
  Serial.println("================================================================================");
  Serial.println("|                                                                              |");
  Serial.println("|                Welcome to the ESP GitHub OTA Update Showcase                 |");
  Serial.println("|                =============================================                 |");
  Serial.println("|                                                                              |");
  Serial.println("|    Authur:     Gavin Smalley                                                 |");
  Serial.println("|    Repository: https://github.com/yknivag/ESP_OTA_GitHub_Showcase/           |");
  Serial.println("|    Version:    0.0.1                                                         |");
  Serial.println("|    Date:       17th January 2020                                             |");
  Serial.println("|                                                                              |");
  Serial.println("================================================================================");
  Serial.println();
  Serial.println();

  // Start SPIFFS and retrieve certificates.
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

  // Your setup code goes here

}

void loop () {
  // Your loop code goes here
 Serial.print("Main loop:");
 delay(5000);
} 
