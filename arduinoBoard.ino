#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <MKRNB.h>
#include "WDTZero.h"
#include "const.h"
#include <SerialFlash.h>
#include <SD.h>
#include <SPI.h>

NB            gNBAccess;
GPRS          gGPRS;
NBClient      gNBClient;                // Used for the TCP socket connection
BearSSLClient gSSLClient(gNBClient);    // Used for SSL/TLS connection, integrates with ECC508
#if SSL_CONNECT
MqttClient    gMQTTClient(gSSLClient);  // 通过SSL/TLS连接创建MQTT客户端
#else
MqttClient    gMQTTClient(gNBClient);   // 通过TCP连接创建MQTT客户端
#endif
#if WATCH_DOG
WDTZero       gWatchDog;                // 通过TCP连接创建MQTT客户端
#endif

         bool  gFirstPowerUp            = true;    // 首次开机
unsigned long  gLastPublishMillis       = 0;       // 最后发布消息的时间
unsigned long  gLastSwitchPumpMillis    = 0;       // 最后切换水泵的时间

unsigned int   gLowerLimit1P            = 225;     // 1台水泵运行的液位下线
unsigned int   gUpperLimit1P            = 250;     // 1台水泵运行的液位上限
unsigned int   gLowerLimit2P            = 275;     // 2台水泵运行的液位下线
unsigned int   gUpperLimit2P            = 300;     // 2台水泵运行的液位上限
unsigned int   gLowerLimit3P            = 325;     // 3台水泵运行的液位下线
unsigned int   gUpperLimit3P            = 350;     // 3台水泵运行的液位上限
unsigned int   gAutoOperatingPumpNum    = 0;       // 根据上下限运行的水泵数量
unsigned int   gAutoOperatingP0         = 0;       // 水泵0的自动控制命令
unsigned int   gAutoOperatingP1         = 0;       // 水泵1的自动控制命令
unsigned int   gAutoOperatingP2         = 0;       // 水泵2的自动控制命令
unsigned int*  gAOPP0                   = &gAutoOperatingP0; // 水泵0的自动控制命令指针
unsigned int*  gAOPP1                   = &gAutoOperatingP1; // 水泵1的自动控制命令指针
unsigned int*  gAOPP2                   = &gAutoOperatingP2; // 水泵2的自动控制命令指针
unsigned int   gPump0UserCommand        = 0;       // 水泵0的远程控制命令
unsigned int   gPump1UserCommand        = 0;       // 水泵1的远程控制命令
unsigned int   gPump2UserCommand        = 0;       // 水泵2的远程控制命令
unsigned int   gPump0RealCommand        = 0;       // 水泵0的真实控制命令
unsigned int   gPump1RealCommand        = 0;       // 水泵1的真实控制命令
unsigned int   gPump2RealCommand        = 0;       // 水泵2的真实控制命令
unsigned int   gSensorScaleOfA0         = 1500;    // A0所安装的表的量程
unsigned int   gSensorScaleOfA1         = 1000;    // A1所安装的表的量程
unsigned int   gSensor0Value            = 0;       // A0读数按照量程转换后的值，即表的读数
unsigned int   gSensor1Value            = 0;       // A1读数按照量程转换后的值，即表的读数

#define SETTING_NUMBER 9
         bool  gFlashMemoryOK           = false;   // 储存有效
         char* gSettingSaveTable[SETTING_NUMBER] = {   // 在FLash中保存设置的文件名
  "LL1P",
  "UL1P",
  "LL2P",
  "UL2P",
  "LL3P",
  "UL3P",
  "P0",
  "P1",
  "P2"
};
unsigned int*  gRemoteSettingTable[SETTING_NUMBER] = { // 远程设置的指针表
  &gLowerLimit1P,
  &gUpperLimit1P,
  &gLowerLimit2P,
  &gUpperLimit2P,
  &gLowerLimit3P,
  &gUpperLimit3P,
  &gPump0UserCommand,
  &gPump1UserCommand,
  &gPump2UserCommand
};

