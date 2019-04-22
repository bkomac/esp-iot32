
#include <Arduino.h>
#include <Espiot32.h>
#include <FS.h>
#include <WebServer.h>

// APP
String FIRM_VER = "2.0.1";
String SENSOR = "ESP32"; // BMP180, HTU21, DHT11, DS18B20
String appVersion = "1.0.0";

int BUILTINLED = 5;
bool INVERT_BUILTINLED = false;
//#ifdef BUILTINLED
// BUILTINLED = BUILTINLED;
//#endif

String app_id = "";
float adc;
String espIp;
int lightThreshold = 50;

String apSsid = "ESP32";
String apPass = "esp12345678";
int rssi;
String ssid;
int apStartTime;
long startTime;
int apTimeOut = 300000;
boolean vccMode = false;
int heartBeatTime = millis();
int heartBeatInterval = 0;

// CONF
String deviceName = "ESP";
char essid[40] = "";
char epwd[40] = "";
String wiFiSsid = "";
String wiFiPwd = "";

String securityToken = "";
String defaultMODE = "AUTO";
int timeOut = 60 * 1000; // seconds

// mqtt config
char mqttAddress[200] = "";
int mqttPort = 1883;
char mqttUser[20] = "";
char mqttPassword[20] = "";
char mqttPublishTopic[200] = "esp";
char mqttSuscribeTopic[200] = "esp";

// REST API CONFIG
char rest_server[40] = "";
boolean rest_ssl = false;
char rest_path[200] = "";
int rest_port = 80;
char api_token[100] = "";
char api_payload[400] = "";

WebServer server(80);

WiFiClient client;
PubSubClient mqClient(client);

boolean disabledWifi = false;

// MDNS
String mdns = "";

Espiot::Espiot() {}

void Espiot::init() { init("1.0.0"); }

void Espiot::init(String appVer) {
  appVersion = appVer;
  pinMode(BUILTINLED, OUTPUT);

  Serial.print(F("**Security token: "));
  Serial.println(securityToken);
  app_id = "ESP" + getMac();
  apSsid = "ESP_" + getMac();

  startTime = millis();

  Serial.print(F("**App ID: "));
  Serial.println(app_id);
  // app_id.toCharArray(deviceName, 200, app_id.length());
  deviceName = app_id;
  // read configuration
  readFS();

  if (wiFiSsid != "")
    wiFiSsid.toCharArray(essid, 40);

  if (wiFiPwd != "")
    wiFiPwd.toCharArray(epwd, 40);

  connectToWiFi();
  Serial.println("Device name: " + String(deviceName));

  if (heartBeatInterval == 0)
    heartBeatInterval = timeOut;
  if (timeOut == 0)
    heartBeatInterval = 60000;
}

void Espiot::loop() {
  yield();

  if (WiFi.status() != WL_CONNECTED && apStartTime + apTimeOut < millis() &&
      disabledWifi == false) {
    Serial.print("\nRetray to connect to AP... " + String(essid));
    Serial.print("\nstatus: " + String(WiFi.status()) + " " +
                 String(apStartTime + apTimeOut) + " < " + String(millis()));
    testWifi();
  }

  rssi = WiFi.RSSI();
  yield();
  server.handleClient();

  // MQTT client
  if (String(mqttSuscribeTopic) != "")
    mqClient.loop();

  if (heartBeatTime + heartBeatInterval < millis() && disabledWifi == false) {
    heartBeat();
    Serial.println("Heartbeat interval: " + String(heartBeatInterval) + " ms");
    heartBeatTime = millis();
  }
}

void Espiot::disableWifi() { disabledWifi = true; }

void Espiot::enableWifi() { disabledWifi = false; }

