// ESP32 Server - Lichtschranken-Zeitmessung mit Ampelsteuerung
// Fungiert als Access Point und steuert die erste Lichtschranke mit Ampelsequenz

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>

// WiFi-Konfiguration als Access Point
// Der Server erstellt sein eigenes Netzwerk, damit die Verbindung
// unabhängig von externen Netzwerken funktioniert
const char *ssid = "MeinESP32AP";
const char *password = "meinPasswort123";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
WiFiServer server(80);
WiFiClient client;

// HC-SR04 Ultraschallsensor
const int trigPin1 = 5;
const int echoPin1 = 18;
#define SOUND_SPEED 0.034f  // cm/µs bei 20°C

// Ampel-LEDs
const int rledPin = 25;
const int yledPin = 26;
const int gledPin = 27;

// Timing-Konstanten für Ampelsequenz und Systemverhalten
const unsigned long YELLOW_PENDING_DELAY_MS = 500;              // Verzögerung nach Objekterkennung bis Gelb angeht
const unsigned long RED_PENDING_DELAY_AFTER_YELLOW_MS = 2000;   // Gelb-Phase Dauer vor Rot
const unsigned long LOOP_DELAY_MS = 20;                         // Kurze Loop-Verzögerung für ~50Hz Abtastrate
const unsigned long CLIENT_TIMEOUT_MS = 10000;                  // Timeout wenn Client nicht antwortet
const float MIN_VALID_DISTANCE = 2.0f;                          // HC-SR04 Minimum (technisches Limit)
const float MAX_VALID_DISTANCE = 400.0f;                        // HC-SR04 Maximum (technisches Limit)
const float HYSTERESIS_FACTOR = 1.15f;                         // Verhindert Prellen beim Objektverlassen (15% Puffer)
const int REFERENCE_SAMPLES = 15;                              // Anzahl Kalibrierungsmessungen für stabilen Mittelwert
const unsigned long MAX_TIMING_DURATION_MS = 30000;            // Maximale Messzeit als Sicherheitsmechanismus

// State Machine für präzise Ablaufsteuerung
// Jeder Zustand hat eine klar definierte Aufgabe und Übergangsbedingung
enum State
{
    SYSTEM_INIT,                        // Initialisierung beim Start
    IDLE_GREEN,                         // Wartet auf Objekterkennung (Grün leuchtet)
    OBJECT_DETECTED_YELLOW_PENDING,     // Objekt erkannt, Verzögerung vor Gelb
    YELLOW_ON_RED_PENDING,              // Gelb-Phase aktiv, wartet auf Rot
    RED_ON_WAITING_FOR_OBJECT_LEAVE,    // Rot an, wartet bis Objekt die Schranke verlässt
    TIMING_STARTED_ALL_ON,              // Zeitmessung läuft (alle LEDs an als Signal)
    WAITING_FOR_TIMING_COMPLETE,        // Cooldown nach Messung
    ERROR_STATE                         // Fehlerbehandlung mit Blink-Pattern
};
State currentState = SYSTEM_INIT;

// Sensor-Kalibrierung und Schwellwerte
float referenceDistance1 = -1.0f;       // Gemessene Referenzdistanz beim Start (leerer Messbereich)
float triggerThreshold1 = -1.0f;        // Auslöseschwelle = Referenz / 2

// Zeitstempel für State-Übergänge
unsigned long objectDetectedTime = 0;
unsigned long yellowLightOnTime = 0;
unsigned long timingStartTime = 0;
unsigned long lastValidMeasurement = 0;
unsigned long displayStartTime = 0;

// Verbindungs- und Zustandsmanagement
bool clientConnected = false;
int consecutiveInvalidReadings = 0;
const int MAX_INVALID_READINGS = 10;    // Nach 10 Fehlmessungen → Sensor-Fehler
bool timingInProgress = false;          // Kritisches Flag: Verhindert Mehrfach-Messungen

// Timing-Sicherheit und Heartbeat
const unsigned long MIN_TIME_BETWEEN_MEASUREMENTS_MS = 2000;  // Verhindert zu schnelle Messfolgen
const unsigned long HEARTBEAT_INTERVAL_MS = 5000;            // Prüft Verbindung alle 5s
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;

// Interrupt-Variablen für präzisere Echo-Messung (noch nicht aktiv genutzt)
volatile bool measurementReady = false;
volatile unsigned long pulseDuration = 0;

// Statistik für Qualitätskontrolle und Debugging
struct Statistics {
    unsigned long totalMeasurements = 0;
    unsigned long successfulMeasurements = 0;
    float minTime = 999999;
    float maxTime = 0;
    float avgTime = 0;
    unsigned long lastResetTime = 0;
    
