import paho.mqtt.client as mqtt
import pynmea2
import mysql.connector
from mysql.connector import errorcode

#placeholder for db connection & cursor object
cnx = ""
cursor = ""
client = ""
# Register a new GPS logger in the database
def registerLogger(macAddress):
        add_logger = ("INSERT INTO gpsLogger(macAddress) VALUES (%s)")
        data_logger = (macAddress)
        cursor.execute(add_logger)
        cnx.commit()

# Insert GPS data into database
def logGpsData(gpsData):
        #msg = pynmea2.parse(gpsData)
        
        addGps = ("INSERT INTO gpsData(loggerID, protocolHeader, utc, lat, ns_indicator, lon, ew_indicator, fs, noSv, hdop, uMsl, altRef, uSep, diffAge, diffStation, cs, msl) VALUES(%(loggerID)s, %(protocolHeader)s, %(utc)s, %(lat)s, %(ns_indicator)s, %(lon)s, %(ew_indicator)s, %(fs)s, %(noSv)s, %(hdop)s, %(uMsl)s, %(altRef)s, %(uSep)s, %(diffAge)s, %(diffStation)s, %(cs)s), %(msl)s")
    
        data_gps = {
                'loggerID': 1, 
                'protocolHeader': "$GPGGA", 
                'utc': gpsData.timestamp, 
                'lat': gpsData.lat, 
                'ns_indicator': gpsData.lat_dir, 
                'lon': gpsData.lon, 
                'ew_indicator': gpsData.lon_dir, 
                'fs': gpsData.gps_qual, 
                'noSv': gpsData.num_sats, 
                'hdop': gpsData.horizontal_dil,
                'msl': gpsData.altitude,
                'uMsl': gpsData.altitude_units, 
                'altRef': gpsData.geo_sep, 
                'uSep': gpsData.geo_sep_units, 
                'diffAge': gpsData.age_gps_data, 
                'diffStation': gpsData.ref_station_id, 
                'cs': ""
        }
        cursor.execute(addGps,data_gps)
        cnx.commit()

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe([("test/topic",0),("gps/register",0),("gps/data",0),("gps/switch/off",0),("gps/switch/on",0)])

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    #print(msg.topic+" "+str(msg.payload))
    if msg.topic == "test/topic":
            print "This is a test topic"
    elif msg.topic == "gps/register":
            print "This is a gps register topic"
    elif msg.topic == "gps/data":
            msg = pynmea2.parse(msg.payload)
            logGpsData(msg)
            print msg
    elif msg.topic == "gps/switch/off":
            client.disconnect()

def connectToDb():
        global cnx 
        global cursor
        try:
                cnx = mysql.connector.connect(user='pi', password='J9a5cxec', host='192.168.0.15', database='PLAYER_GPS_DATA')
                cursor = cnx.cursor()
        except mysql.connector.Error as err:
          if err.errno == errorcode.ER_ACCESS_DENIED_ERROR:
                print("Something is wrong with your user name or password")
          elif err.errno == errorcode.ER_BAD_DB_ERROR:
                print("Database does not exist")
          else:
                print(err)

       
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message


connectToDb()
client.connect("192.168.0.16", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()
#cnx.close()
print "Disconnecting from MQTT broker"
