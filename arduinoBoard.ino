#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <MKRNB.h>

// NB settings
#define SECRET_PINNUMBER ""

// Fill in the hostname of your broker
#define SECRET_BROKER "106.12.34.4"
#define MQTT_USER "luhuafeng"
#define MQTT_PASS "*********"
#define SUBSCRIBE_TOPIC "mtopic"
#define PUBLISH_TOPIC "mtopic"

#define SSL_CONNECT

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
    Serial.println("No ECCX08 present!");
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
  pinMode(31, OUTPUT);          // PA22 TC4-W0  D0
  pinMode(32, OUTPUT);          // PA23 TC4-W1  D1
  // digitalWrite(31, HIGH);
  // digitalWrite(31, LOW);
  // int val = analogRead(3);
  //
}

void loop() {
  if (nbAccess.status() != NB_READY || gprs.status() != GPRS_READY) {
    connectNB();
  }
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  // poll for new MQTT messages and send keep alives
  mqttClient.poll();
  // publish a message roughly every 5 seconds.
  if (millis() - lastMillis > 5000) {
    lastMillis = millis();
    publishMessage();
  }
}

unsigned long getTime() {
  // get the current time from the NB module
  return nbAccess.getTime();
}

void connectNB() {
  Serial.println("Attempting to connect to the cellular network");
  while ((nbAccess.begin(pinnumber) != NB_READY) ||
         (gprs.attachGPRS() != GPRS_READY)) {
    // failed, retry
    Serial.print(".");
    delay(1000);
  }
  Serial.println("You're connected to the cellular network");
}

void connectMQTT() {
  Serial.print("Attempting to MQTT broker: ");
  Serial.println(broker);
  while (!mqttClient.connect(broker, 1883)) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.println();
  Serial.println("You're connected to the MQTT broker");
  // subscribe to a topic
  mqttClient.subscribe(SUBSCRIBE_TOPIC);
}

void publishMessage() {
  Serial.println("Publishing message");
  // send message, the Print interface can be used to set the message contents
  mqttClient.beginMessage(PUBLISH_TOPIC);
  mqttClient.print("hello ");
  mqttClient.print(millis());
  mqttClient.endMessage();
}

void onMessageReceived(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");
  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }
  Serial.println();
}
