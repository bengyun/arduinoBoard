/* Change Log
 *  VGSM20191205: 根据MQTT指令，在线更新控制程序
 *  VGSM20191206: OTA更新文件获取，逐byte略过响应头，按buffer下载文件
 */

#ifndef CONST_H
#define CONST_H

#define CODE_VERSION            "VNB20191206"
// 填写设备的MQTT信息
#define SECRET_BROKER           "121.41.1.169"
#define SECRET_PORT             1883
#define MQTT_USER               "fbbd76aa-f7e0-4fef-85aa-16a4ad7044e0"
#define MQTT_PASS               "ea51f7d3-ed7a-44d6-8eb4-0f49dac0c27c"
#define SUBSCRIBE_TOPIC_COMMAND "channels/967f0ae0-b3a5-4f96-880b-756980b3b807/messages"
#define PUBLISH_TOPIC_REPORT    "channels/ba22f57d-642e-4b82-9718-5e3b68809ac0/messages"
// 如果SIM卡有PIN，需要填写SIM卡的PIN
#define SECRET_PINNUMBER        ""
// 编译开关
#define DEBUG_PUMP              true               // 开启调试模式
#define SSL_CONNECT             false              // 开启SSL连接
#define WATCH_DOG               true               // 开启看门狗
// 常量定义
#define INT_SIZE                4                  // INT型的字节数量
#define SD_SELECT               4                  // SD卡的CS引脚
#define FLASH_SELECT            5                  // Flash芯片的CS引脚
// 如果开启了SSL连接，需要填写设备的公钥
const char SECRET_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)";
/* 影响程序运行流程的Flag */
         bool  gReportCodeVersion       = true;    // 首次开机报告代码版本
         bool  gFlashMemoryOK           = false;   // Flash储存有效
         bool  gSDCardOK                = false;   // SDCard储存有效
         bool  gNeedReconnect           = false;   // 需要重连
/* 运行时状态 */
unsigned int   gAutoOperatingPumpNum    = 0;       // 根据上下限运行的水泵数量
unsigned int   gAutoOperatingP0         = 0;       // 水泵0的自动控制命令
unsigned int   gAutoOperatingP1         = 0;       // 水泵1的自动控制命令
unsigned int   gAutoOperatingP2         = 0;       // 水泵2的自动控制命令
unsigned int*  gAOPP0       = &gAutoOperatingP0;   // 水泵0的自动控制命令指针
unsigned int*  gAOPP1       = &gAutoOperatingP1;   // 水泵1的自动控制命令指针
unsigned int*  gAOPP2       = &gAutoOperatingP2;   // 水泵2的自动控制命令指针
unsigned int   gPump0RealCommand        = 0;       // 水泵0的真实控制命令
unsigned int   gPump1RealCommand        = 0;       // 水泵1的真实控制命令
unsigned int   gPump2RealCommand        = 0;       // 水泵2的真实控制命令
unsigned int   gSensor0Value            = 0;       // A0读数按照量程转换后的值，即表的读数
unsigned int   gSensor1Value            = 0;       // A1读数按照量程转换后的值，即表的读数
/* 功能码设定 */
#define SETTING_NUMBER 11                          // 功能码数量
/* 功能码内存区域定义 */
unsigned int   gLowerLimit1P            = 325;     // 1台水泵运行的液位下线
unsigned int   gUpperLimit1P            = 350;     // 1台水泵运行的液位上限
unsigned int   gLowerLimit2P            = 375;     // 2台水泵运行的液位下线
unsigned int   gUpperLimit2P            = 400;     // 2台水泵运行的液位上限
unsigned int   gLowerLimit3P            = 425;     // 3台水泵运行的液位下线
unsigned int   gUpperLimit3P            = 450;     // 3台水泵运行的液位上限
unsigned int   gPump0UserCommand        = 0;       // 水泵0的远程控制命令
unsigned int   gPump1UserCommand        = 0;       // 水泵1的远程控制命令
unsigned int   gPump2UserCommand        = 0;       // 水泵2的远程控制命令
unsigned int   gSensorScaleOfA0         = 1000;    // A0所安装的表的量程
unsigned int   gSensorScaleOfA1         = 1000;    // A1所安装的表的量程
/* 功能码名字和储存名字定义 */
         char* gSettingSaveTable[SETTING_NUMBER] = {
  "LL1P",                                          // 1台水泵运行的液位下线
  "UL1P",                                          // 1台水泵运行的液位上限
  "LL2P",                                          // 2台水泵运行的液位下线
  "UL2P",                                          // 2台水泵运行的液位上限
  "LL3P",                                          // 3台水泵运行的液位下线
  "UL3P",                                          // 3台水泵运行的液位上限
  "P0",                                            // 水泵0的远程控制命令
  "P1",                                            // 水泵1的远程控制命令
  "P2",                                            // 水泵2的远程控制命令
  "AS0",                                           // A0所安装的表的量程
  "AS1"                                            // A1所安装的表的量程
};
/* 关联功能码和内存区域 */
unsigned int*  gRemoteSettingTable[SETTING_NUMBER] = { // 远程设置的指针表
  &gLowerLimit1P,                                  // 1台水泵运行的液位下线
  &gUpperLimit1P,                                  // 1台水泵运行的液位上限
  &gLowerLimit2P,                                  // 2台水泵运行的液位下线
  &gUpperLimit2P,                                  // 2台水泵运行的液位上限
  &gLowerLimit3P,                                  // 3台水泵运行的液位下线
  &gUpperLimit3P,                                  // 3台水泵运行的液位上限
  &gPump0UserCommand,                              // 水泵0的远程控制命令
  &gPump1UserCommand,                              // 水泵1的远程控制命令
  &gPump2UserCommand,                              // 水泵2的远程控制命令
  &gSensorScaleOfA0,                               // A0所安装的表的量程
  &gSensorScaleOfA1                                // A1所安装的表的量程
};
/* 隐性功能码 */
/* VERSION */                                      // 上报版本的命令
/* CLEAN   */                                      // 初始设定命令
/* UDSEVR  */                                      // 更新服务器域名或IP
/* UDPORT  */                                      // 更新服务器端口
/* UDPATH  */                                      // 更新文件路径
/* UDMD5P  */                                      // 更新文件MD5
/* 定义工具类 */
NB            gNBAccess;
GPRS          gGPRS;
NBClient      gNBClient;                           // Used for the TCP socket connection
BearSSLClient gSSLClient(gNBClient);               // Used for SSL/TLS connection, integrates with ECC508
#if SSL_CONNECT
#define       WEB_CLIENT        gSSLClient         //
MqttClient    gMQTTClient(gSSLClient);             // 通过SSL/TLS连接创建MQTT客户端
#else
#define       WEB_CLIENT        gNBClient          //
MqttClient    gMQTTClient(gNBClient);              // 通过TCP连接创建MQTT客户端
#endif
#if WATCH_DOG
WDTZero       gWatchDog;                           // 看门狗
#endif
#endif
