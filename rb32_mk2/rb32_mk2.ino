#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFiMulti.h>

#define MSG_BUFFER_SIZE (200)
//#define PRINT_DEBUG_MSGS

const char* VERSION = "2.0.8";

ESP8266WiFiMulti wifiMulti;

const char* MQTT_SERVER_PI    = "192.168.42.1";
const char* MQTT_SERVER_JONNY = "192.168.1.201";
const char* MQTT_SERVER_DAVE  = "192.168.1.201";
int         MQTT_PORT         = 1883;

const char* UPDATE_SERVER = "138.68.160.221"; // Digital Ocean Droplet

char msg[MSG_BUFFER_SIZE];
char gps[MSG_BUFFER_SIZE];
char szDay[3];
char szMonth[3];
char szYear[5];
char szHour[3];
char szMins[3];
char szSec[3];
char szTime[12];
char rx;

bool bZda        = false;

byte mac[6];
int  sz = 0;
int  ecg = 0;
int  ledTimer = 0;

String heartRate;
String strMsg;
String sMac;
String sMqttGpsMsg;
String clientId;
String gpsTime;
String timeAgeStr;

time_t sysTime = 0;

WiFiClient espClient;
PubSubClient client(espClient);

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
unsigned long lastGpsTimeUpdate = 0;
unsigned long loopTime = 0;
unsigned long timeAge = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP("ST01", "J9a5cxec");
  wifiMulti.addAP("EE-CCA1QT", "H74eMm9rCighmr");
  wifiMulti.addAP("EE-Hub-UNq9", "coat-tag-CUBIC");
  
  sMac = WiFi.macAddress();
  clientId = "RB32-" + sMac;
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    wifiMulti.run();
    #ifdef PRINT_DEBUG_MSGS
    Serial.print(".");
    #endif
  }

  #ifdef PRINT_DEBUG_MSGS
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  for (int i = 0; i < sizeof(mac); i++)
  {
    Serial.print(mac[i], HEX);
  }
  Serial.println("");
  #endif
}

void callback(char* topic, byte* payload, unsigned int length) {
if ((char)payload[0] == '1')
{
  #ifdef PRINT_DEBUG_MSGS
  Serial.print("Resetting");
  #endif
  ESP.restart();
}
}

// yes, there are 3 nested if/else statements here. Don't judge me. 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    #ifdef PRINT_DEBUG_MSGS
    Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      #ifdef PRINT_DEBUG_MSGS
      Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      client.subscribe("rb32/restart");
    } else {
      #ifdef PRINT_DEBUG_MSGS
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif

      // Try another broker
      client.setServer(MQTT_SERVER_DAVE, MQTT_PORT);
      if (client.connect(clientId.c_str())) {
      #ifdef PRINT_DEBUG_MSGS
      Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      client.subscribe("rb32/restart");
    }
    else {
      #ifdef PRINT_DEBUG_MSGS
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif

      // Wait 5 seconds before retrying
      client.setServer(MQTT_SERVER_JONNY, MQTT_PORT);
      if (client.connect(clientId.c_str())) {
      #ifdef PRINT_DEBUG_MSGS
      Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      client.subscribe("rb32/restart");
      }
      digitalWrite(LED_BUILTIN, HIGH); // LED OFF
      yield();
      delay(5000);
      digitalWrite(LED_BUILTIN, LOW); // LED ON
    }
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // LED ON
  Serial.begin(9600);
  Serial.swap();
  Wire.begin(sda, scl);
  MPU6050_Init();
  setup_wifi();
  client.setServer(MQTT_SERVER_PI, MQTT_PORT);
  client.setCallback(callback);
  ESPhttpUpdate.update(UPDATE_SERVER, 80, "/rb32update", VERSION);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  wifiMulti.run();

  digitalWrite(LED_BUILTIN, HIGH); // LED OFF
  if (ledTimer > 50)
  {
    digitalWrite(LED_BUILTIN, LOW); // LED ON
    ledTimer = 0;
  }
  ledTimer++;

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
  snprintf(msg, MSG_BUFFER_SIZE, "{\"mac\":\"%s\",\"Ax\":%f,\"Ay\":%f,\"Az\":%f,\"T\":%f,\"Gx\":%f,\"Gy\":%f,\"Gz\":%f,\"ecg\":%d,\"time\":%d%d%d.%lu}",sMac.c_str(), Ax, Ay, Az, T, Gx, Gy, Gz,ecg,hour(),minute(),second(), timeAge);

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
  client.publish("imu/data", msg);
  
  while (Serial.available() > 0) {
      rx = Serial.read();
      strMsg += rx;
      yield();
 
  if (rx == '\n')
    {
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
          
          setTime(atoi(szHour),atoi(szMins),atoi(szSec),atoi(szDay),atoi(szMonth),atoi(szYear));

      }

      if (strMsg.startsWith("$GNGGA"))
      {
      //sysTime = now();
      //gpsTime = String(sysTime);
      
      //timeAgeStr = String(timeAge);
      sMqttGpsMsg = "{\"mac\": \"" + sMac + "\",\"gps\": \"" + strMsg + "\"" + "}";
      client.publish("gps/data", sMqttGpsMsg.c_str());
      }
      //sMqttGpsMsg = sMac + strMsg;
      //client.publish("gps/data", sMqttGpsMsg.c_str());
      sMqttGpsMsg = "";
      strMsg = "";
      gpsTime = "";
      timeAgeStr = "";
      break;
      //}

    }
  }
    delay(35);
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
