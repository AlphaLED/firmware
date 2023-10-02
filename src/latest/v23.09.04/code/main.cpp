#include <Arduino.h>
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Battery.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
Preferences preferences;
AsyncWebServer server(80);                  // Web server
const float adcVoltage = 2.83;              // Max voltage for analog to digital convert reference

const byte pin_enableAnalogRead = 2;        // Pin for analog read transistor
const byte pin_leds = 7;                    // LEDs pin                 
const byte pin_batVoltage = 3;              // Battery voltage read pin
const byte pin_brightnessCheck = 1;         // Brightness resistor pin
const byte pin_chrg = 4, pin_chrgStdby = 5; // TP4056 Li-ion charger pins, charging or standby state (LOW = ON)
const byte pin_temp = 10;                   // DS18B20 temp sensor pin, read internal temperature
const byte pin_powerSwitch = 6;             // Pin to toggle power off, LOW = shutting down
const byte pin_powerOn = 8;                 // Power button detection pin

// WiFi
const bool WiFi_UpdateCredentialsFile = false;  // Update network config in EEPROM?
const char *WiFi_ssid = "";                     // Network name
const char *WiFi_password = "";                 // Network password

// LEDs
Adafruit_NeoPixel strip(64, pin_leds, NEO_GRB + NEO_KHZ800);
const uint8_t led_map[8][8] = {     // Table corresponding to the physical position/number of the LEDs
    {56, 55, 40, 39, 24, 23, 8, 7},
    {57, 54, 41, 38, 25, 22, 9, 6},
    {58, 53, 42, 37, 26, 21, 10, 5},
    {59, 52, 43, 36, 27, 20, 11, 4},
    {60, 51, 44, 35, 28, 19, 12, 3},
    {61, 50, 45, 34, 29, 18, 13, 2},
    {62, 49, 46, 33, 30, 17, 14, 1},
    {63, 48, 47, 32, 31, 16, 15, 0}};

// Battery
Battery batt = Battery(3000, 4200, pin_batVoltage);
float ratio = (40 + 9.75) / 9.75;         // Resistors calculated ratio

// Temperature
OneWire oneWire(pin_temp);                
DallasTemperature tempSensor(&oneWire);


bool serverOn = false;
int updateRequested = 0;

const char* board_version = "v23_09_04";
const char* updaterUrl = "https://raw.githubusercontent.com/AlphaLED/firmware/main/updater/latest/%s/firmware.bin";
const char* updaterFSUrl = "https://raw.githubusercontent.com/AlphaLED/firmware/main/updater/latest/%s/filesystem.bin";

int lastPowerButtonState;
bool shutdownRequest = false;

// DigiCert High Assurance EV Root CA
const char githubCert[] PROGMEM = R"EOF( 
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";