    void addMeasurement(float time) {
        totalMeasurements++;
        if (time > 0) {
            successfulMeasurements++;
            minTime = min(minTime, time);
            maxTime = max(maxTime, time);
            avgTime = (avgTime * (successfulMeasurements - 1) + time) / successfulMeasurements;
        }
    }
    
    String toJSON() {
        return String("{\"total\":") + totalMeasurements + 
               ",\"success\":" + successfulMeasurements +
               ",\"min\":" + minTime +
               ",\"max\":" + maxTime +
               ",\"avg\":" + avgTime + "}";
    }
};
Statistics stats;

// Function Prototypes
void IRAM_ATTR echoISR();
float measureDistance(int trigPin, int echoPin);
void setTrafficLight(bool red, bool yellow, bool green);
void handleClientCommunication();
bool establishInitialReferenceDistance();
void handleSystemError(const String &errorMsg);
void resetSystem();
bool isValidDistance(float distance);
void updateClientStatus();
void printSystemStatus();
float measureDistanceWithMedianFilter(int trigPin, int echoPin, int samples = 5);
void logMeasurement(unsigned long time);
void initSPIFFS();

void setup()
{
    Serial.begin(115200);
    delay(100);

    // SPIFFS für persistente Datenspeicherung
    initSPIFFS();

    pinMode(trigPin1, OUTPUT);
    pinMode(echoPin1, INPUT);
    pinMode(rledPin, OUTPUT);
    pinMode(yledPin, OUTPUT);
    pinMode(gledPin, OUTPUT);

    // Interrupt-Setup für zukünftige Optimierung der Echo-Messung
    attachInterrupt(digitalPinToInterrupt(echoPin1), echoISR, CHANGE);

    // LED-Funktionstest zeigt Betriebsbereitschaft
    Serial.println("\nESP1: LED Test...");
    setTrafficLight(true, true, true);
    delay(1000);
    setTrafficLight(false, false, false);
    delay(500);

    // Access Point erstellen für direkte ESP-zu-ESP Kommunikation
    Serial.println("ESP1: Konfiguriere Access Point...");
    WiFi.softAPConfig(local_ip, gateway, subnet);

    if (WiFi.softAP(ssid, password))
    {
        Serial.println("ESP1: Access Point gestartet!");
        Serial.print("ESP1: SSID: ");
        Serial.println(ssid);
        Serial.print("ESP1: IP: ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        handleSystemError("Access Point konnte nicht gestartet werden!");
        return;
    }

    server.begin();
    Serial.println("ESP1: TCP Server gestartet auf Port 80");
    Serial.println("ESP1: Warte auf Client-Verbindungen...");

    // Kritisch: Sensor muss kalibriert werden um Umgebungsbedingungen zu kompensieren
    if (!establishInitialReferenceDistance())
    {
        Serial.println("ESP1: WARNUNG - Sensor-Kalibrierung fehlgeschlagen!");
        handleSystemError("Sensor-Kalibrierung fehlgeschlagen!");
        return;
    }

    triggerThreshold1 = referenceDistance1 / 2.0f;
    Serial.print("ESP1: System bereit - Referenz: ");
    Serial.print(referenceDistance1);
    Serial.print("cm, Trigger: ");
    Serial.print(triggerThreshold1);
    Serial.println("cm");

    setTrafficLight(false, false, true); // Start mit Grün
    currentState = IDLE_GREEN;
    lastValidMeasurement = millis();
    stats.lastResetTime = millis();
}

bool establishInitialReferenceDistance()
{
    Serial.println("ESP1: Kalibriere Sensor 1...");
    setTrafficLight(true, true, false); // Rot+Gelb signalisiert Kalibrierung

    float totalDist = 0;
    int validSamples = 0;

    // Mehrfachmessung mit Median-Filter für stabile Referenz
    for (int i = 0; i < REFERENCE_SAMPLES; i++)
    {
        float dist = measureDistanceWithMedianFilter(trigPin1, echoPin1);
        if (isValidDistance(dist))
        {
            totalDist += dist;
            validSamples++;
            Serial.print(".");
        }
        else
        {
            Serial.print("x");
        }
        delay(100);
    }
    Serial.println();

    // Mindestens 50% gültige Messungen erforderlich für verlässliche Kalibrierung
    if (validSamples >= REFERENCE_SAMPLES / 2)
    {
        referenceDistance1 = totalDist / validSamples;
        Serial.print("ESP1: Referenzdistanz: ");
        Serial.print(referenceDistance1);
        Serial.print("cm (");
        Serial.print(validSamples);
        Serial.print("/");
        Serial.print(REFERENCE_SAMPLES);
        Serial.println(" Samples)");
        return true;
    }
    else
    {
        Serial.print("ESP1: Zu wenige gültige Messungen: ");
        Serial.print(validSamples);
        Serial.print("/");
        Serial.println(REFERENCE_SAMPLES);
        return false;
    }
}

bool isValidDistance(float distance)
{
    return (distance > MIN_VALID_DISTANCE && distance < MAX_VALID_DISTANCE);
}

float measureDistance(int trigPin, int echoPin)
{
    // HC-SR04 Trigger-Sequenz: 10µs HIGH-Puls startet Messung
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Wartet auf Echo-Puls, Timeout verhindert Blockierung bei fehlendem Echo
    long duration = pulseIn(echoPin, HIGH, 30000); // 30ms = ~5m Reichweite
    if (duration == 0)
    {
        return -1.0f;
    }
    return (duration * SOUND_SPEED / 2.0f); // Hin- und Rückweg, daher /2
}

void setTrafficLight(bool red, bool yellow, bool green)
{
    digitalWrite(rledPin, red ? HIGH : LOW);
    digitalWrite(yledPin, yellow ? HIGH : LOW);
    digitalWrite(gledPin, green ? HIGH : LOW);

    // Reduziertes Logging verhindert Serial-Buffer-Überlauf
    static unsigned long lastLEDLog = 0;
    if (millis() - lastLEDLog > 1000)
    {
        Serial.print("ESP1: LEDs - R:");
        Serial.print(red ? "ON" : "OFF");
        Serial.print(" Y:");
        Serial.print(yellow ? "ON" : "OFF");
        Serial.print(" G:");
        Serial.println(green ? "ON" : "OFF");
        lastLEDLog = millis();
    }
}

void updateClientStatus()
{
    // Client-Verwaltung: Nur eine Verbindung gleichzeitig erlaubt
    if (!client || !client.connected())
    {
        if (clientConnected)
        {
            Serial.println("ESP1: Client getrennt");
            clientConnected = false;
        }

        WiFiClient newClient = server.available();
        if (newClient)
        {
            // Alte Verbindung sauber beenden bevor neue akzeptiert wird
            if (client && client.connected())
            {
                client.stop();
            }
            client = newClient;
            clientConnected = true;
            Serial.println("ESP1: Neuer Client verbunden!");
        }
    }
    else
    {
        clientConnected = true;
    }
}

void handleSystemError(const String &errorMsg)
{
    Serial.print("ESP1: SYSTEM FEHLER - ");
    Serial.println(errorMsg);
    currentState = ERROR_STATE;

    // Visuelles Fehlersignal: 5x rotes Blinken
    for (int i = 0; i < 5; i++)
    {
        setTrafficLight(true, false, false);
        delay(200);
        setTrafficLight(false, false, false);
        delay(200);
    }
}

void resetSystem()
{
    Serial.println("ESP1: System Reset");
    objectDetectedTime = 0;
    yellowLightOnTime = 0;
    timingStartTime = 0;
    consecutiveInvalidReadings = 0;
    timingInProgress = false;  // KRITISCH: Muss zurückgesetzt werden für nächste Messung
    setTrafficLight(false, false, true);
    currentState = IDLE_GREEN;
}

void printSystemStatus()
{
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 5000)
    { // Alle 5 Sekunden
        Serial.print("ESP1: State=");
        Serial.print(currentState);
        Serial.print(", Client=");
        Serial.print(clientConnected ? "OK" : "NO");
        Serial.print(", Timing=");
        Serial.print(timingInProgress ? "YES" : "NO");
        Serial.print(", Ref=");
        Serial.print(referenceDistance1);
        Serial.println("cm");
        lastStatusPrint = millis();
    }
}

