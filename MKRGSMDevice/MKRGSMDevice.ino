#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <MKRGSM.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <SD.h>
#include <SDU.h>
#include "WDTZero.h"
#include "MD5.h"
#include "const.h"

// gGSMAccess.getTime()     // 获取时间
// ADC_BATTERY              // 电压ADC

// 控制LED灯闪烁
void ledTwinkle(int loopTime, int delayTime) {
  for (int i = 0; i < loopTime; i++) {
    digitalWrite(LED_BUILTIN, HIGH);delay(delayTime >> 1);
    digitalWrite(LED_BUILTIN,  LOW);delay(delayTime >> 1);
  }
}

int downloadFile(HttpClient iHttpWebClient, const char* iUpdatePath) {
  unsigned long    aFileTotalSize = 0;    // 文件总体积
  unsigned long    aFileLoaddSize = 0;    // 已下载体积
  unsigned long    aTimeoutStart  = 0;    // 超时防止用
           int     aStatusCode    = 0;    // 响应状态码
           uint8_t aDataReadBuffer[512];  // 下载缓存区
  iHttpWebClient.get(iUpdatePath);
  aStatusCode = iHttpWebClient.responseStatusCode();
  if (aStatusCode == 200) {
    aFileTotalSize = iHttpWebClient.contentLength();
    iHttpWebClient.stop();
#if DEBUG_PUMP
    Serial.print("OTA: Fetch File Size: ");Serial.println(aFileTotalSize);
#endif
  } else {
    iHttpWebClient.stop();
#if DEBUG_PUMP
    Serial.print("OTA: Fetch Fail: ");Serial.println(aStatusCode);
#endif
    return -1;
  }
  if(SD.exists("UPDATETP.bin")) SD.remove("UPDATETP.bin");
  File aUpdateTempFile = SD.open("UPDATETP.bin", FILE_WRITE);
  do {
    unsigned long aLoadUpponSize = (aFileTotalSize - aFileLoaddSize > DOWNLOAD_SLICE) ? (aFileLoaddSize + DOWNLOAD_SLICE - 1) : (aFileTotalSize - 1);
    iHttpWebClient.beginRequest();
    iHttpWebClient.get(iUpdatePath);
    iHttpWebClient.sendHeader("Range", "bytes=" + String(aFileLoaddSize) + "-" + String(aLoadUpponSize));
    iHttpWebClient.endRequest();
    aStatusCode = iHttpWebClient.responseStatusCode();
    if (aStatusCode == 206) {
      if(SD.exists("Slice.bin")) SD.remove("Slice.bin");
      File aSliceFile = SD.open("Slice.bin", FILE_WRITE);
      int aSliceLen = iHttpWebClient.contentLength();
      int aSliceDon = 0;
      aTimeoutStart = millis();
#if DEBUG_PUMP
      Serial.print("OTA: Slice Range: ");Serial.print(aFileLoaddSize);Serial.print(" - ");Serial.println(aLoadUpponSize);
#endif
      while ( (iHttpWebClient.connected() || iHttpWebClient.available()) &&
              (!iHttpWebClient.endOfBodyReached()) &&
              ((millis() - aTimeoutStart) < 5000) ) {
        while (iHttpWebClient.available()) {
          int aDateLength = iHttpWebClient.read(aDataReadBuffer, 512);
          aSliceFile.write(aDataReadBuffer, aDateLength);
          aTimeoutStart = millis();        // Reset the timeout counter
          aSliceDon += aDateLength;
#if DEBUG_PUMP
          Serial.print("OTA: Loadding: ");Serial.print(aSliceDon);Serial.print(" / ");Serial.println(aSliceLen);
#endif
        }
#if DEBUG_PUMP
        Serial.print("*");
#endif
      }
      iHttpWebClient.stop();
      aSliceFile.close();
      if (aSliceDon == aSliceLen) {
        aSliceFile = SD.open("Slice.bin", FILE_READ);
        do {
          int aReadSize = aSliceFile.read(aDataReadBuffer, 512);
          aUpdateTempFile.write(aDataReadBuffer, aReadSize);
          aSliceDon -= aReadSize;
        } while(aSliceDon > 0);
        aSliceFile.close();
        aFileLoaddSize = aLoadUpponSize + 1;
        SD.remove("Slice.bin");
#if DEBUG_PUMP
        Serial.println("OTA: Slice Downloaded");
#endif
      } else {
        SD.remove("Slice.bin");
#if DEBUG_PUMP
        Serial.println("OTA: Slice Try Again");
#endif
      }
    }
  } while (aFileLoaddSize < aFileTotalSize);
  aUpdateTempFile.close();
#if DEBUG_PUMP
  Serial.println("OTA: TempFile Downloaded");
#endif
  return 0;
}