void Espiot::connectToWiFi() {

  if (disabledWifi) {
    Serial.println("Disabling WiFi...");
    sleep(3000);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    sleep(2000);
    Serial.println("WiFi is disabled: " + String(WiFi.status()) + " != " +
                   String(WL_CONNECTED));
    return;
  }

  // auto connect
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  if (String(essid) != "" && String(essid) != "nan") {
    Serial.print(F("SSID found. Trying to connect to: "));
    Serial.print(essid);
    Serial.print(":");
    Serial.print(epwd);
    Serial.println("");
    delay(100);
    WiFi.begin(essid, epwd);
    delay(500);
  }

  if (!testWifi())
    setupAP();
  else {
    delay(200);
    if (!MDNS.begin(app_id.c_str())) {
      Serial.println("Error setting up MDNS responder!");
      mdns = "error";
    } else {
      Serial.println("mDNS responder started");
      // Add service to MDNS-SD
      MDNS.addService("http", "tcp", 80);
      mdns = app_id.c_str();
      Serial.println("mDNS address: http://" + mdns + ".local");
    }
    ssid = WiFi.SSID();
    Serial.print(F("\nconnected to "));
    Serial.print(ssid);
    Serial.print(F(" rssi:"));
    Serial.print(rssi);
    Serial.print(F(" status: "));
    Serial.print(WiFi.status());
    Serial.print(F(" = "));
    Serial.println(WL_CONNECTED + String("-WL_CONNECTED?"));

    Serial.println(" ");
    IPAddress ip = WiFi.localIP();
    espIp = getIP(ip);
    Serial.print(F("AP local ip: "));
    Serial.println(espIp);
  }

  yield();
  createWebServer();

  // MQTT
  if (String(mqttAddress) != "") {
    Serial.println(F("Seting mqtt server and callback... "));
    mqClient.setServer(mqttAddress, mqttPort);
    /*mqClient.setCallback(
        [this](char *topic, byte *payload, unsigned int length) {
          this->mqCallback(topic, payload, length);
      }); */
    mqReconnect();
  } else {
    Serial.println(F("No mqtt address."));
  }
}

// tesing for wifi connection
bool Espiot::testWifi() {
  int c = 0;
  Serial.println("\nWaiting for Wifi to connect...");

  if (WiFi.status() == WL_CONNECTED)
    return true;

  if (String(essid) == "") {
    Serial.println("\nNo SSID configured!");
    return false;
  }

  if (checkNetwork(String(essid)) == false)
    return false;

  WiFi.mode(WIFI_STA);
  //  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(essid, epwd);
  apStartTime = millis();
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("WiFi connected to "));
      Serial.println(WiFi.SSID());
      Serial.print(F("IP: "));
      IPAddress ip = WiFi.localIP();
      Serial.println(getIP(ip));

      blink(3, 30, 500);
      return true;
    }
    blink(1, 200);
    delay(500);
    Serial.print(String(c) + F(" Retrying to connect to WiFi ssid: "));
    Serial.print(essid);
    Serial.print(F(" status="));
    Serial.println(WiFi.status());

    c++;
  }
  Serial.println(F(""));
  Serial.println(F("Connect timed out."));

  blink(20, 30);
  delay(1000);
  // reset esp
  // ESP.reset();
  // delay(3000);
  return false;
}

PubSubClient Espiot::getMqClient() { return mqClient; }

void Espiot::setupAP() {
  Serial.print(F("Setting up access point. WiFi.mode= "));
  Serial.println(WIFI_STA);
  WiFi.mode(WIFI_STA);
  // WiFi.disconnect();

  delay(100);

  WiFi.softAP(apSsid.c_str(), apPass.c_str());
  Serial.print("SoftAP ready. AP SID: " + String(apSsid) + " pass: " +
               String(apPass));

  IPAddress apip = WiFi.softAPIP();
  Serial.print(F(", AP IP address: "));

  String softAp = getIP(apip);
  apStartTime = millis();
  Serial.println(apip);
  espIp = String(apip);
  blink(5, 10);
}

bool Espiot::checkNetwork(String ssidName) {
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println(F("Scanning network done."));
  if (n == 0) {
    Serial.println(F("No networks found."));

  } else if (n < 0) {
    Serial.println(F("Error with network scan."));

  } else {
    Serial.print(n);
    Serial.println(F(" networks found"));
    Serial.println(F("---------------------------------"));
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(F(": "));
      Serial.print(WiFi.SSID(i));
      Serial.print(F(" ("));
      Serial.print(WiFi.RSSI(i));
      Serial.print(F(")"));
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
      if (String(WiFi.SSID(i)) == ssidName)
        return true;
    }
  }
  Serial.print(F("\nNetwork "));
  Serial.print(ssidName);
  Serial.println(F(" not found!"));
  return false;
}

// MQTT
void Espiot::mqCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  String msg = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg = msg + (char)payload[i];
  }
  Serial.println();
  blink(3, 20);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &rootMqtt = jsonBuffer.parseObject(msg);
}

