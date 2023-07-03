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

const char* VERSION = "4.0.24";

uint8_t gpsTokenPosition = 0;

uint8_t dHour = 0;
uint8_t dMin = 0;
uint8_t dSec = 0;
uint32_t dImuMsgId;

int  ecg          = 0;
int  rssi         = 0;
int  pubCode      = 0;
int  logPosition1 = 0;
int  logPosition2 = 0;
int  logCounter1  = 0;
int  logCounter2  = 0;

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
File f1, f2;

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
  FLUSH_COUNTERS
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

typedef struct {
  uint16_t typeId;
  uint16_t index;
} TxQueue;

const uint16_t GPS_BUFFER_SIZE = 60;
const uint16_t IMU_BUFFER_SIZE = 1000;
const uint16_t TXQUEUE_SIZE = 1400;
GpsData gpsQueue[GPS_BUFFER_SIZE];
ImuData imuQueue[IMU_BUFFER_SIZE];
TxQueue txQueue[TXQUEUE_SIZE];

uint16_t gpsReadPtr  = 0;
uint16_t gpsWritePtr = 0;
uint16_t imuReadPtr  = 0;
uint16_t imuWritePtr = 0;
uint16_t txqReadPtr = 0;
uint16_t txqWritePtr = 0;

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
  printDebug(mac, "Subscribing at QoS 2, packetId: ");
  printDebug(mac, String(packetIdSub));
  mqttClient.publish("rb32/debug", 0, true, "test 1");
  printDebug(mac, "Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish("rb32/debug", 1, true, "test 2");
  printDebug(mac, "Publishing at QoS 1, packetId: ");
  printDebug(mac, String(packetIdPub1));
  uint16_t packetIdPub2 = mqttClient.publish("rb32/debug", 2, true, "test 3");
  printDebug(mac, "Publishing at QoS 2, packetId: ");
  printDebug(mac, String(packetIdPub2));

  String m;
  m = mac + "," + VERSION + "," + "ONLINE";
  mqttClient.publish("rb32/data/status", 0, true, m.c_str());
  flushState = UPDATE_FIRMWARE;
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
  if (static_cast<uint16_t>(lastPacketId) == packetId) {
    sendNextMessage = true;
    incrementTxReadPtr();
    if (lastMessageType == 0) {
      incrementImuReadPtr();
    }
    else if (lastMessageType == 1) {
      incrementGpsReadPtr();
    }
  }
}

//===========================================================================================================================================
//void writeToTxQueue(ImuData*  ptrImudata) {
//  if (ptrImudata){
//      txQueue[txqWritePtr].typeId = 0;
//      txQueue[txqWritePtr].data = static_cast<void*>(ptrImudata); // write the element to the current write position
//      txqWritePtr = (txqWritePtr + 1) % (TXQUEUE_SIZE); // increment the write pointer and wrap around if necessary
//  }
//
//}

void addToTxQueue(int typeID, int index) {
  String writePtr(txqWritePtr);
  writePtr = "Write Position: " + writePtr;
  //mqttClient.publish("rb32/debug", 0, true, writePtr.c_str());
  txQueue[txqWritePtr].typeId = typeID;
  txQueue[txqWritePtr].index = index; // write the element to the current write position
  txqWritePtr = (txqWritePtr + 1) % TXQUEUE_SIZE; // increment the write pointer and wrap around if necessary
}

TxQueue* readFromTxQueue() {
  String readPtr(txqReadPtr);
  readPtr = "Read Position: " + readPtr;
  //mqttClient.publish("rb32/debug", 0, true, readPtr.c_str());
  TxQueue* element = &txQueue[txqReadPtr]; // read the element at the current read position
  return element;
}

bool isTxQueueEmpty() {
  return txqReadPtr == txqWritePtr;
}

bool isTxQueueFull() {
  return (txqWritePtr + 1) % TXQUEUE_SIZE == txqReadPtr;
}

void incrementTxReadPtr() {
  txqReadPtr = (txqReadPtr + 1) % TXQUEUE_SIZE; // increment the read pointer and wrap around if necessary
  totalSent += 1;
}
//==========================================================================================================================================
int writeToImuBuffer(const ImuData& element) {
  imuQueue[imuWritePtr] = element; // write the element to the current write position
  int index = imuWritePtr;
  imuWritePtr = (imuWritePtr + 1) % IMU_BUFFER_SIZE; // increment the write pointer and wrap around if necessary
  return index;
}

ImuData* readFromImuBuffer() {
  ImuData* element = &imuQueue[imuReadPtr]; // read the element at the current read position

  return element;
}

bool isImuBufferEmpty() {
  return imuReadPtr == imuWritePtr; // the buffer is empty if the read and write pointers are equal
}

bool isImuBufferFull() {
  return (imuWritePtr + 1) % IMU_BUFFER_SIZE == imuReadPtr; // the buffer is full if the next write position is equal to the read position
}

void incrementImuReadPtr() {
  imuReadPtr = (imuReadPtr + 1) % IMU_BUFFER_SIZE; // increment the read pointer and wrap around if necessary
  totalSentImu += 1;
}
//===========================================================================================================================================
int writeToGpsBuffer(const GpsData& element) {
  gpsQueue[gpsWritePtr] = element; // write the element to the current write position
  int index = gpsWritePtr;
  gpsWritePtr = (gpsWritePtr + 1) % GPS_BUFFER_SIZE; // increment the write pointer and wrap around if necessary
  return index;
}

