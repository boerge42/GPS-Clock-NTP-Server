/* *********************************************************************
* 
* GPS-Uhr mit NTP-Server
* ======================
*    Uwe Berger; 2026
* 
* 
* Hardware:
* ---------
* - ESP32-P4-ETH
* - GPS-Modul mit PPS-Signal; z.B. GT-U7
* - OLED SSD1327
* 
* 
* Funktionalitäten:
* -----------------
*  - konsequente Umsetzung aller Funktionen als FreeRTOS-Tasks
*  - Auslesen GPS-Modul via serieller Schnittstelle
*  - initiales Setzen Datum/Uhrzeit aus NMEA-Daten des GPS-Moduls
*  - Synchronisation der Uhrzeit nach PPS-Signal (Anfang der Sekunde)
*  - einfacher NTP-Server 
*  - Ausgabe diverser Informationen auf OLED
*  - Senden von diversen Informationen via MQTT
* 
* 
* =========
* Have fun!
*
*  
***********************************************************************/

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <time.h>
#include <ETH.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>

// MQTT-Definitionen
#define MQTT_BROKER         "nanotuxedo"
#define MQTT_PORT           1883
#define MQTT_USER           ""
#define MQTT_PWD            ""
#define MQTT_CLIENT_ID      "esp32_ntp_server"
#define MQTT_TOPIC          "esp32-ntp-server/"
#define MQTT_BUFFER_SIZE    200

// Ethernet-Client
WiFiClient ethClient;
#define HOSTNAME            "esp32-ntp-server"          

// MQTT-Client
PubSubClient mqtt_client(ethClient);

// UDP-Client (NTP)
WiFiUDP udp;
const int NTP_PORT = 123;

// serielle Schnittstelle
SoftwareSerial gpsSerial;

// HW-Definitionen SW-Serial
#define RX_PIN          32
#define TX_PIN          33

// Definitionen TinyGPSPlus (plus einiger Custom-Values)
TinyGPSPlus gps;
TinyGPSCustom sats_in_view(gps, "GPGSV", 3);    //Anzahl sichtbarer Satelliten
TinyGPSCustom fix_type(gps, "GPGSA", 2);        // Fixum
TinyGPSCustom pdop(gps, "GPGSA", 15);           // PDOP-Wert
TinyGPSCustom vdop(gps, "GPGSA", 17);           // VDOP-Wert

// HW-Definitionen für SSD1327-Display via SW-SPI
#define OLED_CLK        21
#define OLED_MOSI       20
#define OLED_CS         22
#define OLED_DC         23
#define OLED_RESET      26   // eigtl. nicht notw. (-1)

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   128