bool Espiot::mqReconnect() {
  yield();
  if (mqClient.connected())
    return true;

  if (strlen(mqttAddress) > 0) {
    Serial.print(F("\nAttempting MQTT connection... "));
    Serial.print(mqttAddress);
    Serial.print(F(":"));
    Serial.print(mqttPort);
    Serial.print(F("@"));
    Serial.print(mqttUser);
    Serial.print(F(":"));
    Serial.print(mqttPassword);
    Serial.print(F("// Pub: "));
    Serial.print(mqttPublishTopic);
    Serial.print(F(", Sub: "));
    Serial.print(mqttSuscribeTopic);

    testWifi();
    yield();
    // Attempt to connect
    if (mqClient.connect(app_id.c_str(), mqttUser, mqttPassword)) {
      yield();
      Serial.print(F("\nConnected to MQTT with cid: "));
      Serial.println(app_id);

      // suscribe
      if (String(mqttSuscribeTopic) != "") {
        mqClient.subscribe(String(mqttSuscribeTopic).c_str());
        Serial.print(F("\nSuscribed to toppic: "));
        Serial.println(mqttSuscribeTopic);
      }

    } else {
      Serial.print(F("\nFailed to connect! Client state: "));
      Serial.println(mqClient.state());
      blink(3, 20);
    }
  } else {
    Serial.print(
        F("\nNot connecting to MQTT because MQTT address is not set."));
    blink(3, 20);
    return false;
  }
  return mqClient.connected();
}

void Espiot::heartBeat() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["id"] = app_id;
  root["deviceName"] = deviceName;
  root["type"] = "heartBeat";
  root["deviceIP"] = WiFi.localIP().toString();
  root["mdns"] = mdns;
  root["espIotVersion"] = FIRM_VER;
  root["appVersion"] = appVersion;

  // meta["sensor"] = SENSOR;
  // meta["adc_vcc"] = adc;
  root["ssid"] = WiFi.SSID();
  // meta["softAPIP"] = WiFi.softAPIP().toString();
  root["mqttAddress"] = mqttAddress;
  root["mqttPublishTopic"] = mqttPublishTopic;
  root["mqttSuscribeTopic"] = mqttSuscribeTopic;
  root["rssi"] = rssi;
  // meta["freeHeap"] = ESP.getFreeHeap();
  root["upTimeSec"] = (millis() - startTime) / 1000;

  String content;
  root.printTo(content);
  mqPublish(content);
}

bool Espiot::mqPublish(String msg) {

  if (mqReconnect()) {

    // mqClient.loop();

    Serial.print(F("\nPublish message to topic '"));
    Serial.print(String(mqttPublishTopic).c_str());
    Serial.print(F("':"));
    Serial.println(msg);
    bool rc = mqClient.publish(String(mqttPublishTopic).c_str(), msg.c_str());
    Serial.println("success: " + String(rc));
    return true;
  } else {
    Serial.print(F("\nPublish failed! Not connected!"));
    return false;
  }
}

bool Espiot::mqPublishSubTopic(String msg, String subTopic) {

  if (mqReconnect()) {

    // mqClient.loop();
    String completeTopic = String(mqttPublishTopic) + "/" + subTopic;
    Serial.print(F("\nPublish message to topic '"));
    Serial.print(completeTopic);
    Serial.print(F("':"));
    Serial.println(msg);
    mqClient.publish(String(completeTopic).c_str(), msg.c_str());
    return true;

  } else {
    Serial.print(F("\nPublish failed!"));
    return false;
  }
}

