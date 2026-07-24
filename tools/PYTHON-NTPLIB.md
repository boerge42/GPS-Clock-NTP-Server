# Tools zum Vergleichen von NTP-Server untereinander mit Python-Lib `ntplib`

`ntplib` gibt im Kern ein Objekt (`NTPStats`) zurück, welches direkt aus der NTP-Antwort (RFC 5905) berechnet wird. Diese Werte lassen sich in vier Gruppen einteilen:

## 1. Zeitwerte (Messgrundlage)

Diese stammen direkt aus den NTP-Zeitstempeln:

#### `orig_time` (T1)

* Zeitpunkt, an dem der Client die Anfrage gesendet hat.
* Lokal gemessen.

#### `recv_time` (T2)

* Zeitpunkt, an dem der Server die Anfrage empfangen hat.
* Vom Server gesetzt.

#### `tx_time` (T3)

* Zeitpunkt, an dem der Server die Antwort gesendet hat.
* Vom Server gesetzt.

#### `dest_time` (T4)

* Zeitpunkt, an dem der Client die Antwort empfangen hat.
* Lokal gemessen.

**Diese vier Werte sind die Basis für alle Berechnungen.**



## 2. Hauptmetriken (die wichtigen Ergebnisse)

#### `offset`

 Zeitabweichung zwischen Client und Server (berechnet aus T1, T2, T3, T4). 

Es ist der Wert um den die lokale Uhr (Client) korrigiert wird (via `adjtime(offset)` bei < +/-128ms; via `settimeofday(...)` bei >= +/-128ms)

$offset = \frac{(T2 - T1) + (T3 - T4)}{2}$

- Einheit: Sekunden
* Das ist **kein Qualitätsmaß des Servers**, sondern der Unterschied zwischen zwei Uhren.

#### `delay`

Zeit, die ein Paket hin und zurück im Netzwerk benötigt (Round Trip Delay (RTT)). 

Wird wie folgt berechnet:

$delay = (T4 - T1) - (T3 - T2)$

* Einheit: Sekunden

* Enthält also:
  
  * Netzwerk-Latenz
  * Verarbeitung im Server
  * Queueing
- Wichtig für Qualität:
  
  - niedriger = besser
  
  - stabil = gut

## 3. Server-Qualitätsangaben (vom Server geliefert)

#### `stratum`

Position in der Zeit-Hierarchie

| Wert | Bedeutung                    |
| ---- | ---------------------------- |
| 0    | Referenzuhr (Atomuhr, GPS)   |
| 1    | direkt angeschlossene Server |
| 2–15 | weitere Ebenen               |
| 16   | ungültig                     |

Niedriger ist besser, aber:

* kein Qualitätsgarant
* ein guter Stratum-2 kann besser sein als ein schlechter Stratum-1

#### `precision`

Genauigkeit der Server-Uhr, angegeben als Potenz von 2:

$precision = 2^{x} \text{ Sekunden}$

Beispiel:

* `-20` → ca. 1 µs
* `-30` → ca. 1 ns

Beschreibt nicht Netzqualität, sondern Uhrhardware

#### `leap`

Leap-Second Status:

| Wert | Bedeutung                     |
| ---- | ----------------------------- |
| 0    | normal                        |
| 1    | +1 Sekunde am Ende des Monats |
| 2    | -1 Sekunde                    |
| 3    | ungültig                      |

#### `ref_id`

Referenzquelle des Servers

Beispiele:

* `"GPS"` → GPS-Zeit
* `"PPS"` → PPS-Signal
* `"ATOM"` → Atomuhr
* ...
* IP-Adresse → upstream NTP-Server

Wichtig für Vertrauenskette!

#### `ref_time`

Zeitpunkt der letzten Synchronisation des Servers

* zeigt, wie „frisch“ der Server ist
* ältere Werte → potenziell schlechtere Genauigkeit

## 4. Fehler- und Stabilitätsmetriken

#### `root_delay`

Gesamte Verzögerung bis zur Referenzuhr:

* Summe der Netzwerk-Delays entlang der NTP-Kette
* Einheit: Sekunden
* kleiner = besser

#### `root_dispersion`

Geschätzter maximaler Fehler zur Referenzzeit:

- wichtigste Qualitätsgröße im NTP-Kontext

- Einheit: Sekunden

Interpretation:

* niedriger Wert = hohe Genauigkeit

* wächst mit:
  
  * Zeit seit letzter Synchronisation
  * Netzwerkunsicherheit

## Weitere interessante Werte:

### Jitter (nicht direkt vom Server :-(...)

#### `jitter`

* Streuung von Offset-Messungen über mehrere Abfragen
* nicht in einer einzelnen Antwort enthalten
* muss selbst berechnet werden

### Interne / Hilfswerte

#### `version`

* verwendete NTP-Version (meist 4)

#### `mode`

* Betriebsmodus (Client/Server/Broadcast)

---

## Zusammenfassung: Was ist wirklich wichtig?

Für technische Bewertung:

- wichtigste Qualitätsmetriken
  
  - `root_dispersion`
  
  - `delay`
  
  - `jitter` (selbst berechnen!)
  
  - Stabilität über Zeit

- sekundär
  
  - `stratum`
  
  - `root_delay`
  
  - `ref_time`

- nur diagnostisch
  
  - `offset`
  
  - `leap`
  
  - `precision`
  
  - `ref_id`
