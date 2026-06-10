# ToDo

## my_timegm durch mktime() ersetzen

- [x] Erledigt?

```cpp
void task_set_first_datetime(void *pvParameters) {
    for (;;) {
        if (gps.date.isUpdated() && gps.time.isUpdated() && gps.date.isValid() && gps.time.isValid()) {
                struct tm t = {0};
                t.tm_year = gps.date.year() - 1900;
                t.tm_mon  = gps.date.month() - 1;
                t.tm_mday = gps.date.day();
                t.tm_hour = gps.time.hour();
                t.tm_min  = gps.time.minute();
                t.tm_sec  = gps.time.second();
                t.tm_isdst = 0;
                time_t epoch = my_timegm(&t);// time_t epoch = mktime(&t)
                struct timeval tv;
                tv.tv_sec = epoch;
                //tv.tv_usec = gps.time.centisecond() * 10000; // optional
                settimeofday(&tv, nullptr);
                Serial.println("-> set first datetime (Task delete)");
                //~ datetime_is_set_initial = true;
                sync_level = 1;
                // ...danach wird diese Task erstmal nicht mehr benötigt
                //~ vTaskSuspend(handle_task_set_first_datetime);
                vTaskDelete(handle_task_set_first_datetime);
        }        
        taskYIELD();
    }
}
```

Aber mktime() nimmt eingestellte Zeitzone, welche kein UTC sein könnte, zur Berechnung. Wir müssen also temporär auf UTC und dann wieder zurück schalten:

```cpp
time_t my_timegm(struct tm *tm) {
    char *tz = getenv("TZ");
    setenv("TZ", "UTC", 1);
    tzset();

    time_t t = mktime(tm);

    if (tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");

    tzset();
    return t;
}
```

Also direkt einbauen, da wir es nur einmal brauchen:

Statt:

```cpp
...
time_t epoch = my_timegm(&t);
...
```

So:

```cpp
...
setenv("TZ", "UTC", 1); tzset();  // UTC einstellen
time_t epoch = my_timegm(&t);
setenv("TZ", MY_TZ, 1); tzset();  // lokale Zeitzone einstellen
...
```

## Schreiben auf OLED "zentralisieren"

Nur eine Task, die auf OLED schreibt. Alle anderen Task schicken via Queue einen aufbereiteten Text+"display_area".

- [x] Erledigt?

Erweitern (Prinzip):

```cpp
// Display-Definitionen
struct display_areas_t {
    int x;
    int y;
    int w;
    int h;
    int size;
    int fg;
    int bg;
};
...
constexpr display_areas_t oled_area[AREA_COUNT] = {
  {0, 36, SCREEN_WIDTH, 24, 1, SSD1327_WHITE, SSD1327_BLACK},   // LOCATION
  {0, 66, SCREEN_WIDTH, 40, 1, SSD1327_WHITE,SSD1327_BLACK},    // SATELLITES
  {0, 111, SCREEN_WIDTH, 16, 1, SSD1327_WHITE,SSD1327_BLACK},   // PPS_INTERVAL
  {0,  0, SCREEN_WIDTH, 32, 2, SSD1327_WHITE,SSD1327_BLACK}     // DATE_TIME
};
```

Neue OLED-Task:

```cpp
#define MAX_OLED_TEXT 128
struct oled_t {
    long area;
    char text[MAX_OLED_TEXT];
};
...

// FreeRTOS Queues
QueueHandle_t queue_oled;
...

// in setup()
    queue_oled = xQueueCreate(5, sizeof(oled_t));   
    if (queue_phaseadj == NULL) {
        Serial.println("Problem bei Erzeugung queue_oled!");
        while (1);
    } 
...


// *********************************************************************
void task_msg2oled(void *pvParameters) {
    oled_t msg;
    for (;;) {
        if (xQueueReceive(queue_oled, &msg, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(mutex_oled, 1000)) {
                oled.fillRect(oled_area[msg.area].x, 
                              oled_area[msg.area].y, 
                              oled_area[msg.area].w, 
                              oled_area[msg.area].h, 
                              oled_area[msg.area].bg);
                oled.setTextSize(oled_area[msg.area].size);
                oled.setTextColor(oled_area[msg.area].fg);
                oled.setCursor(oled_area[msg.area].x, 
                               oled_area[msg.area].y);
                oled.printf(msg.text);
                oled.display();
                xSemaphoreGive(mutex_oled);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));        
    }
}
```

Gesendet wird dann z.B.:

```cpp
// in einer Task...

oled_t msg;
...
msg.area = LOCATION;
snprintf(msg.text, sizeof(msg.text), "Wert1=%d\nWert2=%d", wert1, wert2);
xQueueSend(queue_oled, &msg, portMAX_DELAY);

...
```

==>> jetzt müssen nur noch die geeigneten Stelle zum "Senden" gefunden werden. Evtl. bleiben die bestehenden Sendertasks in "abgespeckter" Form bestehen...

- [x] Erledigt?

## NTP-Request-Zähler einbauen

- [x] Erledigt?

In Task task_ntpserver einen Zähler einbauen, der die NTP-Requests zählt und diesen auf dem OLED mit ausgeben.

## Code insgesamt aufräumen

- [x] Erledigt?

- diverse Variablen-, Prozedur-Namen etc. aus Gesamtsicht vereinheitlichen

- Kommentare

- "Helper"-Routinen evtl. auslagern (--> so viele sind es nicht, also erstmal nicht auslagern...)

## phase-Regler verbessern (task_adjtime)

**Vermutung:** adjtime() und der PI-Regler in der Task arbeiten gegeneinander

**Idee:** kein adjtime() bei <u>jedem</u> PPS-Impuls aufrufen, sondern nur dann, wenn PPS-Impuls <u>und</u> das letzte adjtime() fertig ist.

adjtime()-Status abfragen:

```cpp
bool adjtime_done() {
    struct timeval remaining = {0};

    // NULL = keine neue Korrektur setzen
    // remaining = noch ausstehende Korrektur
    adjtime(NULL, &remaining);

    return (remaining.tv_sec == 0 && remaining.tv_usec == 0);

}
```

Abfrage direkt bei adjtime() in task_adjtime einbauen (kein adjtime(), wenn adjtime_done() == false). Evtl. diesen Status auch in MQTT-Message packen (Visualisierung Regler-Verhalten in Influx/Grafana).

- [ ] Erledigt?

## Was passiert, wenn auch GPS-Modul initialisiert wird

Testen und evtl. Problemen entgegenwirken

- [ ] Getestet?

## Was muss passieren, wenn PPS-Signal für längere Zeit nicht da?

Zeit driftet weg; kritisch, wenn Drift > 0,5s

- [ ] Geklärt?

## task_gps_read verbessern

Noch mal klären, ob es so optimal ist...

- [ ] Geklärt?

## Schicke(s) Tool(s), um Qualität dieses NTP-Server mit anderen zu vergleichen

hmmm, noch keine richtige Idee, bis auf dieses Python-Script...