void loop()
{
    updateClientStatus();
    printSystemStatus();

    float currentDistance1 = measureDistanceWithMedianFilter(trigPin1, echoPin1);

    // Sensor-Gesundheitsüberwachung erkennt defekte/blockierte Sensoren
    if (!isValidDistance(currentDistance1))
    {
        consecutiveInvalidReadings++;
        if (consecutiveInvalidReadings > MAX_INVALID_READINGS)
        {
            handleSystemError("Sensor ausgefallen - zu viele ungültige Messungen");
            delay(1000);
            return;
        }
    }
    else
    {
        consecutiveInvalidReadings = 0;
        lastValidMeasurement = millis();
    }

    // Hauptzustandsmaschine steuert Messablauf
    switch (currentState)
    {
    case IDLE_GREEN:
        // timingInProgress-Check verhindert Überlappung von Messungen
        if (!timingInProgress && isValidDistance(currentDistance1) &&
            currentDistance1 <= triggerThreshold1)
        {
            Serial.print("ESP1: Objekt erkannt! Distanz: ");
            Serial.print(currentDistance1);
            Serial.print("cm <= ");
            Serial.print(triggerThreshold1);
            Serial.println("cm");

            objectDetectedTime = millis();
            setTrafficLight(false, false, false); // Dunkelphase vor Gelb für klare Sequenz
            currentState = OBJECT_DETECTED_YELLOW_PENDING;
        }
        else if (timingInProgress)
        {
            // Periodische Warnung bei versuchter Mehrfachmessung
            static unsigned long lastWarning = 0;
            if (millis() - lastWarning > 5000)
            {
                Serial.println("ESP1: WARNUNG - Zeitmessung läuft noch!");
                lastWarning = millis();
            }
        }
        break;

    case OBJECT_DETECTED_YELLOW_PENDING:
        if (millis() - objectDetectedTime >= YELLOW_PENDING_DELAY_MS)
        {
            Serial.println("ESP1: Gelb AN");
            setTrafficLight(false, true, false);
            yellowLightOnTime = millis();
            currentState = YELLOW_ON_RED_PENDING;
        }
        break;

    case YELLOW_ON_RED_PENDING:
        if (millis() - yellowLightOnTime >= RED_PENDING_DELAY_AFTER_YELLOW_MS)
        {
            Serial.println("ESP1: Rot AN - Warte auf Objektverlassen");
            setTrafficLight(true, false, false);
            currentState = RED_ON_WAITING_FOR_OBJECT_LEAVE;
        }
        break;

    case RED_ON_WAITING_FOR_OBJECT_LEAVE:
        // Hysterese verhindert Fehlauslösung durch Messrauschen
        if (isValidDistance(currentDistance1) &&
            currentDistance1 > triggerThreshold1 * HYSTERESIS_FACTOR)
        {
            Serial.print("ESP1: Objekt verlassen! Distanz: ");
            Serial.print(currentDistance1);
            Serial.print("cm > ");
            Serial.print(triggerThreshold1 * HYSTERESIS_FACTOR);
            Serial.println("cm");

            setTrafficLight(true, true, true); // Alle LEDs = Zeitmessung aktiv

            if (clientConnected)
            {
                client.println("START_TIMER");
                Serial.println("ESP1: START_TIMER gesendet");
                timingStartTime = millis();
                timingInProgress = true; // KRITISCH: Blockiert neue Messungen bis Abschluss
                currentState = TIMING_STARTED_ALL_ON;
            }
            else
            {
                Serial.println("ESP1: FEHLER - Kein Client verbunden!");
                handleSystemError("Kein Client für Zeitmessung");
            }
        }
        break;

    case TIMING_STARTED_ALL_ON:
        // Sicherheitstimeout falls Client nicht antwortet oder Objekt nie ankommt
        if (millis() - timingStartTime > MAX_TIMING_DURATION_MS)
        {
            Serial.println("ESP1: Zeitmessung Timeout!");
            handleSystemError("Zeitmessung Timeout");
        }
        // Wartet auf STOP_TIMER vom Client
        break;

    case WAITING_FOR_TIMING_COMPLETE:
        // Erzwungene Pause verhindert zu schnelle Messfolgen
        if (millis() - displayStartTime >= MIN_TIME_BETWEEN_MEASUREMENTS_MS)
        {
            resetSystem();
            Serial.println("ESP1: Bereit für nächste Messung");
        }
        break;

    case ERROR_STATE:
        // Selbstheilungsversuch nach 5 Sekunden
        delay(5000);
        Serial.println("ESP1: Versuche System-Wiederherstellung...");
        if (establishInitialReferenceDistance())
        {
            triggerThreshold1 = referenceDistance1 / 2.0f;
            timingInProgress = false; // Muss explizit zurückgesetzt werden
            resetSystem();
        }
        break;

    default:
        handleSystemError("Unbekannter Systemzustand");
        break;
    }

    handleClientCommunication();
    delay(LOOP_DELAY_MS);
}

