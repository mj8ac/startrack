
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

const char* host = "OTA-LEDS";

int led_pin = 13;
#define N_DIMMERS 3
int dimmer_pin[] = {14, 5, 15};

byte    mac[6];
char    macAddr[12];
String  rxData;
String  jsonStr;
String  gpsJson;
String  macJson;
char    rxChar;
bool    newScen = false;
bool    updateFlag = false;
bool    GPGGA   = false;

// The wifi object
WiFiClient espClient;
// The MQTT initialisation
PubSubClient client(espClient);


void setup()
{
  Serial.begin(GPSBaud);

   /* switch on led */
   pinMode(led_pin, OUTPUT);
   digitalWrite(led_pin, LOW);
  
  setup_wifi();
  /* switch off led */
  digitalWrite(led_pin, HIGH);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();

  /* configure dimmers, and OTA server events */
  analogWriteRange(1000);
  analogWrite(led_pin,990);

  for (int i=0; i<N_DIMMERS; i++)
  {
    pinMode(dimmer_pin[i], OUTPUT);
    analogWrite(dimmer_pin[i],50);
  }

  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
                        for(int i=0; i<N_DIMMERS;i++)
                          analogWrite(dimmer_pin[i], 0);
                          analogWrite(led_pin,0);
                    });

  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
                          for (int i=0;i<30;i++)
                          {
                            analogWrite(led_pin,(i*100) % 1001);
                            delay(50);
                          }
                          client.publish("gps/log1/status", "Update complete...");
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
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (client.connect("ESP8266 Client")) {
      // ... and subscribe to topic
      client.subscribe("gps/update");
    } else {
      delay(5000);
    }
  }
}


void loop()
{
  client.loop();
  //client.publish("gps/log1/status", "I'm alive");

  if (updateFlag == true){
    client.publish("gps/log1/status", "Preparing for update...");
    for (int i=0; i<1000; i++){
      ArduinoOTA.handle();
      delay(10);
    }
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
            client.publish("gps/data", jsonStr.c_str());
            GPGGA = false;
          }
          rxData = "";
          jsonStr = "";
        }
      }
    }
  }
}
