
#include "M5Atom.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Sharp.h>
#include <ir_Corona.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h> 
#include <map>

const char *ssid = "XXXX";
const char *password = "XXXX";
const char *FWver = "Ver. 1.0";

const uint16_t kIrLed = 12;
IRSharpAc acSharp(kIrLed);
IRCoronaAc acCorona(kIrLed);
IRsend lightToshiba(26);

const int32_t red = 0x004000;
const int32_t green = 0x300000;
const int32_t blue = 0x000040;
const int32_t white = 0x202020;

const int32_t long_press_timeout = 1000;

WebServer server(80);

void connectWiFi()
{
  WiFi.disconnect(); // reset
  Serial.printf("Attempting to connect to SSID: %s\n", ssid);
  Serial.print("Connecting Wi-Fi");
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++)
  {
    if (i % 10 == 0) {
      WiFi.begin(ssid, password);
    }
    Serial.print(".");
    delay(1000); // wait a second for connection
  }
  Serial.print("Connected: ");
  Serial.println(WiFi.localIP());
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup() {
  M5.begin(true, false, true);
  acSharp.begin();
  acCorona.begin();
  lightToshiba.begin();
  M5.dis.setBrightness(10);
  M5.dis.drawpix(0, red);

  Serial.begin(115200);
  delay(500);

  connectWiFi();
  server.on("/ac_off", HTTP_GET, acOffbyWeb);   
  server.on("/ac_on", HTTP_GET, acOnbyWeb);   
  server.on("/light_off", HTTP_GET, lightOffbyWeb);   
  server.on("/light_on", HTTP_GET, lightOnbyWeb);   
  server.onNotFound(handleNotFound);
  server.begin();
  M5.dis.drawpix(0, green);

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });
  ArduinoOTA.begin();
}


struct _ac_ctrl_func{
  void(*on_func)(uint8_t,uint8_t,uint8_t);
  void(*off_func)(void);
};

struct _ac_ctrl_mode{
 uint8_t auto_mode;
 uint8_t cool;
 uint8_t heat;
 uint8_t dry;
 uint8_t fan;
};

struct _ac_ctrl_fan{
 uint8_t auto_mode;
 uint8_t low;
 uint8_t med;
 uint8_t high;
 uint8_t max;
};

std::map<String, struct _ac_ctrl_func> ac_maker ={
  {"corona", {acCoronaOn, acCoronaOff} },
  {"sharp",  {acSharpOn, acSharpOff} },
};

std::map<String, uint8_t> ac_mode_corona ={
   {"auto", -1 },
   {"cool", kCoronaAcModeCool },
   {"heat", kCoronaAcModeHeat },
   {"dry",  kCoronaAcModeDry},
   {"fan",  kCoronaAcModeFan },
};
std::map<String, uint8_t> ac_mode_sharp ={
   {"auto", kSharpAcAuto },
   {"cool", kSharpAcCool },
   {"heat", kSharpAcHeat },
   {"dry",  kSharpAcDry },
   {"fan",  kSharpAcFan },
};

std::map<String, std::map<String, uint8_t>> ac_mode_maker = {
  {"sharp",  ac_mode_sharp },
  {"corona", ac_mode_corona },
};

std::map<String, uint8_t> ac_fan_corona ={
   {"auto", kCoronaAcFanAuto },
   {"low", kCoronaAcFanLow },
   {"med", kCoronaAcFanMedium },
   {"high",  kCoronaAcFanHigh},
   {"max",  -1 },
};
std::map<String, uint8_t> ac_fan_sharp ={
   {"auto", kSharpAcFanAuto },
   {"low", kSharpAcFanMin },
   {"med", kSharpAcFanMed },
   {"high",  kSharpAcFanHigh},
   {"max",  kSharpAcFanMax },
};

std::map<String, std::map<String, uint8_t>> ac_fan_maker = {
  {"corona", ac_fan_corona },
  {"sharp",  ac_fan_sharp },
};

void acOffbyWeb()
{
  if (server.args()==0){
    server.send(200, "text/plain", "OK");
    ac_maker["sharp"].off_func();
    return;
  }  
  String message = "";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(200, "text/plain", message);
  ac_maker[server.arg(0)].off_func();
}


void acOnbyWeb()
{
  if (server.args()==0){
    server.send(200, "text/plain", "OK");
    ac_maker["sharp"].on_func(ac_mode_maker["sharp"]["cool"], 26, ac_fan_maker["sharp"]["med"]);
    return;
  }  
  if (server.args() != 4){
    server.send(404, "text/plain", "invalid argument");
    return;
  }
  String message = "";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  Serial.print(message);
  server.send(200, "text/plain", message);//GETリクエストへの返答

  if (server.arg(0) == "sharp" || server.arg(0) == "corona" ){
    Serial.printf("%s: %s, temp=%d, fan=%s\n", server.arg(0), server.arg(1), server.arg(2).toInt(), server.arg(3));
    ac_maker[server.arg(0)].on_func(ac_mode_maker[server.arg(0)][server.arg(1)], server.arg(2).toInt(), ac_fan_maker[server.arg(0)][server.arg(3)]);
  }
}

void lightOffbyWeb()
{
  server.send(200, "text/plain", "OK");
  lightToshibaOff();
}

void lightOnbyWeb()
{
  server.send(200, "text/plain", "OK");
  lightToshibaOn();
}

void acSharpOn(uint8_t mode, uint8_t temp, uint8_t fan)
{
  acSharp.setMode(mode);
  acSharp.setPower(true, acSharp.getPower());   
  acSharp.setFan(fan);
  acSharp.setSwingToggle(false);
  acSharp.setTemp(temp-2);
  acSharp.setIon(true);
  acSharp.send();
}

void acSharpOff()
{
  acSharp.setPower(false, acSharp.getPower());   
  acSharp.send(); 
}

void acCoronaOn(uint8_t mode, uint8_t temp, uint8_t fan)
{
  acCorona.setMode(mode);
  acCorona.setPower(true);   
  acCorona.setFan(fan);
  acCorona.setSwingVToggle(false);
  acCorona.setTemp(temp);
  acCorona.send();
}

void acCoronaOff()
{
  acCorona.setPower(false);   
  acCorona.send(); 
}

void lightToshibaOn()
{
  lightToshiba.sendNEC(0xE730E916,32);
}

void lightToshibaOff()
{
  lightToshiba.sendNEC(0xE730D12E,32);
}

void loop() 
{

  if (WiFi.status() != WL_CONNECTED)
  {
    M5.dis.drawpix(0, red);
    connectWiFi();
    M5.dis.drawpix(0, green);
  }
  if (M5.Btn.wasPressed())  
  {
    int32_t timeout = millis() + long_press_timeout;
    while (M5.Btn.isPressed() && millis() <= timeout)
    {
      delay(100);
      M5.Btn.read();  
    } 

    if (millis() <= timeout)
    {
      M5.dis.drawpix(0, blue);
      acSharpOn(kSharpAcCool, 26, kSharpAcFanMed);
      acCoronaOn(kCoronaAcModeCool, 25, kCoronaAcFanMedium);
      lightToshibaOn();
    }
    else
    {
      M5.dis.drawpix(0, white);
      acSharpOff();
      acCoronaOff();
      lightToshibaOff();
    }  
    //printState();
  }
  delay(1);
  M5.update();
  server.handleClient();
  ArduinoOTA.handle();//OTAのための待受処理
}
