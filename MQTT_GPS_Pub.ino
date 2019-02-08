
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// GPS setup
static const uint32_t GPSBaud = 9600;

// Connect to the WiFi
const char* ssid = "ST01";
const char* password = "J9a5cxec";
const char* mqtt_server = "192.168.42.1";
//const char* mqtt_server = "192.168.1.108";

char host[22];

int led_pin = 13;
#define N_DIMMERS 3
int dimmer_pin[] = {14, 5, 15};
int ackCode = -1;
float volt = 0.0;
unsigned int raw =0;
const int sleepTimeS = 60;
byte    mac[6];
char    macAddr[12];
String  rxData;
String  jsonStr;
String  gpsJson;
String  macJson;
char    rxChar;
bool    newScen     = false;
bool    updateFlag  = false;
bool    GPGGA       = false;
bool    deepSleep   = false;

// The wifi object
WiFiClient espClient;
// The MQTT initialisation
PubSubClient client(espClient);


void setup()
{
  Serial.begin(GPSBaud);

   /* switch on led */
   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(A0, INPUT);
   digitalWrite(LED_BUILTIN, LOW);
  setup_wifi();
  /* switch off led */
  digitalWrite(LED_BUILTIN, HIGH);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();

  /* configure dimmers, and OTA server events */
  analogWriteRange(1000);
  analogWrite(LED_BUILTIN,990);

  for (int i=0; i<N_DIMMERS; i++)
  {
    pinMode(dimmer_pin[i], OUTPUT);
    analogWrite(dimmer_pin[i],50);
  }

  sprintf(host, "OTA-LEDS-%s",macAddr); 
  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
                        for(int i=0; i<N_DIMMERS;i++)
                          analogWrite(dimmer_pin[i], 0);
                          analogWrite(LED_BUILTIN,0);
                    });

  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
                          for (int i=0;i<30;i++)
                          {
                            analogWrite(LED_BUILTIN,(i*100) % 1001);
                            delay(50);
                          }
                          client.publish("gps/status", "Update complete...");
                          updateFlag = false;
                        });

   ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); });

   /* setup the OTA server */
   ArduinoOTA.begin();
}

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  WiFi.macAddress(mac);

  int i;
  int j;
  j = 0;
  for (i = 5; i >= 0; i--) {
    sprintf(macAddr + (j * 2), "%02X", mac[i]);
    j++;
  }
}

void resetWifi(){
  WiFi.begin(ssid, password);
  return;  
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic,"gps/update")==0){
    updateFlag = true;
  }
  if (strcmp(topic,"gps/update/off")==0){
    updateFlag = false;
  }
  if (strcmp(topic,"loggers/off")==0){
    deepSleep = true;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (client.connect(host)) {
      // ... and subscribe to topic
      client.subscribe("gps/update");
      client.subscribe("gps/update/off");
      client.subscribe("loggers/off");
    } else {
      delay(5000);
    }
  }
}


void loop()
{
  client.loop();
  raw = analogRead(A0);
  volt = raw/1023;
  volt=volt*4.2;
  if (updateFlag == true){
    client.publish("gps/status", "Preparing for update...");
    for (int i=0; i<1000; i++){
      ArduinoOTA.handle();
      delay(10);
    }
  }

  if (deepSleep == true){
    client.publish("gps/status", "Going into deep sleep");
    ESP.deepSleep(sleepTimeS*1000000);
  }
  
  while (Serial.available() > 0) {
      if (WiFi.status() != WL_CONNECTED){
      setup_wifi();
      }
  
  if (!client.connected()) {
      reconnect();
    }
    
    rxChar = Serial.read();
    //rxData += rxChar;
    
    if (rxChar == '$') {
      rxData += rxChar;
      newScen = true;
    }
    
    else {
      if (newScen) {
        if (rxChar != '\n'){
          rxData += rxChar;
        }
        if (rxData == "$GPGGA") {
          GPGGA = true;
        }
        if (rxChar == '\n') {
          if (GPGGA) {
            jsonStr += macAddr; 
            jsonStr += "," + rxData;
            String v = String(raw);
            ackCode = client.publish("gps/data", jsonStr.c_str());
            client.publish("logger/voltage", v.c_str());
            while (ackCode != 1){
              client.publish("gps/data", "PUBLISH FAILED retrying...");
              ackCode = client.publish("gps/data", jsonStr.c_str());
            }
            ackCode = 0;
            GPGGA = false;
          }
          rxData = "";
          jsonStr = "";
        }
      }
    }
  }
}
