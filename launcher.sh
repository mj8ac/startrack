#!/bin/sh

sudo /usr/bin/modem3g/sakis3g connect APN="internet"
sudo route add default dev ppp0
sudo iptables -t nat -A POSTROUTING -o ppp0 -j MASQUERADE
python /home/pi/startrack/Mqtt2Db.py &
python /home/pi/startrack/Mqtt2Db_3g.py &
