import paho.mqtt.client as mqtt			
import pynmea2
import datetime
import mysql.connector
import socketio
try:
	import thread
except ImportError:
	import _thread as thread

from mysql.connector import errorcode
from geopy import distance

SIO_SERVER_IP	 = "http://localhost:5001"
MQTT_SERVER_IP	 = "localhost"
MQTT_SERVER_PORT = 1883


class Player:
	def __init__(self):
		self.currentLat 	= 0
		self.currentLong 	= 0
		self.lastLat		= 0
		self.lastLong		= 0
		self.time			= 0
		self.noOfSats		= 0
		self.distance		= 0
		self.totalDistance	= 0
		self.currentSpeed	= 0
		self.averageSpeed	= 0
		self.topSpeed		= 0
		self.slowestSpeed	= 0
		self.timeOnFloor	= 0
		self.timeWalking 	= 0
		self.timeSprinting	= 0
		self.totalSprints	= 0
		self.timeJogging	= 0
		self.tackleCount	= 0
		self.ballCarries	= 0
		self.missedTackles	= 0
		self.tryCount		= 0
		self.gForce			= 0
		
		
		self.timeIn22		= 0
		self.timeInOpp22	= 0
		self.timeInOwn5		= 0
		self.timeInOpp5		= 0
		self.timeInOwnHalf	= 0
		self.timeInOppHalf	= 0
		
		self.isSprinting	= False
		self.isInOwn22		= False
		self.isInOpp22		= False
		self.isInOwnHalf	= False
		self.isInOppHalf	= False
		self.isInOwn5		= False
		self.isInOpp5		= False
				
		self.points			= []
		self.logCount		= 0
	
	# this function is called from updatePosition
	def updateDistance(self):
		prevPos 			= (self.lastLat, self.lastLong)
		currentPos 			= (self.currentLat, self.currentLong)
		if (self.lastLat == 0 and self.lastLong == 0):
			self.totalDistance = 0
		else:
			self.distance		 = distance.distance(prevPos, currentPos).meters
			self.totalDistance 	+= self.distance
		return
	
	# ensure that updateData is called before this function 
	def getSpeed(self):
		if (self.distance > self.topSpeed):
			self.topSpeed = self.distance
		if (self.distance < self.slowestSpeed):
			self.slowestSpeed = self.distance
		return self.distance
	
	# this function is called from updatePosition
	def updateAvgSpeed(self):
		elapsedTime = datetime.timedelta()
		timeDelta	= datetime.timedelta()
		avSpeed		= 0
		totDist		= 0
		if (len(self.points) == 5):
			for cur, next in zip(self.points, self.points[1:]):
				next_t_delta 	= datetime.timedelta(hours=next[0].hour, minutes=next[0].minute, seconds=next[0].second)
				cur_t_delta 	= datetime.timedelta(hours=cur[0].hour, minutes=cur[0].minute, seconds=cur[0].second)
				timeDelta       = next_t_delta-cur_t_delta
				print(str(timeDelta.total_seconds()))
				if (timeDelta.total_seconds() > 3600 or timeDelta.total_seconds() < 0):
					timeDelta = datetime.timedelta()
				elapsedTime	   += timeDelta
				totDist 	   += distance.distance(cur[1], next[1]).meters
			print("totDistance: " + str(totDist))
			print("elapsedTime: " + str(elapsedTime))
			self.averageSpeed = totDist/int(elapsedTime.total_seconds())
		return 
	
	
	def getDistance(self):
		return self.totalDistance
	
	def getTimeInOwnHalf(self):
		return
	def getTimeInOwn22(slef):
		return
	def getTimeInRedZone(self):
		return
	def getTimeInOppHalf(self):
		return
	def getTimeInOpp22(self):
		return
	def getTimeInOppRedZone(self):
		return
	
	def updatePosition(self, gpsData):
		self.lastLat		= self.currentLat
		self.lastLong		= self.currentLong
		self.currentLat 	= gpsData.latitude
		self.currentLong 	= gpsData.longitude
		self.time 			= gpsData.timestamp
		self.noOfSats		= gpsData.num_sats
		self.logCount		+= 1
		
		if (len(self.points) == 5):
			del self.points[0]
			self.points.append([self.time,(self.currentLat, self.currentLong)])
		else:
			self.points.append([self.time,(self.currentLat, self.currentLong)])
		
		self.updateDistance()
		self.updateAvgSpeed()
		
	def distanceToOwnTryLine(self):
		return
	def distanceToOppTryLine(self):
		return
	def getNetDistance(self):
		return
	def getLastFivePositions(self):
		return
	def getLast10Positions(self):
		return
		
	def getData(self):
		stats = {
			"totalDistance" : round(self.getDistance(),2),
			"noOfSats"		: self.noOfSats,
			"time"			: str(self.time),
			"receivedLogs"	: self.logCount,
			"currentSpeed"	: round(self.getSpeed(),2), # now you can get the up to date speed stats
			"topSpeed"		: self.topSpeed,
			"slowestSpeed"	: self.slowestSpeed,
			"lastFivePoints": str(self.points),
			"avgSpeed"		: round(self.averageSpeed,2)
		}
		return stats
		
# create a player
p1 = Player()

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
	print("Connected with result code "+str(rc))

	# Subscribing in on_connect() means that if we lose the connection and
	# reconnect then subscriptions will be renewed.
	client.subscribe([("test/topic",0),("gps/register",0),("gps/data",0),("gps/switch/off",0),("gps/switch/on",0)])

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	#print(msg.topic+" "+str(msg.payload))
	if msg.topic == "gps/data":
		macAddress = msg.payload[:12]
		gps = msg.payload[13:]
		try:
			gps = pynmea2.parse(gps)
		except:
			print "some sort of error with gps scentence"
		else:
			# update player data
			p1.updatePosition(gps)
			# send the data to the webserver
			sio.emit('message',p1.getData())
	elif msg.topic == "gps/switch/off":
		client.disconnect()

# create and open the socketio client connection
sio = socketio.Client()
sio.connect(SIO_SERVER_IP)
		
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_SERVER_IP, MQTT_SERVER_PORT, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()
ws.disconnect()
print "Disconnecting from MQTT broker"