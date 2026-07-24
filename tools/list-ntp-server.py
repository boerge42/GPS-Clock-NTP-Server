#!/usr/bin/python3
# **********************************************************************
#
# Diverse NTP-Server anfragen, Rückgaben auflisten
# ================================================
#               Uwe Berger, 2026
#
# Absetzen NTP-Request an diverse NTP-Server via
# Python-Bibliothek ntplib. Aufbereitung der Ergebnisse in einer 
# Tabelle:
# 
#      |Server1|Server2|...
# -----+-------+-------+-----
# attr1|...    |...    |...
# -----+-------+-------+-----
# attr2|...    |...    |...
# -----+-------+-------+-----
# ...  |...    |...    |...
#
# ...andersrum (row/col) kann jeder :-) --> Grund: die Breite der 
# Tabelle wird (nur) durch die Anzahl der abgefragten Server bestimmt...
#
#
# =========
# Have fun!
#
# **********************************************************************

import ntplib
from datetime import datetime
from tabulate import tabulate
import sys
import os

# Liste anzufragender NTP-Server
NTP_SERVERS = []

# Ausgabe-Definitionen 
attr_defs = {
    "server":           {"header":"server",     "convert": False},
    "offset":           {"header":"offset\n((T2-T1)+(T3-T4))/2",     "convert": lambda v: f"{v * 1000:.6f} ms"},

    "precision":        {"header":"precision",  "convert": lambda v: f"{v} ({(2 ** v):.9f} s)"},
    
    "orig_time":        {"header":"orig_time (T1)\nTS send client",  "convert": lambda v: datetime.fromtimestamp(v).strftime("%Y-%m-%d %H:%M:%S.%f")},
    "recv_time":        {"header":"recv_time (T2)\nTS recv server",  "convert": lambda v: datetime.fromtimestamp(v).strftime("%Y-%m-%d %H:%M:%S.%f")},
    "tx_time":          {"header":"tx_time (T3)\nTS send server",    "convert": lambda v: datetime.fromtimestamp(v).strftime("%Y-%m-%d %H:%M:%S.%f")},
    "dest_time":        {"header":"dest_time (t4)\nTS recv client",  "convert": lambda v: datetime.fromtimestamp(v).strftime("%Y-%m-%d %H:%M:%S.%f")},
    "ref_time":         {"header":"ref_time\nlast sync. server",   "convert": lambda v: datetime.fromtimestamp(v).strftime("%Y-%m-%d %H:%M:%S.%f")},
    "leap":             {"header":"leap",       "convert": lambda v: f"{ntplib.leap_to_text(v)}"},
    "mode":             {"header":"mode",       "convert": lambda v: f"{ntplib.mode_to_text(v)}"},
    "poll":             {"header":"poll",       "convert": lambda v: f"{v}"},
    "version":          {"header":"version",    "convert": lambda v: f"{v}"},
    "delay":            {"header":"delay (RTT)\n(T4- T1)-(T3-T2)",      "convert": lambda v: f"{v * 1000:.6f} ms"},
    "root_delay":       {"header":"root_delay", "convert": lambda v: f"{v * 1000:.6f} ms"},
    "root_dispersion":  {"header":"root_dispersion", "convert": lambda v: f"{v * 1000:.6f} ms"},
    # Fehler in ntplib.ref_id_to_text()???
    # ~ "ref_id":           {"header":"ref_id",     "convert": lambda v: f"{ntplib.ref_id_to_text(v)}"},
    "ref_id":           {"header":"ref_id",                             
                         "convert": lambda v: (
                            (lambda b:
                                # ASCII? Dann Nullbytes entfernen
                                b.rstrip(b"\x00").decode("ascii")
                                if all(32 <= x <= 126 or x == 0 for x in b)
                                else ".".join(str(x) for x in b)
                            )(v.to_bytes(4, "big"))
                            if isinstance(v, int)
                            else (
                                v.rstrip(b"\x00").decode("ascii")
                                if isinstance(v, bytes) and all(32 <= x <= 126 or x == 0 for x in v)
                                else ".".join(str(x) for x in v) if isinstance(v, (bytes, bytearray))
                                else str(v)
                            )
                        )},
    "stratum":          {"header":"stratum",    "convert": lambda v: f"{ntplib.stratum_to_text(v)}"},
}




# **********************************************************************
# **********************************************************************
# **********************************************************************

# NTP-Server-Liste einlesen
if len(sys.argv) < 2:
    print(f"Bitte Dateiname angeben: {sys.argv[0]} <ntp-liste-txt>")
    sys.exit(1)

fn = sys.argv[1]

# Existenz prüfen (optional, open() würde es auch merken)
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

# Ergebnis-Array vordefinieren
results = []

# über Serverliste iterieren 
for server in NTP_SERVERS:
    try:
        # NTP-Server abfragen
        r = c.request(server, version=4, timeout=3)
        
        # 1.Zeile einer Spalte ist der Name des angefragten NTP-Servers
        col = {"server":server}
        
        # einzelne Attribute der Antwort auslesen/verarbeiten
        for attr in dir(r):
            
            # nur attr, die in attr_defs definiert sind
            if ((attr in attr_defs) == False):
                continue

            # der Wert des Attributes
            value = getattr(r, attr)
            
            # Wert lt. Definition konvertieren
            if (attr_defs[attr]["convert"]):
                value = attr_defs[attr]["convert"](value)
          
            # ...und zur Ergebisspalte hinzufügen
            col[attr]=value
        
        # Spalte zum Gesamtergebnis hinzufügen        
        results.append(col)
    except:
        pass

# Ausgabe Tabelle

# Attribute in der gewünschten Reihenfolge
attributes = list(attr_defs.keys())

# Tabellenkopf: Leere erste Spalte + Servernamen
headers = [""] + [col["server"] for col in results]

# Zeilen erzeugen (ohne "server")
table = []
for attr in attributes:
    if (attr == "server"):
        continue
    # ~ row = [attr]  # Attributname in Spalte 1
    row = [attr_defs[attr]["header"]]  # Attributname in Spalte 1
    for col in results:
        row.append(col.get(attr, ""))
    table.append(row)

print(tabulate(table, headers=headers, tablefmt="grid"))
