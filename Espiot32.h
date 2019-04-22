/*
  ESPIoT Library
*/
#ifndef espiot_h
#define espiot_h

#include <Arduino.h>
#include <WString.h>
#include <Base64.h>
//#include <HTTPClient.h>
//#include <HTTPUpdate.h>

#include <FS.h>
#include <SPIFFS.h>

#include <WebServer.h>
#include <WiFi.h>

#include <ESPmDNS.h>
//#include <Hash.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

class Espiot {
private:
  void createWebServer();
  void readFS();
  void saveConfig(JsonObject &json);
  void saveSsid(JsonObject &json);

  String getIP(IPAddress ip);
  String getMac();

  void connectToWiFi();
  // void mqPublish(String msg);
  bool mqReconnect();
  void mqCallback(char *topic, byte *payload, unsigned int length);
  void heartBeat();
  bool checkNetwork(String ssidName);

  void onWiFiGET();
  void onWiFiOPTIONS();
  void onWiFiPOST();
  void onResetGET();
  void onResetDELETE();
  void onSettingPOST();
  void onSsidPOST();
  void onSsidGET();
  void onConfigOPTIONS();
  void onConfigPOST();
  void onConfigGET();
  void onUpdatePOST();
  void onUpdateGET();
  void onStatusGET();
  void onRoot();

public:
  boolean disabledWifi;
  void disableWifi();
  void enableWifi();
  String deviceName;
  int timeOut;
  int lightThreshold;
  String SENSOR;
  String defaultMODE;
  float adc;

  String wiFiSsid;
  String wiFiPwd;

  String securityToken;
  String appVersion;

  //ESP access point credensials
  String apSsid;
  String apPass;

  WebServer server;
  WiFiClient client;
  // PubSubClient mqClient;

  PubSubClient getMqClient();
  bool mqPublish(String msg);
  bool mqPublishSubTopic(String msg, String subTopic);
  String sendRequest(String data);
  int heartBeatInterval;

  String getDeviceId();
  void enableVccMeasure();
  bool testWifi();
  void setupAP();
  String getIP();

  Espiot();
  void init(String appVer);
  void init();
  void loop();

  void blink();
  void blink(int times);
  void blink(int times, int milisec);
  void blink(int times, int himilisec, int lowmilisec);
};

#endif
