#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <MKRNB.h>
#include "WDTZero.h"

// NB settings
#define SECRET_PINNUMBER ""

// Fill in the hostname of your broker
#define SECRET_BROKER "<Broker Host>"
#define SECRET_PORT 1883
#define MQTT_USER "<Thing ID>"
#define MQTT_PASS "<Thing Key>"
#define SUBSCRIBE_TOPIC_COMMAND "<Command Topic>"
#define PUBLISH_TOPIC_REPORT "<Report Topic>"

// Fill in the boards public certificate
const char SECRET_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)";

const char pinnumber[]     = SECRET_PINNUMBER;
const char broker[]        = SECRET_BROKER;
const char user[]          = MQTT_USER;
const char pass[]          = MQTT_PASS;
const char* certificate    = SECRET_CERTIFICATE;

NB nbAccess;
GPRS gprs;

NBClient      nbClient;            // Used for the TCP socket connection
BearSSLClient sslClient(nbClient); // Used for SSL/TLS connection, integrates with ECC508

// #define SSL_CONNECT
#ifdef SSL_CONNECT
MqttClient    mqttClient(sslClient);
#else
MqttClient    mqttClient(nbClient);
#endif

#define WATCH_DOG
#ifdef WATCH_DOG
WDTZero MyWatchDoggy;
#endif

unsigned long lastMillis = 0;

unsigned int pump1Cmd = 0;
unsigned int pump2Cmd = 0;
unsigned int pump3Cmd = 0;
unsigned int alarmPumpStatus = 0;
unsigned int pump1Real = 0;
unsigned int pump2Real = 0;
unsigned int pump3Real = 0;

unsigned int waterLevelUpLimit   = 365;
unsigned int waterLevelDownLimit = 265;
unsigned int waterLevel1 = 0;
unsigned int waterLevel2 = 0;

bool NETConnectStatus = false;
bool MQTTConnectStatus = false;

void setup() {
  // DI Configure
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
#ifdef WATCH_DOG
  MyWatchDoggy.setup(WDT_SOFTCYCLE1M); // Set WDT 1 minute
#endif
  // Check ECCX08
  while (!ECCX08.begin()) {
    // Show User
    ledTwinkle(1, 100);
  }
  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(getTime);
  // Set the ECCX08 slot to use for the private key
  // and the accompanying public certificate for it
  sslClient.setEccSlot(0, certificate);
  // Optional, set the client id used for MQTT,
  // each device that is connected to the broker
  // must have a unique client id. The MQTTClient will generate
  // a client id for you based on the millis() value if not set
  mqttClient.setId("272a4ce58ea842de8e3065141050f1ef");
  // Set the Username And Password if necessary
  mqttClient.setUsernamePassword(user, pass);
  // Set the message callback, this function is
  // called when the MQTTClient receives a message
  mqttClient.onMessage(onMessageReceived);
  // Show User
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10000);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  // ↓↓↓Setup network connect and mqtt connect↓↓↓
  if (nbAccess.status() != NB_READY) { /* 判断NB连接状态，如果未连接，则指示不通讯 */
    NETConnectStatus = false;
    if (nbAccess.ready() > 1) { /* 初次连接或上次连接失败，则再次连接 */
      nbAccess.begin(pinnumber, true, false);
    }
  } else { /* 判断NB连接状态，如果已连接，则判断GPRS */
    if (gprs.status() != GPRS_READY) { /* 判断GPRS连接状态，如果未连接，则尝试连接 */
      NETConnectStatus = false;
      if (gprs.attachGPRS() == GPRS_READY) { /* 连接GPRS，如果成功，则指示网络连接有效 */
        NETConnectStatus = true;
      }
    } else { /* 判断GPRS连接状态，如果已连接，则指示网络连接有效 */
      NETConnectStatus = true;
    }
  }
  if (NETConnectStatus) { /* 判断网络连接状态，如果连接，则判断MQTT连接 */
    if (mqttClient.connected()) { /* 判断MQTT连接状态，如果连接，则通讯 */
      MQTTConnectStatus = true;
    } else { /* 判断MQTT连接状态，如果未连接，则尝试连接 */
      MQTTConnectStatus = false;
      if (mqttClient.connect(broker, SECRET_PORT)) { /* 连接MQTT，如果成功，则通讯 */
        mqttClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
        MQTTConnectStatus = true;
      }
    }
  } else { /* 判断网络连接状态，如果未连接，则不通讯 */
    MQTTConnectStatus = false;
  }
  // ↑↑↑Setup network connect and mqtt connect↑↑↑

  // ↓↓↓Regular work↓↓↓
  if (MQTTConnectStatus) mqttClient.poll(); // poll for new MQTT messages and send keep alives
  readWaterLevel(); /* 读取ADC结果 */
  if (MQTTConnectStatus && (millis() - lastMillis > 5000)) { // publish a message every 5 seconds.
    lastMillis = millis();
    publishMessage();
    // Show User
    ledTwinkle(15, 100);
  }
  // ↑↑↑Regular work↑↑↑