// 控制LED灯闪烁
void ledTwinkle(int loopTime, int delayTime){
  for (int i = 0; i < loopTime; i++) {
    delay(delayTime >> 1);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(delayTime >> 1);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// 从NB模块获得当前时间
unsigned long getTime() {
  return gNBAccess.getTime();
}

// 连接NB模块和GPRS网络
void connectNB() {
  digitalWrite(SARA_PWR_ON, HIGH);
  digitalWrite(SARA_RESETN, HIGH);
  delay(100);
  digitalWrite(SARA_RESETN, LOW);
  while ((gNBAccess.begin(SECRET_PINNUMBER) != NB_READY) || (gGPRS.attachGPRS() != GPRS_READY)) {
    ledTwinkle(5, 300);
  }
}
// 连接MQTT服务器并订阅命令频道
void connectMQTT() {
  while (!gMQTTClient.connect(SECRET_BROKER, SECRET_PORT)) ledTwinkle(3, 500);
  gMQTTClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
}

// 读取输入和判断输出
void readAdcAndMakeCmd() {
  int aAdcValue0 = analogRead(A0);
  int aAdcValue1 = analogRead(A1);
  gSensor0Value = (aAdcValue0 * gSensorScaleOfA0) >> 10; // Adc Value is 0-1024, change to sensorScale
  gSensor1Value = (aAdcValue1 * gSensorScaleOfA1) >> 10; // Adc Value is 0-1024, change to sensorScale
  pumpCommand();
  ledTwinkle(1, 1000);
}

// 上传液位和泵指令信息
void publishMessage() {
  const int capacity = JSON_ARRAY_SIZE(3) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  JsonObject jsOb1 = doc.createNestedObject();
  jsOb1["bn"] = MQTT_USER;
  jsOb1["n"]  = "water_level1";
  jsOb1["u"]  = "cm";
  jsOb1["v"]  = gSensor0Value;
  JsonObject jsOb2 = doc.createNestedObject();
  jsOb2["n"]  = "water_level2";
  jsOb2["u"]  = "cm";
  jsOb2["v"]  = gSensor1Value;
  // jsOb2["v"] = pump1Real * (20 + random(0, 10) - 5) + pump2Real * (20 + random(0, 10) - 5) + pump3Real * (20 + random(0, 10) - 5) + sensor1Value;
  JsonObject jsOb3 = doc.createNestedObject();
  jsOb3["n"]  = "pump_status";
  jsOb3["u"]  = "";
  jsOb3["v"]  = gPump0RealCommand | (gPump1RealCommand << 1) | (gPump2RealCommand << 2); // bit0: pump1; bit1: pump2; bit2: pump3
  // jsOb3["v"] = pump1Real + pump2Real * 10 + pump3Real * 100;
  doc.add(jsOb1);
  doc.add(jsOb2);
  doc.add(jsOb3);
  String forprint = "";
  serializeJson(doc, forprint);
  gMQTTClient.beginMessage(PUBLISH_TOPIC_REPORT);
  gMQTTClient.print(forprint);
  gMQTTClient.endMessage();
}

// 接收上位的设定
void onMessageReceived(int messageSize) {
  const int capacity = JSON_OBJECT_SIZE(SETTING_NUMBER << 1);
  StaticJsonDocument<capacity> doc;
  deserializeJson(doc, gMQTTClient);
  for (int fileIdx = 0; fileIdx < SETTING_NUMBER; fileIdx++) { /* 循环读取设置并写入RAM */
    if (!(doc[gSettingSaveTable[fileIdx]].isNull()))
      *(gRemoteSettingTable[fileIdx]) = doc[gSettingSaveTable[fileIdx]].as<int>();
  }
  if (gFlashMemoryOK) {   /* 储存有效 */
    if (!(doc["CLEAN"].isNull())) {  /* 清空 SPI FLASH */
      SerialFlash.eraseAll();
      while (SerialFlash.ready() == false) {} // wait, 30 seconds to 2 minutes for most chips
    } else {
      for (int fileIdx = 0; fileIdx < 9; fileIdx++) {  /* 循环写入FLASH */
        if (!SerialFlash.exists(gSettingSaveTable[fileIdx])) SerialFlash.createErasable(gSettingSaveTable[fileIdx], INT_SIZE);
        SerialFlashFile file = SerialFlash.open(gSettingSaveTable[fileIdx]);
        file.erase();
        file.write((char*)(gRemoteSettingTable[fileIdx]), INT_SIZE);
      }
    }
  }
  pumpCommand();
  // publishMessage();
  ledTwinkle(5, 100); // Show User
}

// 生成和发出水泵指令
void pumpCommand() {
  // 根据水泵运行液位上下限，自动计算需要运行的水泵数量
  unsigned int aAutoOperatingPumpNum = gAutoOperatingPumpNum;
  if ((gSensor0Value <= gLowerLimit1P) && (aAutoOperatingPumpNum == 1)) aAutoOperatingPumpNum = 0;
  if ((gSensor0Value >= gUpperLimit1P) && (aAutoOperatingPumpNum == 0)) aAutoOperatingPumpNum = 1;
  if ((gSensor0Value <= gLowerLimit2P) && (aAutoOperatingPumpNum == 2)) aAutoOperatingPumpNum = 1;
  if ((gSensor0Value >= gUpperLimit2P) && (aAutoOperatingPumpNum == 1)) aAutoOperatingPumpNum = 2;
  if ((gSensor0Value <= gLowerLimit3P) && (aAutoOperatingPumpNum == 3)) aAutoOperatingPumpNum = 2;
  if ((gSensor0Value >= gUpperLimit3P) && (aAutoOperatingPumpNum == 2)) aAutoOperatingPumpNum = 3;
  gAutoOperatingPumpNum = aAutoOperatingPumpNum;
  gAutoOperatingP0 = 0;       // 水泵0的运行状态
  gAutoOperatingP1 = 0;       // 水泵1的运行状态
  gAutoOperatingP2 = 0;       // 水泵2的运行状态
  switch (aAutoOperatingPumpNum) {
    case 3:
      *gAOPP2 = 1;            // 运行3台
    case 2:
      *gAOPP1 = 1;            // 运行2台
    case 1:
      *gAOPP0 = 1;            // 运行1台
    case 0:
    default:
      break;
  }
  gPump0RealCommand = ((gPump0UserCommand >= 1) || (gAutoOperatingP0 >= 1)) ? 1 : 0; // 通过或运算取得泵0运行命令
  gPump1RealCommand = ((gPump1UserCommand >= 1) || (gAutoOperatingP1 >= 1)) ? 1 : 0; // 通过或运算取得泵1运行命令
  gPump2RealCommand = ((gPump2UserCommand >= 1) || (gAutoOperatingP2 >= 1)) ? 1 : 0; // 通过或运算取得泵2运行命令
  digitalWrite(0, gPump0RealCommand);
  digitalWrite(1, gPump1RealCommand);
  digitalWrite(2, gPump2RealCommand);
}

void setup() {
  // 设置端子状态
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SARA_PWR_ON, OUTPUT);
  pinMode(SARA_RESETN, OUTPUT);
  // 设置随机数种子
  randomSeed(analogRead(A0));
  // 读取配置
  if (SerialFlash.begin(FLASH_SELECT)) {
    gFlashMemoryOK = true;
    for (int fileIdx = 0; fileIdx < SETTING_NUMBER; fileIdx++) {  /* 循环读取FLASH */
      if (SerialFlash.exists(gSettingSaveTable[fileIdx]))
      SerialFlash.open(gSettingSaveTable[fileIdx]).read((char*)(gRemoteSettingTable[fileIdx]), INT_SIZE);
    }
  }
  // 提示开机，闪烁1次，每次5秒
  ledTwinkle(1, 5000);
#if WATCH_DOG
  gWatchDog.setup(WDT_SOFTCYCLE8M); // 设置看门狗8分钟
#endif
#if SSL_CONNECT
  // SSL相关的设置
  while (!ECCX08.begin()) ledTwinkle(1, 100); // Check ECCX08
  ArduinoBearSSL.onGetTime(getTime); // Set a callback to get the current time used to validate the servers certificate
  gSSLClient.setEccSlot(0, SECRET_CERTIFICATE); // Set the ECCX08 slot to use for the private key and the accompanying public certificate for it
#endif
  // MQTT相关的设置
  gMQTTClient.setId(MQTT_USER); // Optional, set the client id used for MQTT, each device that is connected to the broker must have a unique client id. The MQTTClient will generate a client id for you based on the millis() value if not set
  gMQTTClient.setUsernamePassword(MQTT_USER, MQTT_PASS); // Set the Username And Password if necessary
  gMQTTClient.onMessage(onMessageReceived); // Set the message callback, this function is called when the MQTTClient receives a message
}

void loop() {
  // 维护网络和MQTT服务器连接
  if (gNBAccess.status() != NB_READY || gGPRS.status() != GPRS_READY) connectNB();
  if (!gMQTTClient.connected()) connectMQTT();

  readAdcAndMakeCmd(); // 读取输入和判断输出
  gMQTTClient.poll(); // 获取MQTT消息并保持连接

  // 上电以后首次运行，发送运行开始
  if (gFirstPowerUp) {
    gFirstPowerUp = false;
    gMQTTClient.beginMessage(PUBLISH_TOPIC_REPORT);
    gMQTTClient.print("Hello System");
    gMQTTClient.endMessage();
  }

  unsigned long aCurrentMillis = millis(); // 获得本次开机后运行的毫秒数
  // 距离上次发布超过5秒 或 时间已经溢出并重新计数，则发布最新的液位信息
  if ((aCurrentMillis - gLastPublishMillis > 5000) || (aCurrentMillis - gLastPublishMillis < 0)) {
    gLastPublishMillis = aCurrentMillis;
    publishMessage();
    ledTwinkle(15, 100); // 闪烁15次，每次100毫秒
  }
  // 距离上次发布超过1小时 或 时间已经溢出并重新计数，则交换水泵运行状态的设置指针
  if ((aCurrentMillis - gLastSwitchPumpMillis > 36000000) || (aCurrentMillis - gLastSwitchPumpMillis < 0)) {
    gLastSwitchPumpMillis = aCurrentMillis;
    unsigned int* aTempAOPP = gAOPP0;
    gAOPP0 = gAOPP1;
    gAOPP1 = gAOPP2;
    gAOPP2 = aTempAOPP;
  }
#if WATCH_DOG
  gWatchDog.clear(); // 更新看门狗
#endif
}
