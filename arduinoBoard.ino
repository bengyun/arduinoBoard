#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <MKRNB.h>
#include "WDTZero.h"

// NB settings
#define SECRET_PINNUMBER ""

// Fill in the hostname of your broker
#define SECRET_BROKER "47.88.225.240"
#define SECRET_PORT 1883
#define MQTT_USER "28d39cd5-3757-42de-9a59-1bb1fdefd4db"
#define MQTT_PASS "486d392b-23fb-4734-815c-f00f4f793f46"
#define SUBSCRIBE_TOPIC_COMMAND "channels/776396e2-979d-4991-94c3-7318ea1746aa/messages"
#define PUBLISH_TOPIC_REPORT "channels/ba22f57d-642e-4b82-9718-5e3b68809ac0/messages"

#define SSL_CONNECT_NONE

#define LED_ON 1
#define LED_OFF 0

// Fill in the boards public certificate
const char SECRET_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----
MIIBLDCB06ADAgECAgEBMAoGCCqGSM49BAMCMB0xGzAZBgNVBAMTEjAxMjMxNzI4MzVCN0NFMTlF
RTAgFw0xOTA4MTQxMTAwMDBaGA8yMDUwMDgxNDExMDAwMFowHTEbMBkGA1UEAxMSMDEyMzE3Mjgz
NUI3Q0UxOUVFMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEZ66BTGtYqAqeusVlZsJROuAVDJh1
WDPLIfRitwCnXLhf2pTu9k9jh5DcJiGuMQxsEtWId96M0YAkPV4ijcgUmaMCMAAwCgYIKoZIzj0E
AwIDSAAwRQIgD03feTKgmv/GZu4mnBMfAGWbA/yv/bn57Ba/2OpQlPsCIQCx/VzOLChE5VWm7ms+
LxCvy0zO5Y42/knmHNuRBV0bAQ==
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

#ifdef SSL_CONNECT
MqttClient    mqttClient(sslClient);
#else
MqttClient    mqttClient(nbClient);
#endif

unsigned long lastMillis = 0;

int pump1 = 0;
int pump2 = 0;
int pump3 = 0;

WDTZero MyWatchDoggy; // Define WDT

void setup() {
  if (!ECCX08.begin()) {
    while (1);
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
  // DI Configure
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  MyWatchDoggy.setup(WDT_SOFTCYCLE1M);  // initialize WDT-softcounter refesh cycle on 1m interval
  ledTwinkle(1, 5000);
}

void loop() {
  if (nbAccess.status() != NB_READY || gprs.status() != GPRS_READY) connectNB();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.poll(); // poll for new MQTT messages and send keep alives
  MyWatchDoggy.clear();  // refresh wdt - before it loops
  if (millis() - lastMillis > 5000) { // publish a message roughly every 5 seconds.
    lastMillis = millis();
    publishMessage();
  }
}

unsigned long getTime() {
  return nbAccess.getTime(); // get the current time from the NB module
}

void connectNB() {
  while ((nbAccess.begin(pinnumber) != NB_READY) ||
         (gprs.attachGPRS() != GPRS_READY)) {
    ledTwinkle(3, 500);
  }
}

void connectMQTT() {
  while (!mqttClient.connect(broker, SECRET_PORT)) {
    ledTwinkle(6, 300);
  }
  mqttClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
}

void publishMessage() {
  mqttClient.beginMessage(PUBLISH_TOPIC_REPORT);
  const int capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  int val1 = analogRead(A0);
  JsonObject waterLevel = doc.createNestedObject();
  waterLevel["n"] = "water_level";
  waterLevel["u"] = "cm";
  waterLevel["v"] = val1;
  int val2 = analogRead(A1);
  JsonObject current = doc.createNestedObject();
  current["n"] = "pump_current";
  current["u"] = "A";
  current["v"] = val2;
  JsonObject pumpStatus = doc.createNestedObject();
  pumpStatus["n"] = "pump_status";
  pumpStatus["u"] = "";
  pumpStatus["v"] = pump1 + pump2 * 10 + pump3 * 100;
  doc.add(waterLevel);
  doc.add(current);
  doc.add(pumpStatus);
  String forprint="";
  serializeJson(doc, forprint);
  mqttClient.print(forprint);
  mqttClient.endMessage();
  ledTwinkle(3,200);
}

void onMessageReceived(int messageSize) {
  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  deserializeJson(doc, mqttClient);
  pump1 = doc["p1"].as<int>();
  pump2 = doc["p2"].as<int>();
  pump3 = doc["p3"].as<int>();
  digitalWrite(0, pump1);
  digitalWrite(1, pump2);
  digitalWrite(2, pump3);
  ledTwinkle(5,200);
  publishMessage();
}

void ledTwinkle(int onoffloopnum, int delaytime){
  for (int i = 0; i < onoffloopnum; i++){
    if (i != 0) {
      delay(delaytime);
    }
    digitalWrite(LED_BUILTIN, LED_ON);
    delay(delaytime);
    digitalWrite(LED_BUILTIN, LED_OFF);
  }
}
