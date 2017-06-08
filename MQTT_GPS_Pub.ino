#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// GPS setup
static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;

// Connect to the WiFi
const char* ssid = "VM4010643";
const char* password = "Xj3jzhqpjLsr";
const char* mqtt_server = "192.168.0.16";
byte mac[6];
char macAddr[12];
String rxData;
char rxChar;
bool newScen = false;
bool GPGGA = false;

// The wifi object
WiFiClient espClient;
// The MQTT initialisation
PubSubClient client(espClient);
// The TinyGPS++ object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial swSer(RXPin, TXPin);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (!WiFi.connected()) {
      WiFi.connect()
    }
    else {
      if (client.connect("ESP8266 Client")) {
        Serial.println("connected");
        // ... and subscribe to topic
        client.subscribe("test/topic");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }

  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  swSer.begin(GPSBaud);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.macAddress(mac);

  int i;
  int j;
  j = 0;
  for (i = 5; i >= 0; i--) {
    sprintf(macAddr + (j * 2), "%02X", mac[i]);
    j++;
  }


  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }

  while (swSer.available() > 0) {
    rxChar = swSer.read();
    if (rxChar == '$') {
      rxData += rxChar;
      newScen = true;
    }
    else {
      if (newScen) {
        rxData += rxChar;
        if (rxData == "$GPGGA") {
          GPGGA = true;
        }
        if (rxChar == '\n') {
          if (GPGGA) {
            rxData += macAddr;
            Serial.println(rxData);
            client.publish("test/new/topic", rxData.c_str());
            GPGGA = false;
          }
          rxData = "";
        }
      }
    }
  }

  // Dispatch incoming characters




  client.loop();
}