#ifdef WATCH_DOG
  MyWatchDoggy.clear();  // refresh wdt
#endif
}

unsigned long getTime() {
  return nbAccess.getTime(); // get the current time from the NB module
}

void readWaterLevel() {
  waterLevel1 = analogRead(A0);
  waterLevel2 = analogRead(A1);
  pumpCommand();
  // Show User
  ledTwinkle(1, 1000);
}

void publishMessage() {
  const int capacity = JSON_ARRAY_SIZE(3) + 3 * JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  JsonObject jsOb1 = doc.createNestedObject();
  jsOb1["n"] = "water_level";
  jsOb1["u"] = "cm";
  jsOb1["v"] = waterLevel1;
  JsonObject jsOb2 = doc.createNestedObject();
  jsOb2["n"] = "pump_current";
  jsOb2["u"] = "A";
  jsOb2["v"] = waterLevel2;
  JsonObject jsOb3 = doc.createNestedObject();
  jsOb3["n"] = "pump_status";
  jsOb3["u"] = "";
  jsOb3["v"] = pump1Real + pump2Real * 10 + pump3Real * 100;
  doc.add(jsOb1);
  doc.add(jsOb2);
  doc.add(jsOb3);
  String forprint = "";
  serializeJson(doc, forprint);
  mqttClient.beginMessage(PUBLISH_TOPIC_REPORT);
  mqttClient.print(forprint);
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  const int capacity = JSON_OBJECT_SIZE(10);
  StaticJsonDocument<capacity> doc;
  deserializeJson(doc, mqttClient);
  if (!(doc["p1"].isNull())) pump1Cmd = doc["p1"].as<int>(); /* 接收泵1命令 */
  if (!(doc["p2"].isNull())) pump2Cmd = doc["p2"].as<int>(); /* 接收泵2命令 */
  if (!(doc["p3"].isNull())) pump3Cmd = doc["p3"].as<int>(); /* 接收泵3命令 */
  if (!(doc["WLUL"].isNull())) waterLevelUpLimit   = doc["WLUL"].as<int>(); /* 接收紧急排水开始液位 */
  if (!(doc["WLDL"].isNull())) waterLevelDownLimit = doc["WLDL"].as<int>(); /* 接收紧急排水停止液位 */
  pumpCommand();
  publishMessage();
  // Show User
  ledTwinkle(10, 150);
}

void pumpCommand() {
  if (waterLevel1 >= waterLevelUpLimit) {
    alarmPumpStatus = HIGH;
  }
  if (waterLevel1 <= waterLevelDownLimit) {
    alarmPumpStatus = LOW;
  }
  if (alarmPumpStatus == HIGH) {
    pump1Real = HIGH;
    pump2Real = HIGH;
    pump3Real = HIGH;
  } else {
    pump1Real = pump1Cmd;
    pump2Real = pump2Cmd;
    pump3Real = pump3Cmd;
  }
  digitalWrite(0, pump1Real);
  digitalWrite(1, pump2Real);
  digitalWrite(2, pump3Real);
}

void ledTwinkle(int loopTime, int delayTime){
  for (int i = 0; i < loopTime; i++){
    delay(delayTime >> 1);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(delayTime >> 1);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
