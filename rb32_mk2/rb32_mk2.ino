#include <FTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPping.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
#include <TimeLib.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Ticker.h>
#include <LittleFS.h>
#include <FS.h>

// WARNING changing these settings could brick the loggers
//---------------------------------------------------------------------
#define WIFI_SSID "ST01"
#define WIFI_PASSWORD "J9a5cxec"
#define MQTT_HOST "st01.local"
#define MQTT_PORT 1883
const char* UPDATE_SERVER = "138.68.160.221"; // Digital Ocean Droplet
// --------------------------------------------------------------------

#define MSG_BUFFER_SIZE (196)

#define MQTT_MIN_FREE_MEMORY (8096)

char msg[MSG_BUFFER_SIZE];
char fLogs[196];

const char* VERSION = "5.1.4";

uint8_t gpsTokenPosition = 0;

uint8_t dHour = 0;
uint8_t dMin = 0;
uint8_t dSec = 0;
uint32_t dImuMsgId = 0;
uint32_t ftpTransferStartTime = 0;
uint32_t ftpTransferEndTime = 0;
unsigned int lastReadPosition = 0;

int  ecg          = 0;
int  rssi         = 0;
int  pubCode      = 0;
int  logPosition1 = 0;
int  logPosition2 = 0;
int  logCounter1  = 0;
int  logCounter2  = 0;

int dumpToFlashCount = 0;

int  totalMissedGps = 0;
int  totalMissedImu = 0;
int  gpsBufferSize   = 0;
int  imuBufferSize  = 0;
int  txBufferSize  = 0;
int  totalSentGps   = 0;
int  totalSentImu   = 0;
int  totalSent = 0;
int  fromCache = 0;
int  updateFailCount = 0;
long lastBlink      = 0;

long timeNow    = 0;
long loopTime   = 0;
long timeAge    = 0;
long lastGpsTimeUpdate  = 0;
long IMU_DELAY_TIME     = 40;
long FLUSH_DELAY_TIME   = 10;
unsigned int imuMsgId  = 0;
uint16_t gpsMsgId  = 0;

long imuT1 = 0;
long imuT0 = 0;
long flushT0 = 0;
long flushT1 = 0;

String mac;
String strMsg;
String sMqttGpsMsg;
String gpsTime;
String timeAgeStr;
String logFileName;

char szDay[3];
char szMonth[3];
char szYear[5];
char szHour[3];
char szMins[3];
char szSec[3];
char szTime[12];
char rx;
char gngga[] = "$GNGGA";

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
File f1;
File f2;
File fu;

// flush the cached logs
Ticker flusherCallBack;

Ticker flushCountersCallBack;

// MPU6050 Slave Device Address
const uint8_t MPU6050SlaveAddress = 0x68;

// Select SDA and SCL pins for I2C communication
const uint8_t scl = D1;
const uint8_t sda = D2;
const uint8_t a0  = A0;

// sensitivity scale factor respective to full scale setting provided in datasheet
const uint16_t AccelScaleFactor = 16384;
const uint16_t GyroScaleFactor = 131;

// MPU6050 few configuration register addresses
const uint8_t MPU6050_REGISTER_SMPLRT_DIV         = 0x19;
const uint8_t MPU6050_REGISTER_USER_CTRL          = 0x6A;
const uint8_t MPU6050_REGISTER_PWR_MGMT_1         = 0x6B;
const uint8_t MPU6050_REGISTER_PWR_MGMT_2         = 0x6C;
const uint8_t MPU6050_REGISTER_CONFIG             = 0x1A;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG        = 0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG       = 0x1C;
const uint8_t MPU6050_REGISTER_FIFO_EN            = 0x23;
const uint8_t MPU6050_REGISTER_INT_ENABLE         = 0x38;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H       = 0x3B;
const uint8_t MPU6050_REGISTER_SIGNAL_PATH_RESET  = 0x68;

int16_t AccelX, AccelY, AccelZ, Temperature, GyroX, GyroY, GyroZ;
double Ax, Ay, Az, T, Gx, Gy, Gz;