void Espiot::readFS() {
  // read configuration from FS json
  Serial.println(F("- mounting FS..."));
  yield();
  boolean spiffsStatus = SPIFFS.begin(true);
  yield();
  if (spiffsStatus) {
    Serial.println(F("- mounted file system"));
    /*FSInfo fs_info;
    SPIFFS.info(fs_info);
    printf("SPIFFS: %lu of %lu bytes used.\n", fs_info.usedBytes,
    fs_info.totalBytes);
    */

    if (SPIFFS.exists("/config.json")) {
      // file exists, reading and loading
      Serial.println(F("- reading config.json file"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println(F("- opened config.json file"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonBuffer configJsonBuffer;
        JsonObject &jsonConfig = configJsonBuffer.parseObject(buf.get());
        jsonConfig.printTo(Serial);

        if (jsonConfig.success()) {
          Serial.println(F("\nparsed config.json"));

          // config parameters
          securityToken = jsonConfig["securityToken"].asString();

          deviceName = jsonConfig["deviceName"].asString();

          String builtInLed1 = jsonConfig["statusLed"];
          BUILTINLED = builtInLed1.toInt();

          bool invertBuiltInLed1 = jsonConfig["invertStatusLed"];
          INVERT_BUILTINLED = invertBuiltInLed1;

          String lightThreshold1 = jsonConfig["lightThreshold"];
          lightThreshold = lightThreshold1.toInt();

          String timeOut1 = jsonConfig["timeOut"];
          timeOut = timeOut1.toInt();

          defaultMODE = jsonConfig["defaultMode"].asString();

          String mqttAddress1 = jsonConfig["mqttAddress"].asString();
          mqttAddress1.toCharArray(mqttAddress, 200, 0);
          String mqttPort1 = jsonConfig["mqttPort"];
          mqttPort = mqttPort1.toInt();

          String mqttUser1 = jsonConfig["mqttUser"].asString();
          mqttUser1.toCharArray(mqttUser, 20, 0);
          String mqttPassword1 = jsonConfig["mqttPassword"].asString();
          mqttPassword1.toCharArray(mqttPassword, 20, 0);

          String mqttPubTopic1 = jsonConfig["mqttPublishTopic"].asString();
          mqttPubTopic1.toCharArray(mqttPublishTopic, 200, 0);
          String mqttSusTopic1 = jsonConfig["mqttSuscribeTopic"].asString();
          mqttSusTopic1.toCharArray(mqttSuscribeTopic, 200, 0);

          String rest_server1 = jsonConfig["restApiServer"].asString();
          rest_server1.toCharArray(rest_server, 40, 0);

          String rest_ssl1 = jsonConfig["restApiSSL"];
          if (rest_ssl1 == "true")
            rest_ssl = true;
          else
            rest_ssl = false;

          String rest_path1 = jsonConfig["restApiPath"].asString();
          rest_path1.toCharArray(rest_path, 200, 0);
          String rest_port1 = jsonConfig["restApiPort"];
          rest_port = rest_port1.toInt();
          String api_token1 = jsonConfig["restApiToken"].asString();
          api_token1.toCharArray(api_token, 200, 0);
          String api_payload1 = jsonConfig["restApiPayload"].asString();
          api_payload1.toCharArray(api_payload, 400, 0);

        } else {
          Serial.println(F("- failed to load json config!"));
        }
      }
    } else {
      Serial.println(F("Config.json file doesn't exist yet!"));
    }

    // ssid.json
    if (SPIFFS.exists("/ssid.json")) {
      // file exists, reading and loading
      Serial.println(F("- reading ssid.json file"));
      File ssidFile = SPIFFS.open("/ssid.json", "r");
      if (ssidFile) {
        Serial.println(F("- opened ssid file"));
        size_t size = ssidFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        ssidFile.readBytes(buf.get(), size);
        DynamicJsonBuffer ssidJsonBuffer;
        JsonObject &jsonConfig = ssidJsonBuffer.parseObject(buf.get());

        jsonConfig.printTo(Serial);
        if (jsonConfig.success()) {
          Serial.println(F("\nparsed ssid.json"));

          // ssid parameters
          String ssid1 = jsonConfig["ssid"].asString();
          ssid1.toCharArray(essid, 40, 0);
          String pwd1 = jsonConfig["password"].asString();
          pwd1.toCharArray(epwd, 40, 0);

        } else {
          Serial.println(F("failed to load ssid.json"));
        }
      }
    } else {
      Serial.println(F("ssid.json file doesn't exist yet!"));
    }
  } else {
    Serial.println(F("\nfailed to mount FS"));
    blink(10, 50, 20);
  }
  // end read
}

void Espiot::saveConfig(JsonObject &json) {

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("failed to open config file for writing"));
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

void Espiot::saveSsid(JsonObject &json) {

  File configFile = SPIFFS.open("/ssid.json", "w");
  if (!configFile) {
    Serial.println(F("failed to open ssid.json file for writing"));
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

// web server
void Espiot::createWebServer() {
  Serial.println(F("Starting server..."));
  yield();
  Serial.println(F("REST handlers init..."));

  server.on("/", [this]() { onRoot(); });

  server.on("/wifi", HTTP_GET, [this]() { onWiFiGET(); });

  server.on("/wifi", HTTP_OPTIONS, [this]() { onWiFiOPTIONS(); });

  server.on("/wifi", HTTP_POST, [this]() { onWiFiPOST(); });

  server.on("/status", HTTP_GET, [this]() { onStatusGET(); });

  server.on("/update", HTTP_GET, [this]() { onUpdateGET(); });

  server.on("/update", HTTP_POST, [this]() { onUpdatePOST(); });

  server.on("/config", HTTP_GET, [this]() { onConfigGET(); });

  server.on("/config", HTTP_POST, [this]() { onConfigPOST(); });

  server.on("/config", HTTP_OPTIONS, [this]() { onConfigOPTIONS(); });

  server.on("/ssid", HTTP_GET, [this]() { onSsidGET(); });

  server.on("/ssid", HTTP_POST, [this]() { onSsidPOST(); });

  server.on("/setting", HTTP_POST, [this]() { onSettingPOST(); });

  server.on("/reset", HTTP_DELETE, [this]() { onResetDELETE(); });

  server.on("/reset", HTTP_GET, [this]() { onResetGET(); });

  server.begin();
  Serial.println("Server started");

} //--

void Espiot::onRoot() {
  blink();
  String content;

  content = "<!DOCTYPE HTML>\r\n<html>";
  content += "<h1><u>ESP REST API</u></h1>";
  content += "<p>IP: " + espIp + "</p>";
  content += "<p>MAC/AppId: " + app_id + "</p>";
  content += "<p>Version: " + FIRM_VER + "</p>";
  content += "<p>ADC: " + String(adc) + "</p>";
  content += "<br><p><b>REST API: </b>";

  content += "<br>Set up WiFi connection (GUI) GET: <a href='http://" + espIp +
             "/wifi'>http://" + espIp + "/wifi </a>";
  content += "<br>Device status GET: <a href='http://" + espIp +
             "/status'>http://" + espIp + "/status </a>";
  content += "<br>Firmware update (payload sample) GET: <a href='http://" +
             espIp + "/update'>http://" + espIp + "/update </a>";
  content += "<br>Firmware update POST: <a href='http://" + espIp +
             "/update'>http://" + espIp + "/update </a>";
  content += "<br>Device configuration GET: <a href='http://" + espIp +
             "/config'>http://" + espIp + "/config </a>";
  content += "<br>Device configuration update POST: <a href='http://" + espIp +
             "/config'>http://" + espIp + "/config </a>";
  content +=
      "<br>Set up WiFi connection (payoad sample) GET: <a href='http://" +
      espIp + "/ssid'>http://" + espIp + "/ssid </a>";
  content += "<br>Set up WiFi connection update POST: <a href='http://" +
             espIp + "/ssid'>http://" + espIp + "/ssid </a>";
  content += "<br>Reboot device (reset) GET: <a href='http://" + espIp +
             "/reset'>http://" + espIp + "/reset </a>";
  content +=
      "<br>Delete all configuration (caution!!!) DELETE: <a href='http://" +
      espIp + "/reset'>http://" + espIp + "/reset </a>";

  content += "</html>";
  server.send(200, "text/html", content);
};

void Espiot::onWiFiGET() {
  blink();
  String content;
  content = "<!DOCTYPE HTML>\r\n<html>";
  content += "<link rel='stylesheet' "
             "href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/"
             "bootstrap.min.css' "
             "integrity='sha384-"
             "BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/"
             "K68vbdEjh4u' crossorigin='anonymous'>";
  content += "<center><div style='width:400px;'>";
  content += "<div class='card w-50'><div class = 'card-body'><h4 class = "
             "'card-title'> Welcome to ESP8266</h4><h6 class = "
             "'card-subtitle mb-2 text-muted'>Device id: " +
             getDeviceId();
  content += "</h6><p class = 'card-text'><div class='form-group'><form "
             "method='post' action='setting'><label for='usrName'>SSID: "
             "</label><input id='usrName' name='ssid' length=32 "
             "class='form-control'> <br/><label for='pwd'>PASSWORD: "
             "</label><input id='pwd' name='pass' type='password' length=64 "
             "class='form-control'></div> <input value='Save and connect' "
             "type='submit' class='btn btn-primary'></form>";
  content += "</p></div></div></center></html>";

  server.send(200, "text/html", content);
};

void Espiot::onWiFiOPTIONS() {
  blink();

  // server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers",
                    "Origin, X-Requested-With, Content-Type, Accept");
  server.send(200, "text/plain", "");
};

void Espiot::onWiFiPOST() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(server.arg("plain"));

  String qsid = root["ssid"];
  String qpass = root["pass"];
  Serial.println("Saving SSID and PASS...");
  root.printTo(Serial);
  Serial.println("");

  String content;
  int statusCode;
  if (qsid.length() > 0 && qpass.length() > 0) {

    Serial.print("new ssid: ");
    Serial.println(qsid);
    Serial.print("new pass: ");
    Serial.println(qpass);
    Serial.println("");

    Serial.println(F("\nNew ssid is set. ESP will reconnect..."));

    saveSsid(root);

    // delay(500);
    // ESP.eraseConfig();
    delay(1000);
    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
    WiFi.begin(essid, epwd);
    delay(1000);
    testWifi();

    content = "<h2>Configuration saved! Connecting ...";
    statusCode = 200;
  } else {
    content = "<h2>Eror: Not found 404</h2>";
    statusCode = 404;
    Serial.println("Sending 404");
  }
  server.send(statusCode, "text/html", content);
};

void Espiot::onStatusGET() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["id"] = app_id;
  root["deviceName"] = deviceName;
  JsonObject &meta = root.createNestedObject("meta");
  meta["espIotVersion"] = FIRM_VER;
  meta["appVersion"] = appVersion;
  meta["sensor"] = SENSOR;
  meta["adc_vcc"] = adc;
  meta["ssid"] = essid;
  meta["rssi"] = rssi;
  meta["mdns"] = mdns;
  meta["freeHeap"] = ESP.getFreeHeap();
  meta["upTimeSec"] = (millis() - startTime) / 1000;

  String content;
  root.printTo(content);
  server.send(200, "application/json", content);

  Serial.print("\n\n/status: ");
  root.printTo(Serial);
};

void Espiot::onUpdateGET() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["url"] = "Enter url to bin file here and POST json object to ESP.";

  String content;
  root.printTo(content);
  server.send(200, "application/json", content);
};

void Espiot::onUpdatePOST() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(server.arg("plain"));
  String content;

  String secToken = root["securityToken"].asString();
  if (secToken == securityToken) {
    Serial.println("Updating firmware...");
    String message = "";
    //    for (uint8_t i = 0; i < server.args(); i++) {
    //      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    //    }

    String url = root["url"];
    Serial.println("");
    Serial.print("Update url: ");
    Serial.println(url);

    blink(10, 80);

    /* t_httpUpdate_return ret = httpUpdate.update(url);

    switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      root["rc"] = httpUpdate.getLastError();
      message += "HTTP_UPDATE_FAILD Error (";
      message += httpUpdate.getLastError();
      message += "): ";
      message += httpUpdate.getLastErrorString().c_str();
      root["msg"] = message;
      root.printTo(content);
      server.send(400, "application/json", content);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");

      root["msg"] = "HTTP_UPDATE_NO_UPDATES";
      root.printTo(content);
      server.send(400, "application/json", content);
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");

      root["msg"] = "HTTP_UPDATE_OK";
      root.printTo(content);
      server.send(200, "application/json", content);
      break;
    }
    */
  } else {

    root["msg"] = "Security token is not valid!";
    root.printTo(content);
    server.send(401, "application/json", content);
  }
};