Adafruit_SSD1327 oled(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// HW-Definition PPS-Pin
#define PPS_PIN         27

// FreeRTOS Mutexe...
SemaphoreHandle_t mutex_oled = NULL;

// FreeRTOS Queues
QueueHandle_t queue_phaseadj2mqtt;
QueueHandle_t queue_oled;

// FreeRTOS Task-Handle
TaskHandle_t handle_task_set_first_datetime = nullptr;
TaskHandle_t handle_task_sync_datetime = nullptr;
TaskHandle_t handle_task_adjtime = nullptr;

// Display-Definitionen
struct display_areas_t {
    int x;      // x-Position
    int y;      // y-Position
    int w;      // Breite (x-Richtg.)
    int h;      // Höhe (y-Richtg.)
    int size;   // Fontgröße
    int fg;     // Farbe Vordergrund
    int bg;     // Farbe Hintergrund
};
// ..."Namen" der Display-Bereiche 
enum display_area_index {
    DATE_TIME,
    LOCATION,
    SATELLITES,
    PPS_INTERVAL,
    ETH_STATUS,
    NTP_REQUESTS,
    AREA_COUNT
};
// ...Konfiguration Display-Bereiche
constexpr display_areas_t oled_area[AREA_COUNT] = {
  {0,   0, SCREEN_WIDTH, 32, 2, SSD1327_WHITE, SSD1327_BLACK},        // DATE_TIME
  {0,  35, SCREEN_WIDTH, 24, 1, SSD1327_WHITE, SSD1327_BLACK},        // LOCATION
  {0,  64, SCREEN_WIDTH, 24, 1, SSD1327_WHITE, SSD1327_BLACK},        // SATELLITES
  {0,  93, SCREEN_WIDTH,  8, 1, SSD1327_WHITE, SSD1327_BLACK},        // PPS_INTERVAL
  {0, 106, SCREEN_WIDTH,  8, 1, SSD1327_WHITE, SSD1327_BLACK},        // ETH_STATUS
  {0, 119, SCREEN_WIDTH,  8, 1, SSD1327_WHITE, SSD1327_BLACK},        // NTP_REQUESTS
};
// ...Message-Definition für queue_msg2oled
#define MAX_OLED_TEXT 128
struct oled_t {
    long area;
    char text[MAX_OLED_TEXT];
};

// meine lokale Zeitzone
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"

// globale Variable micros()-Timestamp des aktuellen PPS-Signals
volatile unsigned long last_pps_micros = 0;

// globale Variable für aktuellen Status der Uhr-Synchronisation
volatile int sync_level = 0;    // 0: keinerlei Sync; 
                                // 1: datetime ist initial gesetzt; 
                                // 2: datetime ist mit PPS synchronisiert  

// globale Variable PPS-Impuls lag an
volatile boolean pps_is_set = false;

// Message-Defintion für queue_phaseadj2mqtt
struct phaseadj_t {
    long phase;
    long adj;
    float filtered;
    float i;
    long latenz;
};

// Definitionen für NTP-Server
#define NTP_UNIX_OFFSET 2208988800UL

typedef struct {
  uint8_t li_vn_mode;
  uint8_t stratum;
  uint8_t poll;
  int8_t precision;
  uint32_t rootDelay;
  uint32_t rootDispersion;
  uint32_t refId;
  uint32_t refTimestampSec;
  uint32_t refTimestampFrac;
  uint32_t origTimestampSec;
  uint32_t origTimestampFrac;
  uint32_t recvTimestampSec;
  uint32_t recvTimestampFrac;
  uint32_t txTimestampSec;
  uint32_t txTimestampFrac;
} ntp_packet_t;

// ETH-Hardware
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   31
#define ETH_PHY_MDIO  52
#define ETH_PHY_POWER 51
#define ETH_CLK_MODE  EMAC_CLK_EXT_IN

// globale Variable ETH hat eine IP-Adresse...
bool eth_connected = false;

// ********************************************************************************
// ETH-Events
void eth_event(arduino_event_id_t event) {
    oled_t msg;
    IPAddress ip;
    
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            snprintf(msg.text, sizeof(msg.text), "%s", "ETH Started");
            Serial.println(msg.text);
            ETH.setHostname(HOSTNAME);
            break;
        case ARDUINO_EVENT_ETH_CONNECTED: 
            snprintf(msg.text, sizeof(msg.text), "%s", "ETH Connected");
            Serial.println(msg.text);
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            //~ snprintf(msg.text, sizeof(msg.text), "%s", "ETH Got IP");
            //~ Serial.println(msg.text);
            Serial.println(ETH);
            ip = ETH.localIP();
            snprintf(msg.text, sizeof(msg.text), "IP: %u.%u.%u.%u",
                     ip[0], ip[1], ip[2], ip[3]);             
            eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            snprintf(msg.text, sizeof(msg.text), "%s", "ETH Lost IP");
            Serial.println(msg.text);
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            snprintf(msg.text, sizeof(msg.text), "%s", "ETH Disconnected");
            Serial.println(msg.text);
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            snprintf(msg.text, sizeof(msg.text), "%s", "ETH Stopped");
            Serial.println(msg.text);
            eth_connected = false;
            break;
        default: 
            break;
    }
    // Statustext an OLED-Task senden
    msg.area = ETH_STATUS;
    xQueueSend(queue_oled, &msg, portMAX_DELAY);
}

// ********************************************************************************
void mqtt_connect(void) {
    uint8_t count = 0;

    Serial.println("Connect to MQTT-Broker: ");
    mqtt_client.setServer(MQTT_BROKER, MQTT_PORT);
    while (!mqtt_client.connected()) {
        count++;
        if (mqtt_client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PWD)) {
            Serial.println("MQTT connected!");  
        } else {
            Serial.print(".");
            Serial.print(mqtt_client.state());
        }
        if (count > 10) {
            Serial.println("...no connection (MQTT-Broker)!!!");
            Serial.flush();
            while(1){};
        }
        delay(100);
    } 
    mqtt_client.setBufferSize(MQTT_BUFFER_SIZE); 
}

