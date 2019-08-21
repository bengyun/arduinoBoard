#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <MKRNB.h>

// NB settings
#define SECRET_PINNUMBER ""

// Fill in the hostname of your broker
#define SECRET_BROKER "106.12.34.4"
#define MQTT_USER "luhuafeng"
#define MQTT_PASS "125436qq!"
#define SUBSCRIBE_TOPIC_COMMAND "mtopic/cmdtopic"
#define PUBLISH_TOPIC_REPORT "mtopic/rpttopic"

#define SSL_CONNECT_NONE

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

void setup() {
  Serial.begin(115200);
  while (!Serial);
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
  for (int idx = 0; idx <= 1; idx++) {
    pinMode(idx, OUTPUT);
  }
}

void loop() {
  if (nbAccess.status() != NB_READY || gprs.status() != GPRS_READY) connectNB();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.poll(); // poll for new MQTT messages and send keep alives
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
    delay(1000);
  }
}

void connectMQTT() {
  while (!mqttClient.connect(broker, 1883)) delay(5000);
  mqttClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
}

void publishMessage() {
  Serial.println("Read AO");
  int val1 = analogRead(A0);
  int val2 = analogRead(A1);
  Serial.println(val1);
  Serial.println(val2);
  Serial.println("Send Topic");
  mqttClient.beginMessage(PUBLISH_TOPIC_REPORT);
  mqttClient.print("{\"Level1\":\"");
  mqttClient.print(val1 / 100);
  mqttClient.print(".");
  mqttClient.print(val1 % 100);
  mqttClient.print("\",\"Level2\":\"");
  mqttClient.print(val2 / 100);
  mqttClient.print(".");
  mqttClient.print(val2 % 100);
  mqttClient.print("\"}");
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  Serial.println("Receive Topic");
  byte commandContent[2];
  int i = 0;
  while ((i <= 2) && (mqttClient.available())){
    commandContent[i] = mqttClient.read();
    i++;
  }
  Serial.print(commandContent[0] - 48);
  Serial.print(commandContent[1] - 48);
  Serial.println();
  if (((commandContent[0] >= 48) && (commandContent[0] <= 49)) && ((commandContent[1] == 48) || (commandContent[1] == 49))) {
    digitalWrite((commandContent[0] - 48), (commandContent[1] - 48));
  }
}
