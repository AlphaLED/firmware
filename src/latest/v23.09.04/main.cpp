#include <Arduino.h>
#include <WiFi.h>
#include <HTTPUpdate.h>

#include <Preferences.h>
#include <LittleFS.h>

#include <ESPAsyncWebServer.h>

#include <Battery.h>
#include <OneWire.h>
#include <DallasTemperature.h>

Preferences preferences;

const bool WiFi_UpdateCredentialsFile = false; // Update network config in EEPROM?
const char *WiFi_ssid = "";                    // Network name
const char *WiFi_password = "";                // Network password


const int pin_enableAnalogRead = 2, pin_brightnessCheck = 1, pin_batVoltage = 3, pin_chrg = 4, pin_chrgStdby = 5, pin_temp = 10, pin_powerSwitch = 6, pin_powerOn = 8, pin_leds = 7;
const float adcVoltage = 2.83;

Battery batt = Battery(3000, 4200, pin_batVoltage);
float ratio = (40 + 9.75) / 9.75;

OneWire oneWire(pin_temp);
DallasTemperature tempSensor(&oneWire);

bool serverOn = false;
int updateRequested = 0;
AsyncWebServer server(80);

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
      msg = "Requested update, please wait...";
    }
    else if(updateRequested == 3) {
      msg = "Update complete, go to /shutdown and restart your ESP.";
    }
    else if(updateRequested == -1) {
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

  updateRequested = 3;
  initServer();
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
      Serial.println("[INFO] Requested shutdown.");
      digitalWrite(pin_powerOn, LOW);
      while(true) {};
  }
}