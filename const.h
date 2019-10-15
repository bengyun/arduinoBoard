#ifndef CONST_H
#define CONST_H

// 如果SIM卡有PIN，需要填写SIM卡的PIN
#define SECRET_PINNUMBER ""

// 填写设备的MQTT信息
#define SECRET_BROKER           "106.12.34.4"
#define SECRET_PORT             1883
#define MQTT_USER               "0ca72eeb-7850-4765-bb32-c3b1962075de"
#define MQTT_PASS               "44ba5100-6938-411a-98d8-adabb7be2855"
#define SUBSCRIBE_TOPIC_COMMAND "channels/23172b90-a025-42d1-8596-e3506e8bbdd1/messages"
#define PUBLISH_TOPIC_REPORT    "channels/ba22f57d-642e-4b82-9718-5e3b68809ac0/messages"

#define INT_SIZE                4      // INT型的字节数量
#define FLASH_SELECT            5      // digital pin for flash chip CS pin

// 编译开关
#define SSL_CONNECT             false  // 开启SSL连接
#define WATCH_DOG               true   // 开启看门狗

// 如果开启了SSL连接，需要填写设备的公钥
const char SECRET_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)";

#endif
