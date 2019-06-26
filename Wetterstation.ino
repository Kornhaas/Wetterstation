#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include "SdsDustSensor.h"
#include <ArduinoOTA.h>
#include <NTPClient.h>

const long utcOffsetInSeconds = 3600;
const int Timeoffset = 1;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


// SDS011 Sensor Konfiguration
int rxPin = D5;
int txPin = D6;
SdsDustSensor sds(rxPin, txPin);

// Over The Air Update
bool ota_flag = false;
uint16_t time_elapsed = 0;

//Zähler für die Steuerung vom SDS011
int SDScounter = 0;
//Zähler für die Anzahl der Läufe
int ESPRunCounter = 0;

//Letzter Status des SDS
bool SDS_last_status = false;

//Steuerung für die Webseite
int val = 0;                    //Startzustand ausgeschaltet
String Temp = "";


// Bot Initialisieren
//TelegramBot bot(botToken, botName, botUserName);
// Set web server port number to 80
ESP8266WebServer server(80);


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);



void Web_Software_Update() {
  ota_flag = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  //  Bechandlung der Ereignissen anschlissen
  server.on("/update.html", Web_Software_Update);
  // start HTTP server:
  server.begin();
  Serial.println("HTTP server started");


  // LuftSensor INit
  sds.begin();

  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setQueryReportingMode().toString()); // ensures sensor is in 'query' reporting mode

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  timeClient.begin();

  Serial.println("Setup finished");
  Serial.println("");

  // set the telegram bot token


  // Start the bot and send a message to you, so you know the ESP has booted.
  //  bot.begin();
  //  bot.sendMessage(adminID, "ESP8266 online.", "");
}

void loop() {
  server.handleClient();
  // Serial.print(SDScounter);
  SDScounter = SDScounter + 1;
  // Serial.print(" -> ");
  // Serial.println(SDScounter);

  if (SDScounter == 1) {
    ESPRunCounter = ESPRunCounter + 1;
    sds.wakeup();
  }
  else if (SDScounter == 300) {
    PmResult pm = sds.queryPm();
    if (pm.isOk()) {
      // Clear the buffer.

      timeClient.update();

      Serial.print(daysOfTheWeek[timeClient.getDay()]);
      Serial.print(", ");
      Serial.print(timeClient.getHours() + Timeoffset );
      Serial.print(":");
      Serial.print(timeClient.getMinutes());
      Serial.print(":");
      Serial.println(timeClient.getSeconds());

      Serial.print("PM2.5 = ");
      Serial.println(pm.pm25);
      Serial.print("PM 10 = ");
      Serial.println(pm.pm10);

      // if you want to just print the measured values, you can use toString() method as well
      Serial.println(pm.toString());

    } else {

      Serial.println("Could not read values from sensor, reason: ");
      Serial.println(pm.statusToString());

    }
    WorkingStateResult state = sds.sleep();
    if (state.isWorking()) {
      //  display.clearDisplay();
      //  display.display();
      Serial.println("Problem with sleeping the sensor.");
      // display.display();
    } else {
      Serial.println("Sensor is sleeping");
    }
  }


  // put your main code here, to run repeatedly:
  //PmResult pm = sds.readPm();
  else if (SDScounter == 900)  {
    Serial.println("Restart Counter");
    SDScounter = 0;
  }
  else {

  }

  if (ESPRunCounter == 1200) {
    ESP.restart();
  }

  if (ota_flag)
  {
    uint16_t time_start = millis();


    Serial.println("Software Update started !!");



    while (time_elapsed < 40000)
    {
      ArduinoOTA.handle();
      time_elapsed = millis() - time_start;




      delay(10);
    }
    ota_flag = false;
  }


  delay(100);
}