int checkFile(const char* aUpdateMD5) {
  if(SD.exists("UPDATETP.bin")) {
    uint8_t aDataReadBuffer[512]; // 下载缓存区
    unsigned char *  hash   = (unsigned char *) malloc(16); // 预分配地址
    MD5_CTX context;
    if(SD.exists("UPDATE.bin")) SD.remove("UPDATE.bin");
    File aUpdateFile = SD.open("UPDATE.bin", FILE_WRITE);
    File aUpdateTempFile = SD.open("UPDATETP.bin", FILE_READ);
    size_t aFileSize = aUpdateTempFile.size();
#if DEBUG_PUMP
    Serial.print("OTA: MD5 Sizeof(UPDATETP.bin)= ");Serial.println(aFileSize);
#endif
    MD5::MD5Init(&context);
    do {
      int aReadSize = aUpdateTempFile.read(aDataReadBuffer, 512);
      aUpdateFile.write(aDataReadBuffer, aReadSize);
      MD5::MD5Update(&context, aDataReadBuffer, aReadSize);
      aFileSize -= aReadSize;
    } while(aFileSize > 0);
    aUpdateFile.close();
    aUpdateTempFile.close();
    SD.remove("UPDATETP.bin");
    MD5::MD5Final(hash, &context);
    char *md5str = MD5::make_digest(hash, 16);
#if DEBUG_PUMP
    Serial.print("OTA: MD5(");Serial.print(md5str);Serial.println(")");
#endif
    int aResult = (strcmp(md5str, aUpdateMD5) == 0) ? 0 : -1;
    free(md5str);
    free(hash);
    return aResult;
  } else {
#if DEBUG_PUMP
    Serial.print("OTA: UPDATETP.bin Not Exists");
#endif
    return -2;
  }
}

// 上传液位和泵指令信息
void publishMessage() {
  const int capacity = JSON_ARRAY_SIZE(5) + 4 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  JsonObject jsOb1 = doc.createNestedObject();
  jsOb1["bn"] = MQTT_USER;
  jsOb1["n"]  = "A0";
  jsOb1["u"]  = "";
  jsOb1["v"]  = gSensor0Value;
  JsonObject jsOb2 = doc.createNestedObject();
  jsOb2["n"]  = "A1";
  jsOb2["u"]  = "";
  jsOb2["v"]  = gSensor1Value;
  JsonObject jsOb3 = doc.createNestedObject();
  jsOb3["n"]  = "P0";
  jsOb3["u"]  = "";
  jsOb3["v"]  = gPump0RealCommand;
  JsonObject jsOb4 = doc.createNestedObject();
  jsOb4["n"]  = "P1";
  jsOb4["u"]  = "";
  jsOb4["v"]  = gPump1RealCommand;
  JsonObject jsOb5 = doc.createNestedObject();
  jsOb5["n"]  = "P2";
  jsOb5["u"]  = "";
  jsOb5["v"] = gPump2RealCommand;
  doc.add(jsOb1);
  doc.add(jsOb2);
  doc.add(jsOb3);
  doc.add(jsOb4);
  doc.add(jsOb5);
  String forprint = "";
  serializeJson(doc, forprint);
  gMQTTClient.beginMessage(PUBLISH_TOPIC_REPORT);
  gMQTTClient.print(forprint);
  gMQTTClient.endMessage();
}