void Espiot::onConfigGET() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["securityToken"] = "";

  root["statusLed"] = BUILTINLED;
  root["invertStatusLed"] = INVERT_BUILTINLED;
  root["lightThreshold"] = lightThreshold;

  root["timeOut"] = timeOut;

  root["mqttAddress"] = mqttAddress;
  root["mqttPort"] = mqttPort;
  root["mqttUser"] = mqttUser;
  root["mqttPassword"] = "******";

  root["mqttPublishTopic"] = mqttPublishTopic;
  root["mqttSuscribeTopic"] = mqttSuscribeTopic;

  root["restApiServer"] = rest_server;
  root["restApiSSL"] = rest_ssl;
  root["restApiPath"] = rest_path;
  root["restApiPort"] = rest_port;
  root["restApiToken"] = api_token;
  root["restApiPayload"] = api_payload;

  root["deviceName"] = deviceName;

  String content;
  root.printTo(content);
  server.send(200, "application/json", content);

  Serial.print("\n\n/config: ");
  root.printTo(Serial);
};

void Espiot::onConfigPOST() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(server.arg("plain"));
  Serial.println(F("\nSaving config..."));
  String content;

  String secToken = root["securityToken"].asString();
  if (secToken == securityToken) {

    securityToken = root["newSecurityToken"].asString();
    root["securityToken"] = root["newSecurityToken"];

    String builtInLed1 = root["statusLed"];
    BUILTINLED = builtInLed1.toInt();

    bool invertBuiltInLed1 = root["invertStatusLed"];
    INVERT_BUILTINLED = invertBuiltInLed1;

    String lightThreshold1 = root["lightThreshold"];
    lightThreshold = lightThreshold1.toInt();

    String timeOut1 = root["timeOut"];
    timeOut = timeOut1.toInt();

    String mqttAddress1 = root["mqttAddress"].asString();
    mqttAddress1.toCharArray(mqttAddress, 200, 0);
    String mqttPort1 = root["mqttPort"];
    mqttPort = mqttPort1.toInt();
    String mqttUser1 = root["mqttUser"].asString();
    mqttUser1.toCharArray(mqttUser, 20, 0);
    String mqttPassword1 = root["mqttPassword"].asString();
    mqttPassword1.toCharArray(mqttPassword, 20, 0);

    String mqttPubTopic1 = root["mqttPublishTopic"].asString();
    mqttPubTopic1.toCharArray(mqttPublishTopic, 200, 0);
    String mqttSusTopic1 = root["mqttSuscribeTopic"].asString();
    mqttSusTopic1.toCharArray(mqttSuscribeTopic, 200, 0);

    String rest_server1 = root["restApiServer"].asString();
    rest_server1.toCharArray(rest_server, 40, 0);

    String rest_ssl1 = root["restApiSSL"];
    if (rest_ssl1 == "true")
      rest_ssl = true;
    else
      rest_ssl = false;

    String rest_path1 = root["restApiPath"].asString();
    rest_path1.toCharArray(rest_path, 200, 0);
    String rest_port1 = root["restApiPort"];
    rest_port = rest_port1.toInt();
    String api_token1 = root["restApiToken"].asString();
    api_token1.toCharArray(api_token, 200, 0);
    String api_payload1 = root["restApiPayload"].asString();
    api_payload1.toCharArray(api_payload, 400, 0);

    deviceName = root["deviceName"].asString();

    root.printTo(Serial);

    saveConfig(root);

    Serial.println(F("\nConfiguration is saved."));

    root["msg"] = "Configuration is saved.";
    root.printTo(content);
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers",
                      "Origin, X-Requested-With, Content-Type, Accept");
    server.send(200, "application/json", content);
  } else {
    Serial.println(F("\nSecurity tokent not valid!"));

    root["msg"] = "Security tokent not valid!";
    root.printTo(content);
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers",
                      "Origin, X-Requested-With, Content-Type, Accept");
    server.send(401, "application/json", content);
  }
  if (mqttAddress != "") {
    mqClient.setServer(mqttAddress, mqttPort);
  }
};

