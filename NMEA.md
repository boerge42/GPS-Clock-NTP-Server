# Inhaltsbeschreibung relevanter NMEA-Datensätze

Beschreibung der NMEA-Sätze, die mein GPS-Modul (GT-U7) über die serielle Schnittstelle sendet. In der jeweiligen Tabellenspalte TinyGPS++ ist vermerkt, ob der Wert direkt mit dieser Arduino-Library ausgelesen werden kann oder ein entsprechendes Custom-Objekt definiert werden muss ([RTFM](https://github.com/mikalhart/TinyGPSPlus)).

## $GPRMC

`$GPRMC` (*Recommended Minimum Navigation Information*) liefert die wichtigsten Navigationsdaten eines GPS‑Empfängers: Zeit, Status, Position, Geschwindigkeit, Kurs und Datum.

**$GPRMC,171207.00,A,5225.57716,N,01230.46723,E,0.131,,210526,,,A*75**

$GPRMC,HHMMSS,A,BBBB.BBBB,b,LLLLL.LLLL,l,GG.G,RR.R,DDMMYY,M.M,m,F*PP

| Symbol                 | Bedeutung                                                                                                                                                                                          | TinyGPS++                                                    |
| ---------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| HHMMSS oder HHMMSS.SSS | Zeit (UTC)                                                                                                                                                                                         | gps.time.hour();<br>gps.time.minute();<br>gps.time.second(); |
| A                      | Status (A für *OK*, V bei Warnungen)                                                                                                                                                               | gps.location.isValid()                                       |
| BBBB.BBBB              | Breitengrad                                                                                                                                                                                        | gps.location.lat()                                           |
| b                      | Ausrichtung (N für *North*, nördlich; S für *South*, südlich)                                                                                                                                      |                                                              |
| LLLLL.LLLL             | Längengrad                                                                                                                                                                                         | gps.location.lng()                                           |
| l                      | Ausrichtung (E für *East*, östlich; W für *West*, westlich)                                                                                                                                        |                                                              |
| GG.G                   | Geschwindigkeit über Grund in Knoten                                                                                                                                                               | gps.speed.kmph()<br>gps.speed.knots()<br>gps.speed.mps()     |
| RR.R                   | Kurs über Grund in Grad bezogen auf geogr. Nord                                                                                                                                                    | gps.course.deg()                                             |
| DDMMYY                 | Datum (Tag Monat Jahr)                                                                                                                                                                             | gps.date.day()<br>gps.date.moth()<br>gps.date.year()         |
| M.M                    | magnetische Abweichung (Ortsmissweisung)                                                                                                                                                           | TinyGPSCustom                                                |
| m                      | Vorzeichen der Abweichung (*E* oder *W*)                                                                                                                                                           | TinyGPSCustom                                                |
| F                      | Navigationsmodus/Signalintegrität:<br>A = Autonomous mode, <br>D = Differential Mode, <br>E = Estimated (dead-reckoning) mode<br>M = Manual Input Mode<br>S = Simulated Mode<br>N = Data Not Valid | TinyGPSCustom                                                |
| PP                     | hexadezimale Darstellung der Prüfsumme  <br>(Die Prüfsumme ergibt sich durch eine XOR-Verknüpfung aller Daten-Bytes zwischen (jeweils exklusive) dem Dollar-Zeichen '$' und dem Stern '*'.)        |                                                              |

## $GPVTG

`$GPVTG` liefert die Bewegungsrichtung (Track) und die Geschwindigkeit eines GPS‑Empfängers. Er ergänzt `$GPRMC`, indem er dieselben Bewegungsdaten *ohne* Positions‑ oder Zeitangaben ausgibt.

**$GPVTG,,T,,M,0.131,N,0.243,K,A*25**

$GPVTG,TTT.T,T,MMM.M,M,GG.G,N,KK.K,K,F*PP

| Symbol | Bedeutung                                                                                                                                                    | TinyGPS++                       |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------- |
| TTT.T  | Kurs über Grund relativ zum **geographischen Norden** (*True Track*)                                                                                         | TinyGPSCustom<br>(siehe $GPRMC) |
| T      | Kennung für „True“ (wahrer Kurs)                                                                                                                             | TinyGPSCustom                   |
| MMM.M  | Kurs über Grund relativ zum **magnetischen Norden** (*Magnetic Track*)                                                                                       | TinyGPSCustom                   |
| M      | Kennung für „Magnetic“                                                                                                                                       | TinyGPSCustom                   |
| GG.G   | Geschwindigkeit über Grund in **Knoten**                                                                                                                     | TinyGPSCustom<br>(siehe $GPRMC) |
| N      | Kennung für Knoten (knots)                                                                                                                                   | TinyGPSCustom                   |
| KK.K   | Geschwindigkeit über Grund in **km/h**                                                                                                                       | TinyGPSCustom<br>(siehe $GPRMC) |
| K      | Kennung für km/h                                                                                                                                             | TinyGPSCustom                   |
| F      | Signalintegrität / Modus:<br>A = Autonomous<br>D = Differential<br>E = Estimated (Dead Reckoning)<br>M = Manual Input<br>S = Simulated<br>N = Data Not Valid | TinyGPSCustom<br>(siehe $GPRMC) |
| PP     | Prüfsumme (hexadezimal), XOR aller Bytes zwischen `` `$` `` und `` `*` ``                                                                                    |                                 |

## $GPGGA

`$GPGGA` liefert die grundlegenden GPS‑Positionsdaten: genaue UTC‑Zeit, Breiten‑ und Längengrad, Fix‑Qualität, Anzahl der Satelliten, Genauigkeitsfaktor (HDOP), Höhe über Meer sowie optionale DGPS‑Informationen.`

**$GPGGA,171207.00,5225.57716,N,01230.46723,E,1,08,0.95,33.3,M,43.3,M,,*6B**

$GPGGA,HHMMSS,BBBB.BBBB,b,LLLLL.LLLL,l,Q,NN,HDOP,ALT,a,GEOID,a,AGE,REF*PP

| Symbol     | Bedeutung                                                                                                                                                  | TinyGPS++                                                    |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| HHMMSS     | Zeit (UTC)                                                                                                                                                 | gps.time.hour();<br>gps.time.minute();<br>gps.time.second(); |
| BBBB.BBBB  | Breitengrad                                                                                                                                                | gps.location.lat()                                           |
| b          | Ausrichtung (N = North, S = South)                                                                                                                         |                                                              |
| LLLLL.LLLL | Längengrad                                                                                                                                                 | gps.location.lng()                                           |
| l          | Ausrichtung (E = East, W = West)                                                                                                                           |                                                              |
| Q          | Fix‑Qualität:<br>0 = kein Fix<br>1 = GPS Fix<br>2 = DGPS Fix<br>4 = RTK Fixed<br>5 = RTK Float<br>6 = Dead Reckoning<br>7 = Manual Input<br>8 = Simulation | TinyGPSCustom                                                |
| NN         | Anzahl der verwendeten Satelliten                                                                                                                          | gps.satellites.value()                                       |
| HDOP       | Horizontal Dilution of Precision (Genauigkeitsfaktor)                                                                                                      | gps.hdop.value()                                             |
| ALT        | Höhe über Meeresspiegel                                                                                                                                    | gps.altitude.meters()                                        |
| a          | Einheit der Höhe (meist „M“ für Meter)                                                                                                                     |                                                              |
| GEOID      | Geoid‑Separation (Abstand zwischen WGS‑84‑Ellipsoid und Geoid)                                                                                             | TinyGPSCustom                                                |
| a          | Einheit der Geoid‑Separation (meist „M“)                                                                                                                   |                                                              |
| AGE        | Alter der DGPS‑Daten in Sekunden (optional)                                                                                                                | TinyGPSCustom                                                |
| REF        | Referenzstations‑ID (optional)                                                                                                                             | TinyGPSCustom                                                |
| PP         | Prüfsumme (hexadezimal), XOR aller Bytes zwischen `` `$` `` und `` `*` ``                                                                                  |                                                              |

## $GPGSA

`$GPGSA` beschreibt den aktuellen GNSS‑Modus, die Art des Fixes (2D/3D), die verwendeten Satelliten sowie die Genauigkeitswerte PDOP, HDOP und VDOP.

**$GPGSA,A,3,04,03,07,11,06,19,31,09,,,,,1.51,0.95,1.18*06**

$GPGSA,M,F,SS,SS,SS,SS,SS,SS,SS,SS,SS,SS,PDOP,HDOP,VDOP*PP

| Symbol | Bedeutung                                                                 | TinyGPS++                        |
| ------ | ------------------------------------------------------------------------- | -------------------------------- |
| M      | Moduswahl: A = automatisch, M = manuell                                   | TinyGPSCustom                    |
| F      | Fix‑Typ: 1 = kein Fix, 2 = 2D‑Fix, 3 = 3D‑Fix                             | TinyGPSCustom                    |
| SS     | PRN‑Nummern der **verwendeten Satelliten** (bis zu 12 Felder)             | TinyGPSCustom                    |
| PDOP   | Position Dilution of Precision (Gesamtgenauigkeit)                        | TinyGPSCustom                    |
| HDOP   | Horizontal Dilution of Precision                                          | TinyGPSCustom<br> (siehe $GPGGA) |
| VDOP   | Vertical Dilution of Precision                                            | TinyGPSCustom                    |
| PP     | Prüfsumme (hexadezimal), XOR aller Bytes zwischen `` `$` `` und `` `*` `` |                                  |

## $GPGSV

`$GPGSV` liefert Informationen über alle sichtbaren Satelliten: Anzahl der Datensätze, laufende Satznummer, Gesamtzahl der Satelliten sowie für jeden Satelliten Azimut, Elevation und Signalstärke.

**$GPGSV,3,1,11,03,26,112,31,,58,251,28,07,28,176,27*71**

**$GPGSV,3,2,11,09,79,240,20,11,35,306,24,16,07,083,20,19,07,238,24*77**

**$GPGSV,3,3,11,21,07,292,,26,14,054,,31,11,037,22*42**

$GPGSV,TT,NN,SS,(PRN,EL,AZ,SNR)…*PP

| Symbol | Bedeutung                                                                 | TinyGPS++     |
| ------ | ------------------------------------------------------------------------- | ------------- |
| TT     | Anzahl der GSV‑Sätze, die gesendet werden (1–3)                           | TinyGPSCustom |
| NN     | Nummer des aktuellen Satzes (1…TT)                                        | TinyGPSCustom |
| SS     | Gesamtzahl der sichtbaren Satelliten                                      | TinyGPSCustom |
| PRN    | Satelliten‑ID (PRN‑Nummer)                                                | TinyGPSCustom |
| EL     | Elevation des Satelliten in Grad (0–90°)                                  | TinyGPSCustom |
| AZ     | Azimut des Satelliten in Grad (0–359°)                                    | TinyGPSCustom |
| SNR    | Signal‑Rausch‑Verhältnis in dB (0–99), 0 = kein Signal                    | TinyGPSCustom |
| PP     | Prüfsumme (hexadezimal), XOR aller Bytes zwischen `` `$` `` und `` `*` `` |               |

## $GPGLL

`$GPGLL` liefert die reine Positionsangabe: Breitengrad, Längengrad, UTC‑Zeit, Status und optional den GNSS‑Modus.

**$GPGLL,5225.57716,N,01230.46723,E,171207.00,A,A*6D**

$GPGLL,BBBB.BBBB,b,LLLLL.LLLL,l,HHMMSS,F*PP

| Symbol     | Bedeutung                                                                                                                                                                           | TinyGPS++                                                    |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| BBBB.BBBB  | Breitengrad                                                                                                                                                                         | gps.location.lat()                                           |
| b          | Ausrichtung (N = North, S = South)                                                                                                                                                  |                                                              |
| LLLLL.LLLL | Längengrad                                                                                                                                                                          | gps.location.lng()                                           |
| l          | Ausrichtung (E = East, W = West)                                                                                                                                                    |                                                              |
| HHMMSS     | Zeit (UTC)                                                                                                                                                                          | gps.time.hour();<br>gps.time.minute();<br>gps.time.second(); |
| F          | Status / Modus:<br>A = Daten gültig<br>V = Daten ungültig<br>Erweiterte GNSS‑Modi (je nach Empfänger):<br>A = Autonomous<br>D = Differential<br>E = Estimated<br>N = Data not valid | über gps.location.isValid()                                  |
| PP         | Prüfsumme (hexadezimal), XOR aller Bytes zwischen `` `$` `` und `` `*` ``                                                                                                           |                                                              |