// 接收上位的设定
void onMessageReceived(int messageSize) {
#if DEBUG_PUMP
    Serial.println("Message Come");
#endif
/*------ 解析云端命令 ------*/
  const int capacity = JSON_OBJECT_SIZE(SETTING_NUMBER << 1 + 3);
  StaticJsonDocument<capacity> doc;
  DeserializationError error = deserializeJson(doc, gMQTTClient);
  if (error) return;
/*------ 读取并更新功能码 ------*/
  for (int fileIdx = 0; fileIdx < SETTING_NUMBER; fileIdx++) {
    if (!(doc[gSettingSaveTable[fileIdx]].isNull()))
      *(gRemoteSettingTable[fileIdx]) = doc[gSettingSaveTable[fileIdx]].as<int>();
  }
/*------ 报告系统版本 ------*/
  if (!(doc["VERSION"].isNull())) gReportCodeVersion = true;
/*------ 将功能码持久化到Flash储存中 ------*/
  if (gFlashMemoryOK) {
    if (!(doc["CLEAN"].isNull())) {
      SerialFlash.eraseAll();                 // 清空 SPI FLASH
      while (SerialFlash.ready() == false) {} // wait, 30 seconds to 2 minutes for most chips
    } else {
      for (int fileIdx = 0; fileIdx < 9; fileIdx++) {
        if (!SerialFlash.exists(gSettingSaveTable[fileIdx])) SerialFlash.createErasable(gSettingSaveTable[fileIdx], INT_SIZE);
        SerialFlashFile file = SerialFlash.open(gSettingSaveTable[fileIdx]);
        file.erase();
        file.write((char*)(gRemoteSettingTable[fileIdx]), INT_SIZE);
      }
    }
  }
/*------ 更新Sketch ------*/
  if (gSDCardOK) {
    if (!(doc["UDSEVR"].isNull()) && !(doc["UDPORT"].isNull()) && !(doc["UDPATH"].isNull()) && !(doc["UDMD5P"].isNull())) {
      const char* updateServer = doc["UDSEVR"].as<char*>();
            int   updatePort   = doc["UDPORT"].as<int>();
      const char* updatePath   = doc["UDPATH"].as<char*>();
      const char* updateMD5    = doc["UDMD5P"].as<char*>();
#if DEBUG_PUMP
      Serial.println("OTA");
      Serial.print("UDSEVR: ");Serial.println(updateServer);
      Serial.print("UDPORT: ");Serial.println(updatePort);
      Serial.print("UDPATH: ");Serial.println(updatePath);
      Serial.print("UDMD5P: ");Serial.println(updateMD5);
#endif
/*------ 下载Sketch ------*/
      while ((gGSMAccess.begin(SECRET_PINNUMBER) != GSM_READY) || (gGPRS.attachGPRS(SECRET_GPRS_APN, SECRET_GPRS_LOGIN, SECRET_GPRS_PASSWORD) != GPRS_READY)) delay(500);
      gNeedReconnect = true;
#if SSL_CONNECT
      HttpClient aHttpWebClient = HttpClient(gGSMSSLClient, updateServer, updatePort);
#else
      HttpClient aHttpWebClient = HttpClient(gGSMClient, updateServer, updatePort);
#endif
      downloadFile(aHttpWebClient, updatePath);
/*------ 校验Sketch ------*/
      if(checkFile(updateMD5) == 0) {
        /*------ 决定是否重启CPU ------*/
#if DEBUG_PUMP
        Serial.println("OTA: Reboot");
#endif
        gWatchDog.setup(WDT_HARDCYCLE62m);
        while (true) {}
      } else {
        SD.remove("UPDATE.bin");
#if DEBUG_PUMP
        Serial.println("OTA: Fail");
#endif
      }
    }
  }
#if DEBUG_PUMP
  Serial.println("Message Deal");
#endif
  ledTwinkle(5, 100);
}