// LEDs patterns
// Alphabet maps
const struct
{
  const uint8_t

      A[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},

      B[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      C[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      D[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      E[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},

      F[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      G[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      H[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      I[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      J[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      K[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 1, 0, 0, 0}, {0, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 1, 0, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      L[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      L2[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 1, 0, 0, 0}, {0, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      M[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 1, 0, 0, 1, 1, 0}, {0, 1, 0, 1, 1, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      N[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 1, 0, 0, 0, 1, 0}, {0, 1, 0, 1, 0, 0, 1, 0}, {0, 1, 0, 0, 1, 0, 1, 0}, {0, 1, 0, 0, 0, 1, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},

      O[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},

      P[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      R[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 1, 0, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      S[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      T[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 0}, {0, 1, 1, 1, 1, 1, 1, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      U[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      V[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      W[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 1, 0, 1, 1, 0, 1, 0}, {0, 1, 1, 0, 0, 1, 1, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      X[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      Y[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      Z[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      Z2[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      Z3[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};

} alphabet;

// Characters maps
const struct
{
  const uint8_t

      space[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      exclam_mark[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}},
      updater[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 1, 1, 1, 1, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};

} characters;

// Numbers maps
const struct
{
  const uint8_t
      one[8][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};

} numbers;

// Return maps
typedef const uint8_t (*arrayPtr)[8];
arrayPtr characterToMap(String value)
{
  if (value == "A")
    return alphabet.A;
  if (value == "Ą")
    return alphabet.A;
  else if (value == "B")
    return alphabet.B;
  else if (value == "C")
    return alphabet.C;
  else if (value == "Ć")
    return alphabet.C;
  else if (value == "D")
    return alphabet.D;
  else if (value == "E")
    return alphabet.E;
  else if (value == "Ę")
    return alphabet.E;
  else if (value == "F")
    return alphabet.F;
  else if (value == "G")
    return alphabet.G;
  else if (value == "H")
    return alphabet.H;
  else if (value == "I")
    return alphabet.I;
  else if (value == "J")
    return alphabet.J;
  else if (value == "K")
    return alphabet.K;
  else if (value == "L")
    return alphabet.L;
  else if (value == "Ł")
    return alphabet.L2;
  else if (value == "M")
    return alphabet.M;
  else if (value == "N")
    return alphabet.N;
  else if (value == "O")
    return alphabet.O;
  else if (value == "Ó")
    return alphabet.O;
  else if (value == "P")
    return alphabet.P;
  else if (value == "R")
    return alphabet.R;
  else if (value == "S")
    return alphabet.S;
  else if (value == "Ś")
    return alphabet.S;
  else if (value == "T")
    return alphabet.T;
  else if (value == "U")
    return alphabet.U;
  else if (value == "W")
    return alphabet.W;
  else if (value == "V")
    return alphabet.V;
  else if (value == "X")
    return alphabet.X;
  else if (value == "Y")
    return alphabet.Y;
  else if (value == "Z")
    return alphabet.Z;
  else if (value == "Ź")
    return alphabet.Z2;
  else if (value == "Ż")
    return alphabet.Z3;

  else if (value == " ")
    return characters.space;
  else if (value == "!")
    return characters.exclam_mark;

  else if (value == "1")
    return numbers.one;

  return characters.space;
}

// ------------------------
// ------- Server ---------
// ------------------------

void initServer()
{

  configTime(2 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Sites
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->redirect("/home"); });
  server.on("/home", HTTP_GET, [](AsyncWebServerRequest *request) {

    bool isChrgOn = !digitalRead(pin_chrg);
    bool isStdbyOn = !digitalRead(pin_chrgStdby);
    String msg = "";
    if(isChrgOn) msg += "Charging... (";
    else if(isStdbyOn) msg += "Charged. Ready to unplug (";
    else msg += "Unplugged. (";

    tempSensor.requestTemperatures();
    msg += String(batt.voltage()/1000.0) + "V, " + String(batt.level()) + "%, " + String(tempSensor.getTempCByIndex(0)) + String(char(176)) + "C)\n";
    File file = LittleFS.open("/version.txt");
    String fileContent = "";
    while (file.available()) {
      char character = file.read();
      fileContent += character;
    }
    file.close();
    msg += "Using version: " + fileContent;

    if (LittleFS.exists("/html/index.html")) {
      request->send(LittleFS, "/html/index.html", "text/html");
    } else {
      request->send(200, "text/plain", msg);
    }
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    String msg;
    if(!updateRequested) {
      updateRequested = 1;
      msg = "Requested update, please restart your device when it shuts down.";
    }
    else {
      msg = "Errorr";
    }

    request->send(200, "text/plain", msg);
  });

  server.on("/shutdown", HTTP_GET, [](AsyncWebServerRequest *request) {

    request->send(200, "text/plain", "Power off target reached.");

    shutdownRequest = true;
  });

  server.begin();
  serverOn = true;
  Serial.print("[INFO] Server IP: ");
  Serial.println(WiFi.localIP());
}

void wiFiInit()
{
  preferences.begin("wifi", true);
  if (preferences.getString("SSID") != String())
  {
    WiFi.begin(preferences.getString("SSID"), preferences.getString("Password")); // If yes, try to connect
    Serial.print("[INFO] Connecting to: ");
    Serial.println(preferences.getString("SSID"));
  }
  else
    Serial.print("[ERROR] No wifi info saved.");

  preferences.end();
}

void firmwareUpdate() // Updater
{

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // Set time via NTP, as required for x.509 validation
  time_t now = time(nullptr);

  server.end();
  updateRequested = 2;

  WiFiClientSecure client;  // Create secure wifi client
  client.setCACert(githubCert);

  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onStart([]()
                        { Serial.println("[INFO] Starting update..."); });
  
  httpUpdate.onProgress([&](int current, int total)
                           {
    Serial.print(((float)current / (float)total) * 100);
    Serial.println("%"); });

  char formattedUpdaterUrl[100];
  sprintf(formattedUpdaterUrl, updaterUrl, board_version);
  Serial.println(formattedUpdaterUrl);

  t_httpUpdate_return ret;
  ret = httpUpdate.update(client, formattedUpdaterUrl);  // Update firmware
  if (!ret) {  // Error
      Serial.println("[ERROR] Update error.");
      initServer();
      updateRequested = -1;
      return;
  }

  sprintf(formattedUpdaterUrl, updaterFSUrl, board_version);
  Serial.println(formattedUpdaterUrl);

  ret = httpUpdate.updateSpiffs(client, formattedUpdaterUrl);
  if (!ret) {  // Error
      Serial.println("[ERROR] Update error.");
      updateRequested = -1;
      initServer();
      return;
  }

  ESP.restart();
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println("[STATUS] Start!");

  pinMode(pin_chrg, INPUT);
  pinMode(pin_chrgStdby, INPUT);
  pinMode(pin_powerSwitch, INPUT);
  lastPowerButtonState = HIGH;

  pinMode(pin_powerOn, OUTPUT);
  digitalWrite(pin_powerOn, HIGH);

  if (WiFi_UpdateCredentialsFile)
  {
    preferences.begin("wifi");
    preferences.putString("SSID", WiFi_ssid);
    preferences.putString("Password", WiFi_password);
    preferences.end();
  }
  wiFiInit();

  if (!LittleFS.begin()) {
    Serial.println("[ERROR] ! FATAL filesystem error, reformatting...");
    LittleFS.format();
    ESP.restart(); // Begin filesystem
  }

  analogReadResolution(10);
  batt.onDemand(pin_enableAnalogRead);
  batt.begin(adcVoltage*1000, ratio, &sigmoidal);

  tempSensor.begin();
  tempSensor.setResolution(9);
}

void loop()
{
  if(updateRequested == 1) firmwareUpdate();

  if (WiFi.status() == WL_CONNECTED && !serverOn)
  {
    initServer(); // Start server if wifi initialized
  }

  int powerBtnState = digitalRead(pin_powerSwitch);
  if (powerBtnState != lastPowerButtonState) {
      if (powerBtnState == HIGH) {
        shutdownRequest = true;
      }
    lastPowerButtonState = powerBtnState;
  }

  if(shutdownRequest) {
      Serial.println("[INFO] Shutdown.");
      digitalWrite(pin_powerOn, LOW);
      while(true) {};
  }
}