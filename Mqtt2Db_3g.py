import paho.mqtt.client as mqtt			
import pynmea2
import datetime
import time
import mysql.connector
from mysql.connector import errorcode

#placeholder for db connection & cursor object
cnx = ""
cursor = ""
client = ""

#check if the loggers are still alive...
logger1IsAlive = False
logger2IsAlive = False

#associate the logger with a mac address
dictGpsLoggers = {}
loggerCount = 0
# Register a new GPS logger in the database
def registerLogger(macAddress):
	add_logger = ("INSERT INTO gpsLogger(macAddress) VALUES (%s)")
	data_logger = (macAddress,)
	print macAddress
	cursor.execute(add_logger, data_logger)
	cnx.commit()

# Insert GPS data into database
def logGpsData(gpsData, macAddr):
	print "Number of satellites: " + gpsData.num_sats
	try:
		noOfSats = int(gpsData.num_sats)
	except:
		print "Bad nmea scentence"
	else:
		if int(gpsData.num_sats) > 3:
			addGps = ("INSERT INTO gpsData(protocolHeader, lat, ns_indicator, lon, ew_indicator, fs, noSv, hdop, uMsl, altRef, uSep, msl, macAddress, gpsUtc) VALUES(%(protocolHeader)s, %(lat)s, %(ns_indicator)s, %(lon)s, %(ew_indicator)s, %(fs)s, %(noSv)s, %(hdop)s, %(uMsl)s, %(altRef)s, %(uSep)s, %(msl)s, %(macAddress)s, %(gpsUtc)s)")

			data_gps = {
				'protocolHeader': "$GPGGA", 
				'gpsUtc': gpsData.timestamp, 
				'lat': gpsData.lat, 
				'ns_indicator': gpsData.lat_dir, 
				'lon': gpsData.lon, 
				'ew_indicator': gpsData.lon_dir, 
				'fs': gpsData.gps_qual, 
				'noSv': gpsData.num_sats, 
				'hdop': gpsData.horizontal_dil,
				'uMsl': gpsData.altitude_units,
				'altRef': gpsData.geo_sep, 
				'uSep': gpsData.geo_sep_units, 
				'msl': gpsData.altitude,
				'macAddress': macAddr,
				}
			cursor.execute(addGps,data_gps)
			cnx.commit()
			print "Row added at: " + datetime.datetime.now().strftime("%H:%M:%S")
			client.publish("st01/status", "Row added at " + datetime.datetime.now().strftime("%H:%M:%S"))
		else:
			print "No satellite fix @ " + datetime.datetime.now().strftime("%H:%M:%S")
			client.publish("st01/status", "No satellite fix @ " + datetime.datetime.now().strftime("%H:%M:%S"))

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
	print("Connected from 3G script with result code "+str(rc))
	# Subscribing in on_connect() means that if we lose the connection and
	# reconnect then subscriptions will be renewed.
	client.subscribe([("test/topic",0),("gps/register",0),("gps/data",0),("gps/switch/off",0),("gps/switch/on",0)])

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	if msg.topic == "gps/data":
		client3g.publish("gps/data", msg.payload, 0, False)
	elif msg.topic == "gps/switch/off":
		client.disconnect()

client = mqtt.Client()
client.reinitialise("3gUser", False, None)
client3g = mqtt.Client()
client3g.reinitialise("ST01", False, None)
client3g.username_pw_set("pi", "j9a5cxec")
client.on_connect = on_connect
client.on_message = on_message
client3g.connect("mirzahome.duckdns.org", 1883, 60)
client.connect("localhost", 1883, 60)
client.publish("st01/status", "connected to database", 0, False)
client.publish("st01/status", "Assigned all callback functions", 0, False)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()
#cnx.close()
print "Disconnecting from MQTT broker"