// 生成和发出水泵指令
void pumpCommand() {
/*------ 根据水泵运行液位上下限和当前液位，计算需要运行的水泵数量 ------*/
  unsigned int aAutoOperatingPumpNum = gAutoOperatingPumpNum;
  if ((gSensor0Value <= gLowerLimit1P) && (aAutoOperatingPumpNum == 1)) aAutoOperatingPumpNum = 0;
  if ((gSensor0Value >= gUpperLimit1P) && (aAutoOperatingPumpNum == 0)) aAutoOperatingPumpNum = 1;
  if ((gSensor0Value <= gLowerLimit2P) && (aAutoOperatingPumpNum == 2)) aAutoOperatingPumpNum = 1;
  if ((gSensor0Value >= gUpperLimit2P) && (aAutoOperatingPumpNum == 1)) aAutoOperatingPumpNum = 2;
  if ((gSensor0Value <= gLowerLimit3P) && (aAutoOperatingPumpNum == 3)) aAutoOperatingPumpNum = 2;
  if ((gSensor0Value >= gUpperLimit3P) && (aAutoOperatingPumpNum == 2)) aAutoOperatingPumpNum = 3;
  gAutoOperatingPumpNum = aAutoOperatingPumpNum;
/*------ 根据需要运行的水泵数量，分配自动运行的水泵 ------*/
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
/*------ 结合云端命令和本地命令生成最终命令 ------*/
  gPump0RealCommand = ((gPump0UserCommand >= 1) || (gAutoOperatingP0 >= 1)) ? 1 : 0; // 通过或运算取得泵0运行命令
  gPump1RealCommand = ((gPump1UserCommand >= 1) || (gAutoOperatingP1 >= 1)) ? 1 : 0; // 通过或运算取得泵1运行命令
  gPump2RealCommand = ((gPump2UserCommand >= 1) || (gAutoOperatingP2 >= 1)) ? 1 : 0; // 通过或运算取得泵2运行命令
/*------ 操作水泵控制继电器 ------*/
  digitalWrite(0, gPump0RealCommand);
  digitalWrite(1, gPump1RealCommand);
  digitalWrite(2, gPump2RealCommand);
}

void setup() {
#if DEBUG_PUMP
  Serial.begin(9600);
//while (!Serial) {}
  Serial.println("Begin");
#endif
/*------ 初始化DO端子状态 ------*/
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
//pinMode(GSM_DTR, OUTPUT);
  pinMode(GSM_RESETN, OUTPUT);
/*------ 初始化随机数种子 ------*/
  randomSeed(analogRead(A0));
/*------ 初始化Flash并读取功能码 ------*/
  if (SerialFlash.begin(FLASH_SELECT)) {
    gFlashMemoryOK = true;
    for (int fileIdx = 0; fileIdx < SETTING_NUMBER; fileIdx++) {
      if (SerialFlash.exists(gSettingSaveTable[fileIdx]))
      SerialFlash.open(gSettingSaveTable[fileIdx]).read((char*)(gRemoteSettingTable[fileIdx]), INT_SIZE);
    }
  }
  if (SD.begin(SD_SELECT)) {
    gSDCardOK = true;
  }
/*------ 初始化看门狗 ------*/
#if WATCH_DOG
  gWatchDog.setup(WDT_SOFTCYCLE4M); // 设置看门狗4分钟
#endif
/*------ 初始化SSL证书 ------*/
#if SSL_CONNECT
  while (!ECCX08.begin()) ledTwinkle(1, 100);   // Check ECCX08
  ArduinoBearSSL.onGetTime(getTime);            // Set a callback to get the current time used to validate the servers certificate
  gSSLClient.setEccSlot(0, SECRET_CERTIFICATE); // Set the ECCX08 slot to use for the private key and the accompanying public certificate for it
#endif
/*------ 初始化MQTT设定 ------*/
  gMQTTClient.setId(MQTT_USER);                          // Optional, set the client id used for MQTT, each device that is connected to the broker must have a unique client id. The MQTTClient will generate a client id for you based on the millis() value if not set
  gMQTTClient.setUsernamePassword(MQTT_USER, MQTT_PASS); // Set the Username And Password if necessary
  gMQTTClient.onMessage(onMessageReceived);              // Set the message callback, this function is called when the MQTTClient receives a message
#if DEBUG_PUMP
  Serial.println("Setup Over");
#endif
}

