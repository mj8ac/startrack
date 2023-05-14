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

const char* VERSION = "4.0.23";

uint8_t gpsTokenPosition = 0;

int  ecg          = 0;
int  rssi         = 0;
int  pubCode      = 0;
int  logPosition1 = 0;
int  logPosition2 = 0;
int  logCounter1  = 0;
int  logCounter2  = 0;
int  totalMissedLog = 0;
int  totalMissedGps = 0;
int  totalMissedImu = 0;
int  totalSentLog   = 0;
int  totalSentGps   = 0;
int  totalSentImu   = 0;
int  fromCache = 0;
int  updateFailCount = 0;
long lastBlink      = 0;

long timeNow    = 0;
long loopTime   = 0;
long timeAge    = 0;
long lastGpsTimeUpdate  = 0;
long IMU_DELAY_TIME     = 40;
long FLUSH_DELAY_TIME   = 10;
unsigned long imuMsgId  = 0;
unsigned long gpsMsgId  = 0;

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

unsigned int t1LED, t2LED, t1FLUSH, t2FLUSH;
int ledState = LOW;

bool flushingFile1        = false;
bool flushingFile2        = false;
bool writeIntoFlushFile1  = true;
bool updateFailed = true;
bool firstTransmit = true;
bool sendNextMessage = true;


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
  uint16_t lat_ddmm;
  uint16_t lat_mmmmm;
  uint16_t lon_dddmm;
  uint16_t lon_mmmmm;
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
  uint32_t msgId;
};

const uint16_t GPS_BUFFER_SIZE = 1600;
const uint16_t IMU_BUFFER_SIZE = 100000;
GpsData gpsQueue[GPS_BUFFER_SIZE];
ImuData imuQueue[IMU_BUFFER_SIZE];

uint16_t gpsReadPtr  = 0;
uint16_t gpsWritePtr = 0;
uint16_t imuReadPtr  = 0;
uint16_t imuWritePtr = 0;

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
  }

  //  if (lastPacketId == packetId) {
  //      if (!isGpsBufferEmpty())
  //      {
  //          GpsData* tmpGpsData = readFromGpsBuffer();
  //          char sendBuffer[64];
  //          snprintf(sendBuffer, sizeof(sendBuffer), "%s,%u,%u,%u%u%u,%f,%f,%u,%u,%f", mac.c_str(), tmpGpsData->msgId, tmpGpsData->fromCache, tmpGpsData->hh, tmpGpsData->mm, tmpGpsData->ss, tmpGpsData->lat, tmpGpsData->lon, tmpGpsData->valid, tmpGpsData->sats, tmpGpsData->hdop);
  //          lastPacketId = mqttClient.publish("gps/data", 1, true, msg);
  //      }
  //  }


}

void writeToGpsBuffer(const GpsData& element) {
  gpsQueue[gpsWritePtr] = element; // write the element to the current write position
  gpsWritePtr = (gpsWritePtr + 1) % GPS_BUFFER_SIZE; // increment the write pointer and wrap around if necessary
}

GpsData* readFromGpsBuffer() {
  GpsData* element = &gpsQueue[gpsReadPtr]; // read the element at the current read position
  gpsReadPtr = (gpsReadPtr + 1) % GPS_BUFFER_SIZE; // increment the read pointer and wrap around if necessary
  return element;
}

bool isGpsBufferEmpty() {
  return gpsReadPtr == gpsWritePtr; // the buffer is empty if the read and write pointers are equal
}

