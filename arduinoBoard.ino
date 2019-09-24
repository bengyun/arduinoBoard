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

void setup() {
  // DI Configure
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
#ifdef WATCH_DOG
  MyWatchDoggy.setup(WDT_SOFTCYCLE2M); // Set WDT 2 minute
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
  delay(7000);
  digitalWrite(LED_BUILTIN, LOW);
  // Fake data
  randomSeed(analogRead(A0));
  pinMode(SARA_PWR_ON, OUTPUT);
  pinMode(SARA_RESETN, OUTPUT);
}

void loop() {
  readWaterLevel(); /* 读取ADC结果 */
  if (nbAccess.status() != NB_READY || gprs.status() != GPRS_READY) {
    digitalWrite(SARA_PWR_ON, HIGH);
    digitalWrite(SARA_RESETN, HIGH);
    delay(100);
    digitalWrite(SARA_RESETN, LOW);
    connectNB();
  }
  if (!mqttClient.connected()) connectMQTT();

  mqttClient.poll(); // poll for new MQTT messages and send keep alives
  if (millis() - lastMillis > 5000) { // publish a message every 5 seconds.
    lastMillis = millis();
    publishMessage();
    // Show User
    ledTwinkle(15, 100);
  }
#ifdef WATCH_DOG
  MyWatchDoggy.clear();  // refresh wdt
#endif
}

unsigned long getTime() {
  return nbAccess.getTime(); // get the current time from the NB module
}

void connectNB() {
  while ((nbAccess.begin(pinnumber) != NB_READY) ||
         (gprs.attachGPRS() != GPRS_READY)) {
    // Show User
    ledTwinkle(5, 300);
  }
}

void connectMQTT() {
  while (!mqttClient.connect(broker, SECRET_PORT)) {
    // Show User
    ledTwinkle(3, 500);
  }
  mqttClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
}

void readWaterLevel() {
  waterLevel1 = analogRead(A0);
  waterLevel2 = analogRead(A1);
  pumpCommand();
  // Show User
  ledTwinkle(1, 1000);
}

void publishMessage() {
  const int capacity = JSON_ARRAY_SIZE(3) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  JsonObject jsOb1 = doc.createNestedObject();
  jsOb1["bn"] = user;
  jsOb1["n"] = "water_level";
  jsOb1["u"] = "cm";
  jsOb1["v"] = waterLevel1;
  JsonObject jsOb2 = doc.createNestedObject();
  jsOb2["n"] = "pump_current";
  jsOb2["u"] = "A";
  jsOb2["v"] = pump1Real * (20 + random(0, 10) - 5) + pump2Real * (20 + random(0, 10) - 5) + pump3Real * (20 + random(0, 10) - 5) + waterLevel2;
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
