#include "HardwareSerial.h"
#include "Arduino.h"
#include "Wardriver.h"
#include "Recon.h"
#include "Screen.h"

#include <TinyGPSPlus.h>
#include "../Vars.h"
#include "Filesys.h"

const int MAX_MACS = 150;
#if defined(ESP8266)
#include <SoftwareSerial.h>
SoftwareSerial SERIAL_VAR(GPS_RX, GPS_TX);  // RX, TX
#endif

TinyGPSPlus gps;

// CURRENT GPS & DATTIME STRING
float lat = 0;
float lng = 0;
int alt;
double hdop;
char strDateTime[20];
char currentGPS[17];

// RECON PARAMS
uint32_t totalNets = 0;
uint8_t clients = 0;
uint8_t openNets = 0;
uint8_t sats = 0;
uint8_t bat = 0;
uint8_t speed = 0;

// CURRENT DATETIME
uint8_t hr;
uint8_t mn;
uint8_t sc;
uint16_t yr;
uint8_t mt;
uint8_t dy;
char currTime[9];

Wardriver::Wardriver() {
}

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (SERIAL_VAR.available())
      gps.encode(SERIAL_VAR.read());
  } while (millis() - start < ms);
}

void updateGPS() {
  lat = gps.location.lat();
  lng = gps.location.lng();
  alt = (int)gps.altitude.meters();
  hdop = gps.hdop.hdop();
  sats = gps.satellites.value();
  speed = gps.speed.mph();

  yr = gps.date.year();
  mt = gps.date.month();
  dy = gps.date.day();

  hr = gps.time.hour();
  mn = gps.time.minute();
  sc = gps.time.second();

  sprintf(strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", yr, mt, dy, hr, mn, sc);
  sprintf(currentGPS, "%1.3f,%1.3f", lat, lng);
  sprintf(currTime, "%02d:%02d", hr, mn);
  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, "Wifi: Scanning...");
}

void updateGPS(uint8_t override) {
  lat = 37.8715, 122.2730;
  lng = 122.2730;
  alt = 220;
  hdop = 1.5;
  sats = 3;
  speed = 69;

  yr = 2023;
  mt = 7;
  dy = 25;
  hr = 10;
  mn = 36;
  sc = 56;

  sprintf(strDateTime, "%i-%i-%i %i:%i:%i", yr, mt, dy, hr, mn, sc);
  sprintf(currentGPS, "%1.3f,%1.3f", lat, lng);
  sprintf(currTime, "%02d:%02d", hr, mn);

  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, "GPS: UPDATED");
}

void initGPS() {

    #if defined(ESP32)
        SERIAL_VAR.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    #elif defined(ESP8266)
        SERIAL_VAR.begin(GPS_BAUD);
    #endif

    Screen::drawMockup("...","...",sats,totalNets,openNets,clients,bat,speed,"GPS: Initializing...");

    unsigned long startGPSTime = millis();
    while (! (gps.location.isValid())) {
        if (millis()-startGPSTime > 5000 && gps.charsProcessed() < 10) {
            Screen::drawMockup("...","...",sats,totalNets,openNets,clients,bat,speed,"GPS: NOT FOUND");
        }
        else if (gps.charsProcessed() > 10) {
            Screen::drawMockup("...","...",sats,totalNets,openNets,clients,bat,speed,"GPS: Waiting for fix...");
        }
        sats = gps.satellites.value();
        
        Serial.println(gps.location.isValid());
        ESP.wdtFeed(); smartDelay(500);
    }
    while (!(gps.date.year() == 2023) && hdop > 30) {
        Screen::drawMockup("...","...",sats,totalNets,openNets,clients,bat,speed,"GPS: Validating...");
        ESP.wdtFeed(); 
        smartDelay(500);
        Serial.println(gps.date.year());
    }
    Screen::drawMockup("...","...",sats,totalNets,openNets,clients,bat,speed,"GPS: VALID LOCATION");

    updateGPS();
}

void initGPS(uint8_t override) {
#if defined(ESP32)
  SERIAL_VAR.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
#endif
  Screen::drawMockup("...", "...", 0, 0, 0, 0, 0, 0, "GPS: Waiting for fix");
  delay(500);

  updateGPS(0);
  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, "GPS: LOCATION FOUND");
}

bool isSSIDSeen(String ssid, String ssidBuffer[], int& ssidIndex) {
  for (int j = 0; j < ssidIndex; j++) {
    if (ssidBuffer[j] == ssid) {
      return true;
    }
  }
  return false;
}

void scanNets() {
  char entry[250];
  char message[21];

  // Buffer and index for SSIDs
  static String ssidBuffer[MAX_MACS];
  static int ssidIndex = 0;
  int newNets = 0;

  //Serial.println("[ ] Scanning WiFi networks...");
  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, "WiFi: Scanning...");
  int n = WiFi.scanNetworks();
  openNets = 0;

  Filesys::open();

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);

    if (!isSSIDSeen(ssid, ssidBuffer, ssidIndex)) {
      ssidBuffer[ssidIndex++] = ssid;
      if (ssidIndex == MAX_MACS) ssidIndex = 0;  // Reset index if it reaches MAX_MACS

      char* authType = getAuthType(WiFi.encryptionType(i));

      #if defined(ESP8266)
          if (WiFi.encryptionType(i) == ENC_TYPE_NONE) openNets++;
      #elif defined(ESP32)
          if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) openNets++;
      #endif
      
      sprintf(entry, "%s,%s,%s,%s,%u,%i,%f,%f,%i,%f,WIFI", WiFi.BSSIDstr(i).c_str(), ssid.c_str(), authType, strDateTime, WiFi.channel(i), WiFi.RSSI(i), lat, lng, alt, hdop);
      Serial.println(entry);
      Filesys::write(entry);
      
      newNets++;
    }
  }
  totalNets += newNets;
  
  sprintf(message, "Logged %d new nets...", newNets);
  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, message);
  
  Filesys::close();
  WiFi.scanDelete();
}

void getBattery() {
  float analogVal = analogRead(A0);
  float voltage = (analogVal / 1023.0) * 5.0; // Convert to voltage
  bat = static_cast<uint8_t>(map(voltage, 3.7, 4.2, 0, 100)); // Map to battery level and convert to uint8_t
  bat = constrain(bat, 0, 100); // Constrain between 0 and 100
}

void Wardriver::init() {
    Screen::init();
    Screen::drawSplash(2);

    Filesys::init(updateScreen); delay(1000);
    getBattery();
    initGPS();
    
    char filename[23]; sprintf(filename,"%i-%02d-%02d",yr, mt, dy);
    Filesys::createLog(filename, updateScreen);
    
}

void Wardriver::updateScreen(char* message) {
  Screen::drawMockup(currentGPS, currTime, sats, totalNets, openNets, clients, bat, speed, message);
}

void Wardriver::scan() {
  getBattery();
  updateGPS();  // poll current GPS coordinates
  scanNets();   // scan WiFi nets
  smartDelay(500);
}