bool isGpsBufferFull() {
  return (gpsWritePtr + 1) % GPS_BUFFER_SIZE == gpsReadPtr; // the buffer is full if the next write position is equal to the read position
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

  int tend = 0;
  int tstart = 0;
  tstart = millis();

  switch (flushState) {
    case UPDATE_FIRMWARE:
      if (updateFailCount < 5)
        updateFirmware();
      else
      {
        updateFailed = true;
        //flusherCallBack.attach(5, initiateFlush);
        flushCountersCallBack.attach(2, setFlushState);
        flushState = UPDATE_COMPLETE;
        sendNextMessage = true;
      }
      break;
    default:
      break;
  }

  //readAndPublishImuData();
  readAndPublishGpsData();

  if (gpsWritePtr > 1500) {
    if (sendNextMessage == true && !isGpsBufferEmpty()) {
      GpsData* tmpGpsData = readFromGpsBuffer();
      char sendBuffer[64];
      snprintf(sendBuffer, sizeof(sendBuffer), "%s,%u,%u,%02u%02u%02u,%04d.%05d,%05d.%05d,%u,%u,%f", mac.c_str(), tmpGpsData->msgId, tmpGpsData->fromCache, tmpGpsData->hh, tmpGpsData->mm, tmpGpsData->ss, tmpGpsData->lat_ddmm, tmpGpsData->lat_mmmmm, tmpGpsData->lon_dddmm, tmpGpsData->lon_mmmmm, tmpGpsData->valid, tmpGpsData->sats, tmpGpsData->hdop);
      int rc = mqttClient.publish("gps/data", 1, true, sendBuffer);

      if (rc > 0){
        lastPacketId = rc;
      }
      
      sendNextMessage = false;

      char dbg[32];
      snprintf(dbg, sizeof(dbg), "Sent gps msg: %u", lastPacketId);
      mqttClient.publish("rb32/debug", 0, true, dbg);
    }
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
  tend = millis();
  if (tend - tstart > 40) {
    //Serial.print("Loop time: ");
    //Serial.println(tend - tstart);
  }
}

void readAndPublishImuData() {
  imuT1 = millis();

  if (imuT1 - imuT0 >= IMU_DELAY_TIME)
  {
    rssi = WiFi.RSSI();
    Read_RawValue(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_XOUT_H);
    //divide each with their sensitivity scale factor
    Ax = (double)AccelX / AccelScaleFactor;
    Ay = (double)AccelY / AccelScaleFactor;
    Az = (double)AccelZ / AccelScaleFactor;
    T  = (double)Temperature / 340 + 36.53; //temperature formula
    Gx = (double)GyroX / GyroScaleFactor;
    Gy = (double)GyroY / GyroScaleFactor;
    Gz = (double)GyroZ / GyroScaleFactor;
    ecg = analogRead(a0);
    loopTime = millis();
    timeAge = loopTime - lastGpsTimeUpdate;
    imuMsgId += 1;
    //int copiedBytes = snprintf(msg, MSG_BUFFER_SIZE, "{\"mac\":\"%s\",\"Ax\":%+08.3f,\"Ay\":%+08.3f,\"Az\":%+08.3f,\"T\":%+08.3f,\"Gx\":%+08.3f,\"Gy\":%+08.3f,\"Gz\":%+08.3f,\"rssi\":%+d,\"ecg\":%d,\"time\":\"%02d:%02d:%02d.%04lu\"}\r", mac.c_str(), Ax, Ay, Az, T, Gx, Gy, Gz, rssi, 0, hour(), minute(), second(), timeAge);
    int copiedBytes = snprintf(msg, MSG_BUFFER_SIZE, "{\"mac\":\"%s\",\"Ax\":%+f,\"Ay\":%+f,\"Az\":%f,\"T\":%f,\"Gx\":%+f,\"Gy\":%+f,\"Gz\":%f,\"rssi\":%+d,\"ecg\":%d,\"time\":\"%02d:%02d:%02d.%04lu\",\"msgId\":\"%lu\"}\r", mac.c_str(), Ax, Ay, Az, T, Gx, Gy, Gz, rssi, 0, hour(), minute(), second(), timeAge, imuMsgId);
#ifdef PRINT_DEBUG_MSGS
    Serial.print("Ax: "); Serial.print(Ax);
    Serial.print(" Ay: "); Serial.print(Ay);
    Serial.print(" Az: "); Serial.print(Az);
    Serial.print(" T: "); Serial.print(T);
    Serial.print(" Gx: "); Serial.print(Gx);
    Serial.print(" Gy: "); Serial.print(Gy);
    Serial.print(" Gz: "); Serial.println(Gz);
    Serial.println(msg);
#endif
    pubCode = mqttClient.publish("imu/data", 1, true, msg);

    if (pubCode < 1) {
      if (writeIntoFlushFile1 == true) {
        int writtenBytes = f1.write(msg, copiedBytes);
        logCounter1++;
      }
      else {
        int writtenBytes = f2.write(msg, copiedBytes);
        logCounter2++;
      }
      totalMissedImu++;
    }
    totalSentImu++;
    imuT0 = imuT1;
  }
}

void readAndPublishGpsData() {
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
            g.msgId = atoi(token);                      // message ID
          } else if (gpsTokenPosition == 3) {
            g.fromCache = atoi(token);                  // fromCahce
          } else if (gpsTokenPosition == 5) {
            String s = String(token);                   // time
            g.hh = atoi(s.substring(0, 2).c_str());     // hh
            g.mm = atoi(s.substring(2, 4).c_str());     // mm
            g.ss = atoi(s.substring(4, 6).c_str());     // ss
          } else if (gpsTokenPosition == 6) {
            String lat = String(token);
            g.lat_ddmm = static_cast<uint16_t>(lat.substring(0, 4).toInt());
            g.lat_mmmmm = static_cast<uint16_t>(lat.substring(5).toInt());               // lat
          } else if (gpsTokenPosition == 8) {
            String lon = String(token);
            g.lon_dddmm = static_cast<uint16_t>(lon.substring(0, 5).toInt());
            g.lon_mmmmm = static_cast<uint16_t>(lon.substring(6).toInt());
          } else if (gpsTokenPosition == 10) {
            g.valid = static_cast<uint8_t>(atoi(token));              // valid
          } else if (gpsTokenPosition == 11) {
            g.sats = static_cast<uint8_t>(atoi(token));               // sats
          } else if (gpsTokenPosition == 12) {
            g.hdop = atof(token);               // hdop
          }

          token = strtok(NULL, ",");
        }

        writeToGpsBuffer(g);
        mqttClient.publish("gps/data", 0, true, "written to gps buffer");
        char x[64];
        snprintf(x, sizeof(x), "sendNextMessage = %d, gpsWritePtr: %u", sendNextMessage, gpsWritePtr);
        mqttClient.publish("gps/data", 0, true, x);

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
  int totalCached = 0;
  int totalSent   = 0;

  totalCached = logCounter1 + logCounter2;
  totalSent = totalSentImu + totalSentGps + totalSentLog;

  snprintf(buff, sizeof(buff), "%s,%d,%d,%d,%d,%d,%d,%d, %d", mac.c_str(), totalMissedImu, totalSentImu, totalMissedGps, totalSentGps, totalCached, totalSentLog, totalMissedLog, totalSent);
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
