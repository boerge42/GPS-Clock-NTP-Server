# Aufbau/Inhalt NTP-Paket

Kurze Variante (48 Byte) <u>ohne die optionalen Felder</u>:

- Extension Field 1/ 2

- Key/Algorithm Identifier

- Message Digest

| Byte-Offset | Länge | Bedeutung                                                                          |
| ----------- | ----- | ---------------------------------------------------------------------------------- |
| 0           | 1     | LI (Leap Indicator; Schaltsekunde) (Bit 0)<br>Version (Bit 1-3)<br> Mode (Bit 4-7) |
| 1           | 1     | Stratum                                                                            |
| 2           | 1     | Poll<br>Logarithmischer Poll‑Intervallwert (z. B. 6 = 64 s)                        |
| 3           | 1     | Precision<br>Genauigkeit der lokalen Uhr in 2‑er‑Potenz (z. B. −20 ≈ 1 µs)         |
| 4–7         | 4     | Root Delay<br>Round‑Trip‑Zeit zur Primärquelle (16.16‑Fixed‑Point)                 |
| 8–11        | 4     | Root Dispersion<br>Maximale Abweichung zur UTC‑Zeit (16.16‑Fixed‑Point)            |
| 12–15       | 4     | Reference ID<br>Kennung der Zeitquelle (z. B. „GPS“, „PPS“, IPv4‑Adresse).         |
| 16–23       | 8     | Reference Timestamp<br>Zeitpunkt der letzten erfolgreichen Synchronisation.        |
| 24–31       | 8     | Origin Timestamp<br>Kopie des Client‑Transmit‑Timestamps (T1).                     |
| 32–39       | 8     | Receive Timestamp<br>Zeitpunkt, an dem der Server das Paket empfing (T2).          |
| 40–47       | 8     | Transmit Timestamp<br>Zeitpunkt, an dem der Server das Paket sendet (T3).          |

# Ablauf eines NTP‑Requests laut RFC 5905

Ein Client sendet:

- **Origin Timestamp = 0**

- **Transmit Timestamp = T1**

Der Server muss senden:

- **Origin Timestamp = T1** (vom Client kopieren)

- **Receive Timestamp = T2** (Zeitpunkt des Empfangs)

- **Transmit Timestamp = T3** (Zeitpunkt des Sendens)

- **Reference Timestamp = Zeitpunkt der letzten Synchronisation**