int period = 500;
unsigned int lastPacketId = 0;
unsigned int lastImuPacketId = 0;
uint8_t lastMessageType = -1;

unsigned int t1LED, t2LED, t1FLUSH, t2FLUSH;
int ledState = LOW;

bool flushingFile1        = false;
bool flushingFile2        = false;
bool writeIntoFlushFile1  = true;
bool updateFailed = true;
bool firstTransmit = true;
bool sendNextMessage = true;
bool sendNextImuMessage = true;

enum FLUSH_STATE {
  FLUSH_FILE_1,
  FLUSH_FILE_2,
  RESET_FILES,
  TOGGLE_FLUSH_FLAGS,
  DONT_FLUSH,
  UPDATE_FIRMWARE,
  UPDATE_COMPLETE,
  START_UP,
  FLUSH_COUNTERS,
  READING_FROM_RAM,
  READING_FROM_FLASH,
  FLUSH_TX_QUEUE,
  UPLOAD_LOG_FILE,
  CHECK_FTP_STATUS,
  TEST_FTP,
  CHECK_FLASH_FLUSH_STATUS
};

enum class TransferState {
  IN_PROGRESS,
  COMPLETE,
  WAITING
};

struct GpsData {
  uint16_t msgId;
  uint8_t  fromCache;
  uint8_t  hh;
  uint8_t  mm;
  uint8_t  ss;
  float    lat;
  float    lon;
  uint8_t  valid;
  uint8_t  sats;
  float    hdop;
};

struct ImuData {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t t;
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int8_t  rssi;
  uint8_t  hh;
  uint8_t  mm;
  uint8_t  ss;
  uint16_t ms;
  uint32_t msgId;
};

struct TxQueue {
  uint8_t typeId;
  union {
    GpsData gps;
    ImuData imu;
  } data;
};

//const int GPS_BUFFER_SIZE = 60;
//const int IMU_BUFFER_SIZE = 1000;
const int TXQUEUE_SIZE = 1400;
//GpsData gpsQueue[GPS_BUFFER_SIZE];
//ImuData imuQueue[IMU_BUFFER_SIZE];
TxQueue txQueue[TXQUEUE_SIZE];

int gpsReadPtr  = 0;
int gpsWritePtr = 0;
int imuReadPtr  = 0;
int imuWritePtr = 0;
int txqReadPtr = 0;
int txqWritePtr = 0;

int testCount = 0;

FTPClient ftpClient(LittleFS);

FTPClient::ServerInfo ftpServerInfo("pi", "J9a5cxec", "st01.local", 21);

FLUSH_STATE flushState = START_UP;
FLUSH_STATE prevFlushState = DONT_FLUSH;
//#define PRINT_DEBUG_MSGS

void printDebug(String mac, String msg) {
#ifdef PRINT_DEBUG_MSGS
  Serial.println(mac + "," + msg);
  String mqttStr = mac + "," + msg;
  mqttClient.publish("rb32/debug", 0, true, mqttStr.c_str());
#endif
}