void Espiot::onConfigOPTIONS() {
  blink();

  // server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers",
                    "Origin, X-Requested-With, Content-Type, Accept");
  server.send(200, "text/plain", "");
};

void Espiot::onSsidGET() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["ssid"] = essid;
  root["password"] = epwd;

  root["connectedTo"] = WiFi.SSID();
  root["localIP"] = WiFi.localIP().toString();

  root["softAPIP"] = WiFi.softAPIP().toString();

  String content;
  root.printTo(content);
  server.send(200, "application/json", content);
};

void Espiot::onSettingPOST() {
  blink(5);
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  Serial.print(F("\nSetting ssid to "));

  String ssid1 = server.arg("ssid");
  ssid1.toCharArray(essid, 40, 0);
  String pwd1 = server.arg("pass");
  pwd1.toCharArray(epwd, 40, 0);

  root["ssid"] = ssid1;
  root["password"] = pwd1;

  Serial.println(String(essid) + " " + String(epwd));
  Serial.println(F("\nNew ssid is set. ESP will reconect..."));

  String content = " <br/><br/><center><h1>New ssid is set. ESP will "
                   "reconect.</h1></center>";
  saveSsid(root);
  server.send(200, "text/html", content);

  // delay(500);
  // ESP.eraseConfig();
  delay(1000);
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin(essid, epwd);
  delay(1000);
  testWifi();
};