void handleClientCommunication()
{
    if (clientConnected && client.available())
    {
        String clientData = client.readStringUntil('\n');
        clientData.trim();
        Serial.print("ESP1: Client: '");
        Serial.print(clientData);
        Serial.println("'");

        if (clientData.startsWith("STOP_TIMER"))
        {
            Serial.println("ESP1: STOP_TIMER empfangen");

            // Protokoll: "STOP_TIMER:12345" mit Zeit in Millisekunden
            int colonIndex = clientData.indexOf(':');
            unsigned long measuredTime = 0;
            if (colonIndex != -1)
            {
                String timeValue = clientData.substring(colonIndex + 1);
                Serial.print("ESP1: Gemessene Zeit: ");
                Serial.print(timeValue);
                Serial.println("ms");
                
                measuredTime = timeValue.toInt();
                stats.addMeasurement(measuredTime / 1000.0);
                
                // Persistente Speicherung für spätere Analyse
                logMeasurement(measuredTime);
            }
            
            timingInProgress = false; // Gibt System für nächste Messung frei
            currentState = WAITING_FOR_TIMING_COMPLETE;
            displayStartTime = millis();
            
            setTrafficLight(false, true, false); // Gelb = Ergebnis empfangen
            Serial.println("ESP1: Cooldown-Phase gestartet");
            Serial.print("ESP1: Statistik - ");
            Serial.println(stats.toJSON());
        }
        else if (clientData.startsWith("CLIENT_READY"))
        {
            Serial.println("ESP1: Client bereit");
        }
        else if (clientData.startsWith("HEARTBEAT_ACK"))
        {
            lastHeartbeatReceived = millis();
        }
        else
        {
            Serial.print("ESP1: Unbekannte Nachricht: ");
            Serial.println(clientData);
        }
    }
    
    // Heartbeat-Mechanismus erkennt stille Verbindungsabbrüche
    if (clientConnected && millis() - lastHeartbeatSent > HEARTBEAT_INTERVAL_MS)
    {
        client.println("HEARTBEAT");
        lastHeartbeatSent = millis();
        Serial.println("ESP1: Heartbeat gesendet");
    }
}

