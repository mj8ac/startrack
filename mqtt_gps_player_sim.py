import paho.mqtt.client as mqtt	
import time

MQTT_SERVER_IP	 	= "localhost"
MQTT_SERVER_PORT 	= 1883
NMEA_DATA_FILE_PATH = r"E:\STAR_TRACK\Logging\2017-12-16_Bournemouth_Home\0B574C7FCF5C_B_SANDERSON_sim.nmea"
MSG_FREQUENCY		= 1

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
	print("Connected with result code "+str(rc))

def transmitMsg(msg):
		rc = client.publish("gps/data", msg, qos=0, retain=True)
		print("Message sent, rc=" + str(rc))
	
client = mqtt.Client()
client.on_connect = on_connect

client.connect(MQTT_SERVER_IP, MQTT_SERVER_PORT, 60)

# Read the file and spit it out over mqtt!
f = open(NMEA_DATA_FILE_PATH, "r")
for x in f:
  x = x.rstrip()
  transmitMsg(x)
  time.sleep(MSG_FREQUENCY)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()