GpsData* readFromGpsBuffer() {
  GpsData* element = &gpsQueue[gpsReadPtr]; // read the element at the current read position
  return element;
}

bool isGpsBufferEmpty() {
  return gpsReadPtr == gpsWritePtr; // the buffer is empty if the read and write pointers are equal
}

bool isGpsBufferFull() {
  return (gpsWritePtr + 1) % GPS_BUFFER_SIZE == gpsReadPtr; // the buffer is full if the next write position is equal to the read position
}

void incrementGpsReadPtr() {
  gpsReadPtr = (gpsReadPtr + 1) % GPS_BUFFER_SIZE; // increment the read pointer and wrap around if necessary
  totalSentGps += 1;
}
//===========================================================================================================================================
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

    if (LittleFS.format()) {
      //Serial.println("Formatted filesystem");
    }
    else {
      //Serial.println("Failed to format filesystem");
      f1 = LittleFS.open("/flushFile1.bin", "w");
      f2 = LittleFS.open("/flushFile2.bin", "w");
      if (!f1 || !f2) {
        //Serial.println("Failed to create flush both file!");
      }
      else {
        f1.setTimeout(100);
        f2.setTimeout(100);
      }

      //fRead = LittleFS.open("/dropped.txt", "r");
    }

    // Add optional callback notifiers
    ESPhttpUpdate.onEnd(update_finished);

    t1LED = 0;
    t2LED = 0;
    t1FLUSH = 0;
    t2FLUSH = 0;
  }
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
        //sendNextMessage = true;
      }
      break;
    case FLUSH_COUNTERS:
      flushCounters();
    default:
      break;
  }

  int index = readAndBufferImuData();
  if (index != -1) {
    if (isTxQueueFull()) {
      mqttClient.publish("rb32/imu/debug", 0, true, "Warning! TX Queue Full!");
    }
    else {
      addToTxQueue(0, index);
    }
  }

  index = readAndBufferGpsData();
  if (index != -1) {
    if (isTxQueueFull()) {
      mqttClient.publish("rb32/imu/debug", 0, true, "Warning! TX Queue Full!");
    }
    else {
      addToTxQueue(1, index);
    }
  }

  if (WiFi.isConnected() && mqttClient.connected()) {
    //publishGpsData();
    //publishImuData();
    publishFromTxQueue();
  }

  t2LED = millis();
  if ((t2LED - t1LED) >= 1000) {
    if (ledState == LOW) {
      ledState = HIGH;

    }
    else {
      ledState = LOW;
    }

    digitalWrite(LED_BUILTIN, ledState);
    t1LED = t2LED;
  }
}

void publishFromTxQueue() {
  if (sendNextMessage == true && !isTxQueueEmpty()) {
    TxQueue* tmpTxqData = readFromTxQueue();
    if (tmpTxqData->typeId == 0) {
      publishImuData(tmpTxqData->index);
    }
    else if (tmpTxqData->typeId == 1) {
      publishGpsData(tmpTxqData->index);
    }
  }
}


int readAndBufferImuData() {
  imuT1 = millis();
  int rc = -1;
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

    if (isImuBufferFull()) {
      totalMissedImu += 1;
    }
    else {
      rc = writeToImuBuffer(imu);
    }

    imuMsgId += 1;
    imuT0 = imuT1;
  }

  return rc;
}

void publishImuData(int index) {
  ImuData* tmpImuData = &imuQueue[index];;
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

void publishGpsData(int index) {
  GpsData* tmpGpsData = &gpsQueue[index];
  char sendBuffer[64];
  snprintf(sendBuffer, sizeof(sendBuffer), "%s,%u,%u,%02u%02u%02u,%.4f,%.4f,%u,%u,%.2f", mac.c_str(), tmpGpsData->msgId, tmpGpsData->fromCache, tmpGpsData->hh, tmpGpsData->mm, tmpGpsData->ss, tmpGpsData->lat, tmpGpsData->lon, tmpGpsData->valid, tmpGpsData->sats, tmpGpsData->hdop);
  int rc = mqttClient.publish("gps/data", 1, true, sendBuffer);

  if (rc > 0) {
    lastPacketId = rc;
    lastMessageType = 1;
  }

  sendNextMessage = false;

  char dbg[32];
  snprintf(dbg, sizeof(dbg), "Sent gps msg: %u", lastPacketId);
  //mqttClient.publish("rb32/debug", 0, true, dbg);
}


int readAndBufferGpsData() {
  int rc = -1;
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

        if (isGpsBufferFull()) {
          totalMissedGps += 1;
        }
        else {
          rc = writeToGpsBuffer(g);
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
  return rc;
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
  char buff[64];

  txBufferSize = (txqWritePtr - txqReadPtr) % TXQUEUE_SIZE;
  gpsBufferSize = (gpsWritePtr - gpsReadPtr) % GPS_BUFFER_SIZE;
  imuBufferSize = (imuWritePtr - imuReadPtr) % IMU_BUFFER_SIZE;

  snprintf(buff, sizeof(buff), "%s,%d,%d,%d,%d,%d,%d,%d, %d", mac.c_str(), totalMissedImu, totalSentImu, totalMissedGps, totalSentGps, txBufferSize, gpsBufferSize, imuBufferSize, totalSent);
  mqttClient.publish("rb32/counters", 0, true, buff);
  flushState = prevFlushState;
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