void Espiot::onSsidPOST() {
  blink(5);
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(server.arg("plain"));
  Serial.print(F("\nSetting ssid to "));

  String secToken = root["securityToken"].asString();
  if (secToken == securityToken) {
    String ssid1 = root["ssid"].asString();
    ssid1.toCharArray(essid, 40, 0);
    String pwd1 = root["password"].asString();
    pwd1.toCharArray(epwd, 40, 0);

    Serial.println(essid);

    Serial.println(F("\nNew ssid is set. ESP will reconect..."));
    root["msg"] = "New ssid is set. ESP will reconect.";

    String content;
    root.printTo(content);
    saveSsid(root);
    server.send(200, "application/json", content);

    delay(500);
    // ESP.eraseConfig();
    delay(1000);
    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
    WiFi.begin(essid, epwd);
    delay(1000);
    testWifi();
  } else {
    Serial.println(F("\nSecurity token not valid!"));

    root["msg"] = "Security token not valid!";

    String content;
    root.printTo(content);
    server.send(401, "application/json", content);
  }
};

void Espiot::onResetDELETE() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  String secToken = root["securityToken"].asString();
  if (secToken == securityToken) {

    root["msg"] = "Reseting ESP config. Configuration will be erased ...";
    Serial.println(F("\nReseting ESP config. Configuration will be erased..."));

    String content;
    root.printTo(content);
    server.send(200, "application/json", content);
    delay(3000);

    // clean FS, for testing
    SPIFFS.format();
    delay(1000);
    // reset settings - for testing
    WiFi.disconnect(true);
    delay(1000);

    // ESP.eraseConfig();

    // ESP.reset();
    // delay(5000);
  } else {

    root["msg"] = "Security token not valid!";
    Serial.println(F("\nSecurity token not valid!"));

    String content;
    root.printTo(content);
    server.send(401, "application/json", content);
  }
};