// *********************************************************************
void IRAM_ATTR isr_pps_signal() {
    pps_is_set = true;
    last_pps_micros = micros();
    // je nach Sync-Level, Signal an richtige Task senden
    if (sync_level == 2) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(handle_task_adjtime, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else
    if (sync_level == 1) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(handle_task_sync_datetime, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// *********************************************************************
uint64_t get_ntp_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t seconds = tv.tv_sec + NTP_UNIX_OFFSET;
  uint64_t fraction = 
              (uint64_t)((double)tv.tv_usec * (double)(1LL<<32) / 1e6);
  return (seconds << 32) | fraction;
}

// *********************************************************************
String fixtype2text(int fix)
{
    switch (fix)
    {
        case 1: return "no Fix";
        case 2: return "2D-Fix";
        case 3: return "3D-Fix";
        default: return "n/a";
    }
}

// *********************************************************************
String dopvalue2qualitytext(float dop)
{
  if (dop < 1.0)       return "Excell"; // Excellent
  else if (dop < 2.0)  return "V.Good";  // Very Good
  else if (dop < 5.0)  return "Good";   // Good
  else if (dop < 10.0) return "Moder";  // Moderate
  else if (dop < 20.0) return "Poor";   // Poor
  else                 return "V.Poor";  // Very Poor
}

// *********************************************************************
void task_gps_read(void *pvParameters) {
    for (;;) {
        while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// *********************************************************************
void task_location2oled(void *pvParameters) {
    oled_t msg;
    for (;;) {
        if (gps.location.isValid()) {
            msg.area = LOCATION;
            snprintf(msg.text, sizeof(msg.text), "Lat.: %.7f\nLon.: %.7f\nAlt.: %.1fm",
                      gps.location.lat(),
                      gps.location.lng(),
                      gps.altitude.meters());
            xQueueSend(queue_oled, &msg, portMAX_DELAY);
        }        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// *********************************************************************
void task_satellites2oled(void *pvParameters) {
    oled_t msg;
    for (;;) {
        msg.area = SATELLITES;
        snprintf(msg.text, sizeof(msg.text), "Sat.: %d/%d (%s)\nHDOP | VDOP | PDOP\n%.2f | %.2f | %.2f",
                  gps.satellites.value(),
                  atoi(sats_in_view.value()),
                  fixtype2text(atoi(fix_type.value())),
                  gps.hdop.hdop(),
                  atof(vdop.value()),
                  atof(pdop.value())
                  );
        xQueueSend(queue_oled, &msg, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// *********************************************************************
// ...wird nur einmal zum Setzen von Datum/Uhrzeit aus den GPS-Daten
// benötigt welche aber noch nicht mit dem PPS-Signal synchronisiert 
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
                
                //~ time_t epoch = my_timegm(&t);
                setenv("TZ", "UTC", 1); tzset();  // UTC
                time_t epoch = mktime(&t);
                setenv("TZ", MY_TZ, 1); tzset();  // lokale Zeitzone
                
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

// *********************************************************************
// ...vorher gesetztes Datum/Uhrzeit einmalig mit PPS-Signal synchronisieren
void task_sync_datetime(void *pvParameters) {
    for (;;) {
        if (atoi(fix_type.value()) == 3) {
            
            // Blockiert bis PPS kommt
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            
            pps_is_set = false;
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            // PPS kommt zur vollen Sekunde
            long latenz = micros() - last_pps_micros;
            tv.tv_sec += 1;         // PPS-Signal VOR der Sekunde, die gemeint ist!
            tv.tv_usec  = latenz;   // 0+latenz; Laufzeit zw. PPS-Signal und dieser Stelle einberechnen
            settimeofday(&tv, nullptr);
            sync_level = 2;
            Serial.printf("-> sync datetime (Task delete); latenz zu PPS=%ldus\n", latenz);
            // ...danach nur noch adjtime(drift) via task_adjtime()
            //~ vTaskSuspend(handle_task_sync_datetime);
            vTaskDelete(handle_task_sync_datetime);
        }
    }
}

// *********************************************************************
// adjtime() mit PPS-Interval-Drift etc. (PI-Regler)
void task_adjtime(void *pvParameters) {
    
    static float filtered = 0.0f;
    static float I = 0.0f;

    const float alpha = 0.05f;
    const float Kp    = 1.0f / 6.0f;
    const float Ki    = 0.0001f;
    
    oled_t msg_oled;
    phaseadj_t msg;

    for (;;) {
        // Blockiert bis PPS kommt
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        pps_is_set = false; // ...falls wir es nochmal brauchen
        
        // PI-Regler phase
        // phase ermitteln
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        long latenz = micros() - last_pps_micros;
        long phase = tv.tv_usec;
        if (phase > 500000)  phase -= 1000000;
        if (phase < -500000) phase += 1000000;
        
        // 1) Filter
        filtered = filtered * (1.0f - alpha) + phase * alpha;
        // 2) Integrator – WICHTIG: GENAU SO
        I += filtered * Ki;          // kein Minus, Ki > 0
        // Anti-Windup
        if (I > 200.0f)  I = 200.0f;
        if (I < -200.0f) I = -200.0f;
        // 3) PI-Regler
        float u = Kp * filtered + I; // Reglerausgang
        long adj_usec = -(long)u;    // adjtime bekommt das NEGATIVE
        // adjtime()
        struct timeval adj;
        adj.tv_sec  = 0;
        adj.tv_usec = adj_usec;
        adjtime(&adj, nullptr);
        
        // --> MQTT
        msg.phase = phase;
        msg.adj   = adj_usec;
        msg.filtered = filtered;
        msg.i = I;
        msg.latenz = latenz;
        xQueueSend(queue_phaseadj2mqtt, &msg, portMAX_DELAY);
        
        // --> OLED
        msg_oled.area = PPS_INTERVAL;
        snprintf(msg_oled.text, sizeof(msg_oled.text), "Phase: %dus", phase);
        xQueueSend(queue_oled, &msg_oled, portMAX_DELAY);

        Serial.printf("phase=%ld filtered=%.2f I=%.2f adj=%ld latenz(pps)=%ld\n",
                      phase, filtered, I, adj_usec, latenz);
    }
}

// *********************************************************************
void task_datetime2oled(void *pvParameters) {
    time_t now;
    tm tm;
    int old_sec = -1;
    oled_t msg;
    for (;;) {
        time(&now);
        localtime_r(&now, &tm);
        if (old_sec != tm.tm_sec) {
            old_sec = tm.tm_sec;
            msg.area = DATE_TIME;
            snprintf(msg.text, sizeof(msg.text), "%02d.%02d.%04d\n%02d:%02d:%02d",
                        tm.tm_mday,
                        tm.tm_mon+1,
                        tm.tm_year+1900,
                        tm.tm_hour,
                        tm.tm_min,
                        tm.tm_sec
                       );
            xQueueSend(queue_oled, &msg, portMAX_DELAY);
        }        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// *********************************************************************
// zentrale Task, die auf das OLED schreibt; Mutex eigentlich nicht notwendig,
// wenn es so bleibt
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
        vTaskDelay(pdMS_TO_TICKS(10));        
    }
}

// *********************************************************************
// ein paar interessante Werte via MQTT in die Welt senden...
void task_phaseadj2mqtt(void *pvParameters) {
    phaseadj_t msg;
    char mqtt_payload[MQTT_BUFFER_SIZE];
    for (;;) {
        if (xQueueReceive(queue_phaseadj2mqtt, &msg, portMAX_DELAY) == pdTRUE) {
            // MQTT senden
            sprintf(mqtt_payload, "%s phase=%d,adj=%d,filtered=%f,i=%f,latenz=%d", 
                    MQTT_CLIENT_ID,
                    msg.phase,
                    msg.adj,
                    msg.filtered,
                    msg.i,
                    msg.latenz
                   );
            mqtt_client.publish(MQTT_TOPIC, mqtt_payload, false);
        }
    }
}

// *********************************************************************
// Verarbeitung von NTP-Requests
void task_ntpserver(void *param) {
    static long ntp_counter = 0;
    oled_t msg;
    for (;;) {
        if (sync_level == 2) {
            if (udp.parsePacket() == 48) {
                ntp_packet_t request;
                udp.read((uint8_t*)&request, 48);

                uint64_t recvTS = get_ntp_timestamp();
                uint64_t txTS   = get_ntp_timestamp();

                ntp_packet_t reply = {0};

                reply.li_vn_mode = 0b00100100; // LI=0, Version=4, Mode=4
                reply.stratum = 1;             // wenn z.B. GPS-Zeit mit PPS...
                reply.poll = 6;                // 6 gut; evtl. 10 bei Stratum 1
                reply.precision = -20;         // guter Wert für GPS-PPS auf ESP32

                //~ reply.rootDelay = htonl(1 << 16);       // 1.0s???
                //~ reply.rootDispersion = htonl(1 << 16);  // 1.0s???
                
                reply.rootDelay = htonl((uint32_t)(0.000015 * 65536.0));        // 15us
                reply.rootDispersion = htonl((uint32_t)(0.000005 * 65536.0));   // 5us
                
                reply.refId = htonl(0x47505300); // "GPS" + 0 (PPS?)

                uint64_t refTS = get_ntp_timestamp();
                reply.refTimestampSec  = htonl((uint32_t)(refTS >> 32));
                reply.refTimestampFrac = htonl((uint32_t)(refTS & 0xFFFFFFFF));

                reply.origTimestampSec  = request.txTimestampSec;
                reply.origTimestampFrac = request.txTimestampFrac;

                reply.recvTimestampSec  = htonl((uint32_t)(recvTS >> 32));
                reply.recvTimestampFrac = htonl((uint32_t)(recvTS & 0xFFFFFFFF));

                reply.txTimestampSec  = htonl((uint32_t)(txTS >> 32));
                reply.txTimestampFrac = htonl((uint32_t)(txTS & 0xFFFFFFFF));

                udp.beginPacket(udp.remoteIP(), udp.remotePort());
                udp.write((uint8_t*)&reply, 48);
                udp.endPacket();
                
                ntp_counter++;
                
                // Counter auf OLED anzeigen
                msg.area = NTP_REQUESTS;
                snprintf(msg.text, sizeof(msg.text), "NTP-Req.: %d", ntp_counter);
                xQueueSend(queue_oled, &msg, portMAX_DELAY);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// *********************************************************************
void setup() {
    Serial.begin(115200);

    // serielle Schnittstelle für GPS-Modul initialisieren
    gpsSerial.begin(9600, SWSERIAL_8N1, RX_PIN, TX_PIN, false);
    if (!gpsSerial) {
        Serial.println("Fehler: Pins nicht geeignet!");
    }

    // PPS-Signal von GPS-Modul
    pinMode(PPS_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PPS_PIN), isr_pps_signal, RISING);

    // OLED initialisieren
    if (!oled.begin() ) {
     Serial.println("SSD1327-OLED kann nicht initialisiert werden!");
     while (1);
    }
    oled.setRotation(3);
    oled.clearDisplay();
    oled.display();
    
    // ...lokale Zeitzone
    setenv("TZ", MY_TZ, 1);
    tzset();

    // FreeRTOS Mutexe
    mutex_oled = xSemaphoreCreateMutex();
    if (mutex_oled == NULL) {
        Serial.println("Problem bei Erzeugung mutex_oled!");
        while (1);
    } 
    
    // FreeRTOS Queues
    queue_phaseadj2mqtt = xQueueCreate(5, sizeof(phaseadj_t));   
    if (queue_phaseadj2mqtt == NULL) {
        Serial.println("Problem bei Erzeugung queue_phaseadj2mqtt!");
        while (1);
    }
    queue_oled = xQueueCreate(10, sizeof(oled_t));   
    if (queue_oled == NULL) {
        Serial.println("Problem bei Erzeugung queue_oled!");
        while (1);
    }        

    // FreeRTOS-Tasks starten
    xTaskCreatePinnedToCore(task_gps_read, "task_gps_read", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_location2oled, "task_location2oled", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_phaseadj2mqtt, "task_phaseadj2mqtt", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_datetime2oled, "task_datetime2oled", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_satellites2oled, "task_satellites2oled", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_set_first_datetime, "task_set_first_datetime", 4096, NULL, 1, &handle_task_set_first_datetime, 1);
    xTaskCreatePinnedToCore(task_sync_datetime, "task_sync_datetime", 4096, NULL, 1, &handle_task_sync_datetime, 1);
    xTaskCreatePinnedToCore(task_adjtime, "task_adjtime", 4096, NULL, 4, &handle_task_adjtime, 1);
    xTaskCreatePinnedToCore(task_ntpserver, "task_ntpserver", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_msg2oled, "task_msg2oled", 4096, NULL, 1, NULL, 1);
    
    // Ethernet
    Network.onEvent(eth_event);  // eth_event() wird aufgerufen, wenn es einen Status-Wechsel an ETH gibt
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
    
    // MQTT
    mqtt_connect(); 

    // UDP (NTP)
    udp.begin(NTP_PORT);

}

// *********************************************************************
// *********************************************************************
// *********************************************************************
void loop() {
    // hier gibt es nichts; Steuerung via FreeRTOS-Tasks etc.
}