void loop() {
/*------ 降低运行频率 ------*/
  delay(5000);
/*------ 运行时指示 ------*/
#if DEBUG_PUMP
  Serial.println("New Loop");
#endif
  ledTwinkle(1, 1000);
/*------ 读取ADC转换结果 ------*/
#if DEBUG_PUMP
  Serial.println("Local Calcu: Begin");
#endif
  gSensor0Value = (analogRead(A0) * gSensorScaleOfA0) >> 10; // Adc Value is 0-1024, change to sensorScale
  gSensor1Value = (analogRead(A1) * gSensorScaleOfA1) >> 10; // Adc Value is 0-1024, change to sensorScale
  pumpCommand();
#if DEBUG_PUMP
  Serial.println("Local Calcu: Over");
#endif
/*------ 维护网络和MQTT服务器连接 ------*/
#if DEBUG_PUMP
  Serial.println("Network Maintenance: Begin");
#endif
  if (gGSMAccess.status() != GSM_READY || gGPRS.status() != GPRS_READY) {
//  digitalWrite(GSM_DTR, LOW);
    digitalWrite(GSM_RESETN, HIGH);
    delay(100);
//  digitalWrite(GSM_DTR, HIGH);
    digitalWrite(GSM_RESETN, LOW);
    while ((gGSMAccess.begin(SECRET_PINNUMBER) != GSM_READY) || (gGPRS.attachGPRS(SECRET_GPRS_APN, SECRET_GPRS_LOGIN, SECRET_GPRS_PASSWORD) != GPRS_READY)) ledTwinkle(5, 300);
#if DEBUG_PUMP
    Serial.println("GSM connected");
#endif
  }
  if (!gMQTTClient.connected()) {
    while (!gMQTTClient.connect(SECRET_BROKER, SECRET_PORT)) ledTwinkle(3, 500);
    gMQTTClient.subscribe(SUBSCRIBE_TOPIC_COMMAND);
    gReportCodeVersion = true;
#if DEBUG_PUMP
    Serial.println("MQTT Connected");
#endif
  }
#if DEBUG_PUMP
  Serial.println("Network Maintenance: Over");
#endif
  gNeedReconnect = false;
/*------ 获取MQTT消息并保持连接 ------*/
#if DEBUG_PUMP
  Serial.println("Pull Mqtt: Begin");
#endif
  gMQTTClient.poll();
  if (gNeedReconnect) return;
#if DEBUG_PUMP
  Serial.println("Pull Mqtt: Over");
#endif
/*------ 执行首次连接网络任务 ------*/
  if (gReportCodeVersion) {
    gReportCodeVersion = false;
    gMQTTClient.beginMessage(PUBLISH_TOPIC_REPORT);
    gMQTTClient.print("HiSys,MyVerIs"CODE_VERSION);
    gMQTTClient.endMessage();
#if DEBUG_PUMP
    Serial.println("Say Hello");
#endif
  }
/*------ 执行定期任务 ------*/
  unsigned long aCurrentMillis = millis();                  // 获得本次开机后运行的毫秒数
  static unsigned long  gLastPublishMillis       = 0;       // 最后发布消息的时间
  static unsigned long  gLastSwitchPumpMillis    = 0;       // 最后切换水泵的时间
  // 距离上次发布超过15分钟 或 时间已经溢出并重新计数，则发布消息
  if ((aCurrentMillis - gLastPublishMillis > 900000) || (aCurrentMillis - gLastPublishMillis < 0)) {
    gLastPublishMillis = aCurrentMillis;
    publishMessage();
    ledTwinkle(15, 100);
  }
  // 距离上次发布超过1小时 或 时间已经溢出并重新计数，则切换水泵
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
