#ifndef CONST_H
#define CONST_H

// 如果SIM卡有PIN，需要填写SIM卡的PIN
#define SECRET_PINNUMBER ""

// 填写设备的MQTT信息
#define SECRET_BROKER           "121.41.1.169"
#define SECRET_PORT             1883
#define MQTT_USER               "4e2b4730-c5bc-4b1d-8fa1-b810d3aca96d"
#define MQTT_PASS               "c19c6e4e-a881-4a7f-b83b-908da0e39c29"
#define SUBSCRIBE_TOPIC_COMMAND "channels/<none>/messages"
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
