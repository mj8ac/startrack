
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// GPS setup
static const int      RXPin   = 5,    TXPin = 4;
static const uint32_t GPSBaud = 9600;

// Connect to the WiFi
//const char* ssid = "VM4010643";
//const char* ssid = "TNCAP39BADF";
const char* ssid = "Jonathan's iPhone";
//const char* password = "Xj3jzhqpjLsr";
//const char* password = "36F8BA6FEE";
const char* password = "J9a5cxec";
const char* mqtt_server = "mirzahome.duckdns.org";
//const char* mqtt_server = "192.168.1.140";
byte    mac[6];
char    macAddr[12];
String  rxData;
String  jsonStr;
String  gpsJson;
String  macJson;
char    rxChar;
bool    newScen = false;
bool    GPGGA   = false;

// The wifi object
WiFiClient espClient;
// The MQTT initialisation
PubSubClient client(espClient);
// The TinyGPS++ object
TinyGPSPlus gps;
// The serial connection to the GPS device
//SoftwareSerial swSer(RXPin, TXPin);

void setup()
{
  //Serial.begin(115200);
  Serial.begin(GPSBaud);
  //swSer.begin(GPSBaud);
  setup_wifi();
  client.setServer(mqtt_server, 1884);
  //client.setCallback(callback);
  reconnect();
}

void setup_wifi() {
  delay(10);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }

  //Serial.println("");
  //Serial.println("WiFi connected");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  WiFi.macAddress(mac);

  int i;
  int j;
  j = 0;
  for (i = 5; i >= 0; i--) {
    sprintf(macAddr + (j * 2), "%02X", mac[i]);
    j++;
    //macAddr += mac[i];
  }
}

void resetWifi(){
  //Serial.println("Trying to reconnect to WiFi...");
  //WiFi.diconnect();
  WiFi.begin(ssid, password);
  //Serial.println("");
  //Serial.println("WiFi connected");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  return;  
}

void callback(char* topic, byte* payload, unsigned int length) {
  //Serial.print("Message arrived [");
  //Serial.print(topic);
  //Serial.print("] ");
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    //Serial.print(receivedChar);
  }
  //Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    //Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266 Client")) {
      //Serial.println("connected");
      // ... and subscribe to topic
      client.subscribe("test/topic");
    } else {
      //Serial.print("failed, rc=");
      //Serial.print(client.state());
      //Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void loop()
{
  client.loop();
  
  while (Serial.available() > 0) {
      if (WiFi.status() != WL_CONNECTED){
      setup_wifi();
  }
  
  if (!client.connected()) {
      reconnect();
    }
    
    rxChar = Serial.read();
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
            //Serial.println(jsonStr);
            client.publish("gps/data", jsonStr.c_str());
            GPGGA = false;
          }
          rxData = "";
          jsonStr = "";
        }
      }
    }
  }
  // Dispatch incoming characters

}