void Espiot::onResetGET() {
  blink();
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["msg"] = "Reseting ESP...";
  Serial.println(F("\nReseting ESP..."));

  String content;
  root.printTo(content);
  server.send(200, "application/json", content);

  // ESP.reset();
  delay(5000);
};

String Espiot::sendRequest(String msg) {
  blink();
  WiFiClientSecure client;
  // WiFiClient client;
  int i = 0;
  String url = rest_path;
  Serial.println("");
  Serial.print("Connecting for request: ");
  Serial.print(String(rest_server));
  Serial.print(":");
  Serial.print(String(rest_port));

  while (!client.connect(rest_server, rest_port)) {
    Serial.print("\nTry ");
    Serial.print(i);
    blink(2, 30);

    if (i == 4) {
      blink(20, 20);
      Serial.print("\nCan't connect to: " + String(rest_server) + ":" +
                   String(rest_port) + String(url));
      return "Can't connect to: " + String(rest_server);
    }
    i++;
  }

  Serial.print(" POST data to URL: ");
  Serial.println(url);

  String data = String(api_payload);

  String req = String("POST ") + url + " HTTP/1.1\r\n" + "Host: " +
               String(rest_server) + "\r\n" + "User-Agent: ESP/1.0\r\n" +
               "Content-Type: application/json\r\n" +
               "Cache-Control: no-cache\r\n" + "Sensor-Id: " + String(app_id) +
               "\r\n" + "Token: " + String(api_token) + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded;\r\n" +
               "Content-Length: " + data.length() + "\r\n" + "\r\n" + data;
  Serial.print("Request: ");
  Serial.println(req);
  client.print(req);

  delay(100);

  Serial.println("Response: ");
  String resp = "";
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
    resp += line;
  }

  blink(2, 40);

  Serial.println();
  Serial.println("Connection closed");
  return resp;
}

// blink
void Espiot::blink() { blink(1, 30, 30); }

void Espiot::blink(int times) { blink(times, 30, 30); }

void Espiot::blink(int times, int milisec) { blink(times, milisec, milisec); }

void Espiot::blink(int times, int himilisec, int lowmilisec) {
  pinMode(BUILTINLED, OUTPUT);
  for (int i = 0; i < times; i++) {
    if (INVERT_BUILTINLED)
      digitalWrite(BUILTINLED, HIGH);
    else
      digitalWrite(BUILTINLED, LOW);
    delay(lowmilisec);
    if (INVERT_BUILTINLED)
      digitalWrite(BUILTINLED, LOW);
    else
      digitalWrite(BUILTINLED, HIGH);
    delay(himilisec);
  }
}

String Espiot::getIP(IPAddress ip) {
  String ipo = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' +
               String(ip[3]);
  if (ipo == "0.0.0.0")
    ipo = "";
  return ipo;
}

String Espiot::getIP() { return getIP(WiFi.localIP()); }

String Espiot::getMac() {
  unsigned char mac[6];
  WiFi.macAddress(mac);
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
}

String Espiot::getDeviceId() { return app_id; }

void Espiot::enableVccMeasure() { vccMode = true; }
