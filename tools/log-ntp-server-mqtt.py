#!/usr/bin/python3
# **********************************************************************
#
# Diverse NTP-Server anfragen; Antworten --> MQTT
# ===============================================
#                Uwe Berger, 2026
#
# * Absetzen NTP-Request an diverse NTP-Server via Python-Bibliothek 
#   ntplib. 
# * Versenden relevanter Daten (offset, delay pro Server) via MQTT...,
#   um sie vielleicht, mit geeigneten Mitteln (z.B. Telegraf) in eine
#   Datenbank, über die Zeit, abzulegen (z.B. in eine InfluxDB) und zu
#   analysieren/visualisieren (z.B. Grafana)
#
# Sinnvollerweise ruft man dieses Python-Script regelmäßig/zyklisch auf;
# z.B. Cron ist dazu ganz geeignet ;-)
#
# =========
# Have fun!
#
# **********************************************************************

import ntplib
import sys
import os
import paho.mqtt.client as mqtt

# MQTT-Broker
MQTT_HOST = "nanotuxedo"
MQTT_PORT = 1883
MQTT_USER = ""
MQTT_PWD  = ""
MQTT_TOPIC = "esp32-ntp-server/"

# Liste anzufragender NTP-Server
NTP_SERVERS = []

# **********************************************************************
# **********************************************************************
# **********************************************************************

# NTP-Server-Liste einlesen
if len(sys.argv) < 2:
    print(f"Bitte Dateiname angeben: {sys.argv[0]} <ntp-liste-txt>")
    sys.exit(1)

fn = sys.argv[1]

# ...plus ein wenig Fehlerbehandlung
if not os.path.exists(fn):
    print(f"Fehler: Datei '{fn}' existiert nicht.")
    sys.exit(1)

try:
    with open(fn, "r", encoding="utf-8") as f:
        for line in f:
            NTP_SERVERS.append(line.rstrip("\n"))

except FileNotFoundError:
    print(f"Fehler: Datei '{fn}' wurde nicht gefunden.")
    sys.exit(1)

except PermissionError:
    print(f"Fehler: Keine Berechtigung zum Lesen von '{fn}'.")
    sys.exit(1)

except IsADirectoryError:
    print(f"Fehler: '{fn}' ist ein Verzeichnis, keine Datei.")
    sys.exit(1)
    
except UnicodeDecodeError:
    print(f"Fehler: '{fn}' ist keine UTF‑8‑Textdatei.")
    sys.exit(1)

except OSError as e:
    print(f"Allgemeiner OS-Fehler beim Zugriff auf '{fn}': {e}")
    sys.exit(1)

# NTP-Client erzeugen
c = ntplib.NTPClient()

# MQTT senden
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PWD)
client.connect(MQTT_HOST, MQTT_PORT, 60)

# über Serverliste iterieren 
for server in NTP_SERVERS:
    try:
        # NTP-Server abfragen
        r = c.request(server, version=4, timeout=3)
        
        mqtt_payload = f"esp32_ntp_server,server={server} offset={getattr(r, 'offset'):.6f},delay={getattr(r, 'delay'):.6f}"
        # ~ print(mqtt_payload)
        client.publish(MQTT_TOPIC, mqtt_payload)        
    except:
        print("!!!")

client.disconnect()