void connectToWifi() {
  printDebug(mac, "Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  printDebug(mac, "Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  printDebug(mac, "Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  printDebug(mac, "Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  printDebug(mac, "Connected to MQTT.");
  printDebug(mac, "Session present: ");
  printDebug(mac, String(sessionPresent));
  uint16_t packetIdSub = mqttClient.subscribe("rb32/debug", 2);
  mqttClient.subscribe("rb32/upload", 2);
  printDebug(mac, "Subscribing at QoS 2, packetId: ");

  String m;
  m = mac + "," + VERSION + "," + "ONLINE";
  mqttClient.publish("rb32/data/status", 0, true, m.c_str());
  flushState = UPDATE_FIRMWARE;
  sendNextMessage = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  printDebug(mac, "Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  printDebug(mac, "Subscribe acknowledged.");
  printDebug(mac, "  packetId: ");
  printDebug(mac, String(packetId));
  printDebug(mac, "  qos: ");
  printDebug(mac, String(qos));
}

void onMqttUnsubscribe(uint16_t packetId) {
  printDebug(mac, "Unsubscribe acknowledged.");
  printDebug(mac, "  packetId: ");
  printDebug(mac, String(packetId));
}

void onMqttPublish(uint16_t packetId) {
  sendNextMessage = true;
  if (lastMessageType == 0) {
    totalSentImu += 1;
  }
  else if (lastMessageType == 1) {
    totalSentGps += 1;
  }
  if (flushState == READING_FROM_FLASH || flushState == CHECK_FLASH_FLUSH_STATUS || flushState == FLUSH_TX_QUEUE || flushState == FLUSH_COUNTERS)
  {
    return;
  }
  else
  {
    incrementTxReadPtr();
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (topic == "rb32/upload")
  {
    flushState ==  UPLOAD_LOG_FILE;
  }
}
void dumpDataToFlash() {
  mqttClient.publish("rb32/imu/debug", 0, true, "Dumping data to flash");
  if (!f2) {
    mqttClient.publish("rb32/imu/debug", 0, true, "Warning! Unable to open log file for dumping");
    return;
  }

  String rdPtr(txqReadPtr);
  String wrPtr(txqWritePtr);
  String t = "TxQ Write Ptr: " + wrPtr + ", TxQ Read Ptr: " + rdPtr;
  mqttClient.publish("rb32/imu/debug", 0, true, t.c_str());

  int elementsToWrite = 0;
  int elementsWritten = 0;
  int confirmedWritten = 0;
  if (txqReadPtr < txqWritePtr) {
    // If no wrap-around, write the data in a single operation
    elementsToWrite = txqWritePtr - txqReadPtr;
    confirmedWritten += f2.write((uint8_t*)&txQueue[txqReadPtr], sizeof(TxQueue) * elementsToWrite);
    elementsWritten += elementsToWrite;
  } else {
    // If wrap-around occurred, write the data in two parts
    // First, from readIndex to the end of the buffer
    elementsToWrite = TXQUEUE_SIZE - txqReadPtr;
    confirmedWritten += f2.write((uint8_t*)&txQueue[txqReadPtr], sizeof(TxQueue) * elementsToWrite);
    elementsWritten += elementsToWrite;

    // Next, from the start of the buffer to writeIndex
    elementsToWrite = txqWritePtr;
    confirmedWritten += f2.write((uint8_t*)txQueue, sizeof(TxQueue) * elementsToWrite);
    elementsWritten += elementsToWrite;
  }
  String a = " bytes written to memory";
  String msg = confirmedWritten + a;
  mqttClient.publish("rb32/imu/debug", 0, true, msg.c_str());
  String sizeofTxQ(sizeof(TxQueue));
  String el2Write(elementsToWrite);
  String sz = "Size of TxQueue: " + sizeofTxQ + ", Elements to write: " + el2Write;
  mqttClient.publish("rb32/imu/debug", 0, true, sz.c_str());
  // Update readIndex to indicate that all data has been written
  txqReadPtr = ((txqReadPtr + elementsWritten) % TXQUEUE_SIZE);
  f2.flush();
  flushState = READING_FROM_FLASH;
  dumpToFlashCount += 1;
}

void addToTxQueue(int typeID, const ImuData& imuData) {
  txQueue[txqWritePtr].typeId = typeID;
  txQueue[txqWritePtr].data.imu = imuData;
  txqWritePtr = (txqWritePtr + 1) % TXQUEUE_SIZE; // increment the write pointer and wrap around if necessary
}

void addToTxQueue(int typeID, const GpsData& gpsData) {
  txQueue[txqWritePtr].typeId = typeID;
  txQueue[txqWritePtr].data.gps = gpsData;
  txqWritePtr = (txqWritePtr + 1) % TXQUEUE_SIZE; // increment the write pointer and wrap around if necessary
}

TxQueue* readFromTxQueue() {
  TxQueue* element = &txQueue[txqReadPtr]; // read the element at the current read position
  return element;
}

bool isTxQueueEmpty() {
  return txqReadPtr == txqWritePtr;
}

bool isTxQueueFull() {
  if ((txqWritePtr + 1) % TXQUEUE_SIZE == txqReadPtr) {
    mqttClient.publish("rb32/imu/debug", 0, true, "Warning! TX Queue Full!");
    //flushState = FLUSH_TX_QUEUE;
    return true;
  }
  else
    return false;
}

void incrementTxReadPtr() {
  txqReadPtr = (txqReadPtr + 1) % TXQUEUE_SIZE; // increment the read pointer and wrap around if necessary
  totalSent += 1;
}

String createLogFileName() {
  char dateBuffer[34];
  sprintf(dateBuffer, "/%s_%04d-%02d-%02d.txt", mac.c_str(), year(), month(), day());
  String s = String(dateBuffer);
  s.replace(":", "");
  return s;
}

void setup() {
  Serial.begin(9600);
  Serial.swap();
  printDebug(mac, "In setup");
  printDebug(mac, "Still in setup");
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin(sda, scl);
  MPU6050_Init();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  mac = WiFi.macAddress();

  if (LittleFS.begin()) {
    // Add optional callback notifiers
    ESPhttpUpdate.onEnd(update_finished);

    mqttClient.publish("rb32/debug", 0, true, "LOG FILENAME: ");
    mqttClient.publish("rb32/debug", 0, true, logFileName.c_str());

    logFileName = createLogFileName();
    LittleFS.remove(logFileName);
    f2 = LittleFS.open(logFileName, "w");
    //fu = LittleFS.open(logFileName, "w");

    t1LED = 0;
    t2LED = 0;
    t1FLUSH = 0;
    t2FLUSH = 0;
  }
  else {
    mqttClient.publish("rb32/debug", 0, true, "Warning! Unable to mount filesystem");
  }

  ftpClient.begin(ftpServerInfo);
}

void loop() {
  switch (flushState) {
    case UPDATE_FIRMWARE:
      if (updateFailCount < 1)
        updateFirmware();
      else
      {
        updateFailed = true;
        flushCountersCallBack.attach(2, setFlushState);
        flushState = UPDATE_COMPLETE;
      }
      break;
    case UPDATE_COMPLETE:
      flushState = DONT_FLUSH;
      break;
    case FLUSH_COUNTERS:
      flushCounters();
      break;
    case READING_FROM_RAM:
      if (WiFi.isConnected() && mqttClient.connected()) {
        publishFromTxQueue();
      }
      break;
    case READING_FROM_FLASH:
      if (WiFi.isConnected() && mqttClient.connected()) {
        mqttClient.publish("rb32/status", 0, true, "Reading from flash");
        ftpTransferStartTime = millis();
        f2.close();
        f2 = LittleFS.open(logFileName, "r");
        f2.seek(lastReadPosition);
        if (publishFromFlash() == TransferState::IN_PROGRESS)
        {
          flushState = CHECK_FLASH_FLUSH_STATUS;
        }
      }
      break;
    case CHECK_FLASH_FLUSH_STATUS:
      if (publishFromFlash() == TransferState::COMPLETE)
      {
        ftpTransferEndTime = millis();
        String m = mac + ", Upload failed after " + String(ftpTransferEndTime - ftpTransferStartTime) + "ms";
        mqttClient.publish("rb32/status", 0, true, m.c_str());
        lastReadPosition = f2.position();
        f2.close();
        f2 = LittleFS.open(logFileName, "a");
        flushState = DONT_FLUSH;
      }
      break;
    case FLUSH_TX_QUEUE:
      dumpDataToFlash();
      break;
    default:
      break;
  }

  if (flushState != CHECK_FLASH_FLUSH_STATUS && flushState != READING_FROM_FLASH && flushState != FLUSH_TX_QUEUE && flushState != FLUSH_COUNTERS)
  {
    readAndBufferImuData();
    //readAndBufferGpsData();
  }

  t2LED = millis();
  if ((t2LED - t1LED) >= 1000) {
    if (ledState == LOW) {
      ledState = HIGH;

    }
    else {
      ledState = LOW;
    }

    ftpClient.handleFTP();
    digitalWrite(LED_BUILTIN, ledState);
    t1LED = t2LED;
  }
}

void publishFromTxQueue() {
  if (sendNextMessage == true && !isTxQueueEmpty()) {
    TxQueue* tmpTxqData = readFromTxQueue();
    if (tmpTxqData->typeId == 0) {
      publishImuData(tmpTxqData->data.imu);
    }
    else if (tmpTxqData->typeId == 1) {
      publishGpsData(tmpTxqData->data.gps);
    }
  }
}

TransferState publishFromFlash() {
  TxQueue t;
  int bytesRead = 0;
  if (sendNextMessage == true)
  {
    if (f2.available())
    {
      bytesRead += f2.read((uint8_t *)&t.typeId, 1);
      f2.read();
      f2.read();
      f2.read();
      if (t.typeId == 0) {
        bytesRead += f2.read((uint8_t *)&t.data.imu, sizeof(ImuData));
        publishImuData(t.data.imu);
      }
      else if (t.typeId == 1) {
        f2.read((uint8_t *)&t.data.gps, sizeof(GpsData) + 1);
        publishGpsData(t.data.gps);
      }
      return TransferState::IN_PROGRESS;
    }
    else
      return TransferState::COMPLETE;
  }
  else
  {
    return TransferState::WAITING;
  }
}


void readAndBufferImuData() {
  imuT1 = millis();

  if (imuT1 - imuT0 >= IMU_DELAY_TIME)
  {
    rssi = WiFi.RSSI();
    Read_RawValue(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_XOUT_H);
    loopTime = millis();
    timeAge = loopTime - lastGpsTimeUpdate;
    ImuData imu;

    imu.ax = AccelX;
    imu.ay = AccelY;
    imu.az = AccelZ;
    imu.t = Temperature;
    imu.gx = GyroX;
    imu.gy = GyroY;
    imu.gz = GyroZ;
    imu.rssi = rssi;
    imu.hh = hour();
    imu.mm = minute();
    imu.ss = second();
    imu.ms = timeAge;
    imu.msgId = imuMsgId;


    if (isTxQueueFull()) {
      totalMissedImu += 1;
    }
    else {
      addToTxQueue(0, imu);
    }

    if (calculateBufferSize(txqWritePtr, txqReadPtr, TXQUEUE_SIZE) == 1398) {
      flushState = FLUSH_TX_QUEUE;
    }

    imuMsgId += 1;
    imuT0 = imuT1;
  }

  return;
}

void publishImuData(ImuData& data) {
  ImuData* tmpImuData = &data;
  //divide each with their sensitivity scale factor
  Ax = static_cast<double>(tmpImuData->ax) / AccelScaleFactor;
  Ay = static_cast<double>(tmpImuData->ay) / AccelScaleFactor;
  Az = static_cast<double>(tmpImuData->az) / AccelScaleFactor;
  T  = static_cast<double>(tmpImuData->t) / 340 + 36.53; //temperature formula
  Gx = static_cast<double>(tmpImuData->gx) / GyroScaleFactor;
  Gy = static_cast<double>(tmpImuData->gy) / GyroScaleFactor;
  Gz = static_cast<double>(tmpImuData->gz) / GyroScaleFactor;
  rssi = tmpImuData->rssi;
  dHour = tmpImuData->hh;
  dMin = tmpImuData->mm;
  dSec = tmpImuData->ss;
  timeAge = tmpImuData->ms;
  dImuMsgId = tmpImuData->msgId;
  snprintf(msg, MSG_BUFFER_SIZE, "{\"mac\":\"%s\",\"Ax\":%+f,\"Ay\":%+f,\"Az\":%f,\"T\":%f,\"Gx\":%+f,\"Gy\":%+f,\"Gz\":%f,\"rssi\":%+d,\"ecg\":%d,\"time\":\"%02d:%02d:%02d.%04u\",\"msgId\":\"%u\"}\r", mac.c_str(), Ax, Ay, Az, T, Gx, Gy, Gz, rssi, 0, dHour, dMin, dSec, timeAge, dImuMsgId);

  int rc = mqttClient.publish("imu/data", 1, true, msg);

  if (rc > 0) {
    lastPacketId = rc;
    lastMessageType = 0;
  }

  sendNextMessage = false;
}

void publishGpsData(GpsData& data) {
  GpsData* tmpGpsData = &data;
  char sendBuffer[64];
  snprintf(sendBuffer, sizeof(sendBuffer), "%s,%u,%u,%02u%02u%02u,%.4f,%.4f,%u,%u,%.2f", mac.c_str(), tmpGpsData->msgId, tmpGpsData->fromCache, tmpGpsData->hh, tmpGpsData->mm, tmpGpsData->ss, tmpGpsData->lat, tmpGpsData->lon, tmpGpsData->valid, tmpGpsData->sats, tmpGpsData->hdop);
  int rc = mqttClient.publish("gps/data", 1, true, sendBuffer);

  if (rc > 0) {
    lastPacketId = rc;
    lastMessageType = 1;
  }

  sendNextMessage = false;
}


void readAndBufferGpsData() {
  while (Serial.available() > 0) {
    rx = Serial.read();
    if (rx != '\n')
      strMsg += rx;

    if (rx == '\n')
    {
      if (strMsg.startsWith("$GNGGA"))
      {
        fromCache = 0;
        sMqttGpsMsg = mac + ',';
        sMqttGpsMsg += gpsMsgId;
        sMqttGpsMsg += ',';
        sMqttGpsMsg += fromCache;
        sMqttGpsMsg += ',';
        sMqttGpsMsg += strMsg;

        GpsData g;
        gpsTokenPosition = 0;
        char* token;
        const char* gpsMsg = sMqttGpsMsg.c_str();
        token = strtok(const_cast<char*>(gpsMsg), ",");
        while (token != NULL) {
          gpsTokenPosition++;

          if (gpsTokenPosition == 2) {
            g.msgId = static_cast<uint16_t>(atoi(token));                      // message ID
          } else if (gpsTokenPosition == 3) {
            g.fromCache = static_cast<uint8_t>(atoi(token));                  // fromCahce
          } else if (gpsTokenPosition == 5) {
            String s = String(token);                   // time
            g.hh = atoi(s.substring(0, 2).c_str());     // hh
            g.mm = atoi(s.substring(2, 4).c_str());     // mm
            g.ss = atoi(s.substring(4, 6).c_str());     // ss
          } else if (gpsTokenPosition == 6) {
            g.lat = atof(token);
          } else if (gpsTokenPosition == 8) {
            g.lon = atof(token);
          } else if (gpsTokenPosition == 10) {
            g.valid = static_cast<uint8_t>(atoi(token));              // valid
          } else if (gpsTokenPosition == 11) {
            g.sats = static_cast<uint8_t>(atoi(token));               // sats
          } else if (gpsTokenPosition == 12) {
            g.hdop = atof(token);               // hdop
          }

          token = strtok(NULL, ",");
        }

        if (isTxQueueFull()) {
          totalMissedGps += 1;
        }
        else {
          addToTxQueue(1, g);
        }

        gpsMsgId += 1;
      }

      //parse out the time so we can set the wemos clock
      if (strMsg.startsWith("$GNZDA"))
      {

        szHour[0]  = strMsg.charAt(7);
        szHour[1]  = strMsg.charAt(8);
        szHour[2]  = '\0';
        szMins[0]  = strMsg.charAt(9);
        szMins[1]  = strMsg.charAt(10);
        szMins[2]  = '\0';
        szSec[0]   = strMsg.charAt(11);
        szSec[1]   = strMsg.charAt(12);
        szSec[2]  = '\0';
        szDay[0]   = strMsg.charAt(18);
        szDay[1]   = strMsg.charAt(19);
        szDay[2]  = '\0';
        szMonth[0] = strMsg.charAt(21);
        szMonth[1] = strMsg.charAt(22);
        szMonth[2]  = '\0';
        szYear[0]  = strMsg.charAt(24);
        szYear[1]  = strMsg.charAt(25);
        szYear[2]  = strMsg.charAt(26);
        szYear[3]  = strMsg.charAt(27);
        szYear[4]  = '\0';

        lastGpsTimeUpdate = millis();

        setTime(atoi(szHour), atoi(szMins), atoi(szSec), atoi(szDay), atoi(szMonth), atoi(szYear));
      }

      sMqttGpsMsg = "";
      strMsg = "";
      gpsTime = "";
      timeAgeStr = "";
      break;
    }
  }
  return;
}

void updateFirmware() {
  ESPhttpUpdate.closeConnectionsOnUpdate(false);
  t_httpUpdate_return ret = ESPhttpUpdate.update(UPDATE_SERVER, 80, "/rb32update", VERSION);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      update_failed();
      break;

    case HTTP_UPDATE_NO_UPDATES:
      flushState = UPDATE_COMPLETE;
      update_finished();
      break;

    case HTTP_UPDATE_OK:
      flushState = UPDATE_COMPLETE;
      update_finished();
      break;

    default:
      update_failed();
      break;
  }
}

void update_failed() {
  flushState = UPDATE_FIRMWARE;
  String m;
  m = mac + "," + VERSION + "," + "failed to contact update server";
  mqttClient.publish("rb32/data/fail", 0, true, m.c_str());
  updateFailCount += 1;
}


void update_finished() {
  flushState = UPDATE_COMPLETE;
  updateFailed = false;
  String m;
  m = mac + "," + VERSION + ',' + "update complete";
  pubCode = mqttClient.publish("rb32/data/fail", 0, true, m.c_str());
  //flusherCallBack.attach(5, initiateFlush);
  flushCountersCallBack.attach(2, setFlushState);
}

void setFlushState()
{
  prevFlushState = flushState;
  flushState = FLUSH_COUNTERS;
}

void flushCounters()
{
  char buff[256];

  txBufferSize  = calculateBufferSize(txqWritePtr, txqReadPtr, TXQUEUE_SIZE);

  snprintf(buff, sizeof(buff), "%s,%d,%d,%d,%d,%d,%d, txqWritePtr: %u, txqReadPtr: %u, gpsWritePtr: %u, gpsReadPtr: %u, imuWritePtr: %u, imuReadPtr: %u", mac.c_str(), totalMissedImu, totalSentImu, totalMissedGps, totalSentGps, txBufferSize, totalSent, txqWritePtr, txqReadPtr, gpsWritePtr, gpsReadPtr, imuWritePtr, imuReadPtr);
  mqttClient.publish("rb32/counters", 0, true, buff);
  flushState = prevFlushState;
}

int calculateBufferSize(int writePos, int readPos, int qSize) {
  return ((writePos - readPos) < 0) ? ((writePos - readPos) + qSize) : (writePos - readPos);
}

void I2C_Write(uint8_t deviceAddress, uint8_t regAddress, uint8_t data) {
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.write(data);
  Wire.endTransmission();
}

// read all 14 register
void Read_RawValue(uint8_t deviceAddress, uint8_t regAddress) {
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, (uint8_t)14);
  AccelX = (((int16_t)Wire.read() << 8) | Wire.read());
  AccelY = (((int16_t)Wire.read() << 8) | Wire.read());
  AccelZ = (((int16_t)Wire.read() << 8) | Wire.read());
  Temperature = (((int16_t)Wire.read() << 8) | Wire.read());
  GyroX = (((int16_t)Wire.read() << 8) | Wire.read());
  GyroY = (((int16_t)Wire.read() << 8) | Wire.read());
  GyroZ = (((int16_t)Wire.read() << 8) | Wire.read());
}

//configure MPU6050
void MPU6050_Init() {
  delay(150);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SMPLRT_DIV, 0x07);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_1, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_2, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_CONFIG, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_GYRO_CONFIG, 0x00);//set +/-250 degree/second full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_CONFIG, 0x00);// set +/- 2g full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_FIFO_EN, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_INT_ENABLE, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SIGNAL_PATH_RESET, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_USER_CTRL, 0x00);
}