// ISR für zukünftige präzisere Echo-Zeitmessung (vorbereitet, noch nicht aktiv)
void IRAM_ATTR echoISR() {
    static unsigned long startTime = 0;
    if (digitalRead(echoPin1)) {
        startTime = micros();
    } else {
        pulseDuration = micros() - startTime;
        measurementReady = true;
    }
}

// Median-Filter eliminiert Ausreißer durch Ultraschall-Reflexionen
float measureDistanceWithMedianFilter(int trigPin, int echoPin, int samples) {
    float measurements[samples];
    
    for (int i = 0; i < samples; i++) {
        measurements[i] = measureDistance(trigPin, echoPin);
        if (measurements[i] < 0) {
            measurements[i] = MAX_VALID_DISTANCE + 1; // Sortiert Fehler ans Ende
        }
        delayMicroseconds(500); // Verhindert Echo-Überlagerungen
    }
    
    // Einfacher Bubble Sort reicht für kleine Datenmengen
    for (int i = 0; i < samples - 1; i++) {
        for (int j = 0; j < samples - i - 1; j++) {
            if (measurements[j] > measurements[j + 1]) {
                float temp = measurements[j];
                measurements[j] = measurements[j + 1];
                measurements[j + 1] = temp;
            }
        }
    }
    
    // Median ist robuster als Mittelwert gegen Ausreißer
    float median = measurements[samples / 2];
    return (median > MAX_VALID_DISTANCE) ? -1.0f : median;
}

// SPIFFS für persistente Datenspeicherung über Neustarts hinweg
void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("ESP1: SPIFFS Mount fehlgeschlagen");
        return;
    }
    
    Serial.println("ESP1: SPIFFS erfolgreich gemountet");
    
    // Automatische Rotation bei 100KB verhindert Speicherüberlauf
    File file = SPIFFS.open("/measurements.csv");
    if (file && file.size() > 100000) {
        file.close();
        SPIFFS.remove("/measurements.csv");
        Serial.println("ESP1: Alte Logs gelöscht");
    } else if (file) {
        file.close();
    }
}

// CSV-Logging für spätere Analyse und Qualitätssicherung
void logMeasurement(unsigned long time) {
    File file = SPIFFS.open("/measurements.csv", FILE_APPEND);
    if (!file) {
        Serial.println("ESP1: Fehler beim Öffnen der Log-Datei");
        return;
    }
    
    // CSV-Format ermöglicht einfache Analyse in Excel/Python
    file.printf("%lu,%lu,%s,%.2f\n", 
        millis(),                                   // Zeitstempel seit Boot
        time,                                       // Gemessene Zeit in ms
        clientConnected ? "OK" : "NO_CLIENT",      // Verbindungsstatus
        referenceDistance1                          // Aktuelle Kalibrierung
    );
    
    file.close();
    Serial.println("ESP1: Messung geloggt");
}