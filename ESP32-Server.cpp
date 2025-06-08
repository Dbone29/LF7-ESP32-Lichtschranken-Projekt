// ESP32_1_Server_TrafficLight.ino - Verbesserte Version

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>


// WiFi AP Settings
const char *ssid = "MeinESP32AP";
const char *password = "meinPasswort123";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
WiFiServer server(80);
WiFiClient client;

// Sensor 1 Pins
const int trigPin1 = 5;
const int echoPin1 = 18;
#define SOUND_SPEED 0.034f

// LED Pins (Traffic Light)
const int rledPin = 25; // Red LED
const int yledPin = 26; // Yellow LED
const int gledPin = 27; // Green LED

// Constants
const unsigned long YELLOW_PENDING_DELAY_MS = 500;
const unsigned long RED_PENDING_DELAY_AFTER_YELLOW_MS = 2000;
const unsigned long LOOP_DELAY_MS = 20;        // Reduziert für bessere Responsivität
const unsigned long CLIENT_TIMEOUT_MS = 10000; // 10s timeout für Client-Antwort
const float MIN_VALID_DISTANCE = 2.0f;
const float MAX_VALID_DISTANCE = 400.0f;
const float HYSTERESIS_FACTOR = 1.15f; // 15% Hysterese
const int REFERENCE_SAMPLES = 15;
const unsigned long MAX_TIMING_DURATION_MS = 30000; // 30s max Messzeit

// States
enum State
{
    SYSTEM_INIT,
    IDLE_GREEN,
    OBJECT_DETECTED_YELLOW_PENDING,
    YELLOW_ON_RED_PENDING,
    RED_ON_WAITING_FOR_OBJECT_LEAVE,
    TIMING_STARTED_ALL_ON,
    WAITING_FOR_TIMING_COMPLETE,
    ERROR_STATE
};
State currentState = SYSTEM_INIT;

// Variables
float referenceDistance1 = -1.0f;
float triggerThreshold1 = -1.0f;
unsigned long objectDetectedTime = 0;
unsigned long yellowLightOnTime = 0;
unsigned long timingStartTime = 0;
unsigned long lastValidMeasurement = 0;
unsigned long displayStartTime = 0; // Für Cooldown-Timer
bool clientConnected = false;
int consecutiveInvalidReadings = 0;
const int MAX_INVALID_READINGS = 10;
bool timingInProgress = false;
const unsigned long MIN_TIME_BETWEEN_MEASUREMENTS_MS = 2000; // 2 Sekunden Mindestabstand
const unsigned long HEARTBEAT_INTERVAL_MS = 5000; // 5 Sekunden Heartbeat
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;

// Interrupt-basierte Messung
volatile bool measurementReady = false;
volatile unsigned long pulseDuration = 0;

// Statistik-Tracking
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


    // SPIFFS initialisieren
    initSPIFFS();

    // Pin Setup
    pinMode(trigPin1, OUTPUT);
    pinMode(echoPin1, INPUT);
    pinMode(rledPin, OUTPUT);
    pinMode(yledPin, OUTPUT);
    pinMode(gledPin, OUTPUT);

    // Interrupt für Echo-Pin
    attachInterrupt(digitalPinToInterrupt(echoPin1), echoISR, CHANGE);

    // Initial LED test
    Serial.println("\nESP1: LED Test...");
    setTrafficLight(true, true, true);
    delay(1000);
    setTrafficLight(false, false, false);
    delay(500);

    // WiFi AP Setup
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

    // Server Setup
    server.begin();
    Serial.println("ESP1: TCP Server gestartet auf Port 80");
    Serial.println("ESP1: Warte auf Client-Verbindungen...");

    // Sensor Kalibrierung
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
    setTrafficLight(true, true, false); // Gelb + Rot während Kalibrierung

    float totalDist = 0;
    int validSamples = 0;

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
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
    if (duration == 0)
    {
        return -1.0f;
    }
    return (duration * SOUND_SPEED / 2.0f);
}

void setTrafficLight(bool red, bool yellow, bool green)
{
    digitalWrite(rledPin, red ? HIGH : LOW);
    digitalWrite(yledPin, yellow ? HIGH : LOW);
    digitalWrite(gledPin, green ? HIGH : LOW);

    // Debug output für LED Status
    static unsigned long lastLEDLog = 0;
    if (millis() - lastLEDLog > 1000)
    { // Nur alle Sekunde loggen
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
    // Prüfe Client-Verbindung
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

    // Error Blink Pattern
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
    timingInProgress = false; // Wichtig: Flag zurücksetzen
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

    // Überwache Sensor-Gesundheit
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

    // State Machine
    switch (currentState)
    {
    case IDLE_GREEN:
        if (!timingInProgress && isValidDistance(currentDistance1) &&
            currentDistance1 <= triggerThreshold1)
        {
            Serial.print("ESP1: Objekt erkannt! Distanz: ");
            Serial.print(currentDistance1);
            Serial.print("cm <= ");
            Serial.print(triggerThreshold1);
            Serial.println("cm");

            objectDetectedTime = millis();
            setTrafficLight(false, false, false); // Alle aus
            currentState = OBJECT_DETECTED_YELLOW_PENDING;
        }
        else if (timingInProgress)
        {
            // Zeitmessung läuft noch - ignoriere neue Objekte
            static unsigned long lastWarning = 0;
            if (millis() - lastWarning > 5000) // Warnung alle 5 Sekunden
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
        if (isValidDistance(currentDistance1) &&
            currentDistance1 > triggerThreshold1 * HYSTERESIS_FACTOR)
        {
            Serial.print("ESP1: Objekt verlassen! Distanz: ");
            Serial.print(currentDistance1);
            Serial.print("cm > ");
            Serial.print(triggerThreshold1 * HYSTERESIS_FACTOR);
            Serial.println("cm");

            setTrafficLight(true, true, true); // Alle AN

            if (clientConnected)
            {
                client.println("START_TIMER");
                Serial.println("ESP1: START_TIMER gesendet");
                timingStartTime = millis();
                timingInProgress = true; // Markiere Timing als aktiv
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
        // Timeout für Zeitmessung
        if (millis() - timingStartTime > MAX_TIMING_DURATION_MS)
        {
            Serial.println("ESP1: Zeitmessung Timeout!");
            handleSystemError("Zeitmessung Timeout");
        }
        // Hauptsächlich Client-Kommunikation
        break;

    case WAITING_FOR_TIMING_COMPLETE:
        // Cooldown-Phase nach Zeitmessung
        if (millis() - displayStartTime >= MIN_TIME_BETWEEN_MEASUREMENTS_MS)
        {
            resetSystem();
            Serial.println("ESP1: Bereit für nächste Messung");
        }
        break;

    case ERROR_STATE:
        // Warte auf manuellen Reset oder versuche automatische Wiederherstellung
        delay(5000);
        Serial.println("ESP1: Versuche System-Wiederherstellung...");
        if (establishInitialReferenceDistance())
        {
            triggerThreshold1 = referenceDistance1 / 2.0f;
            timingInProgress = false; // Stelle sicher, dass Flag zurückgesetzt wird
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

            // Extrahiere Zeit falls vorhanden
            int colonIndex = clientData.indexOf(':');
            unsigned long measuredTime = 0;
            if (colonIndex != -1)
            {
                String timeValue = clientData.substring(colonIndex + 1);
                Serial.print("ESP1: Gemessene Zeit: ");
                Serial.print(timeValue);
                Serial.println("ms");
                
                // Zeit in Statistik aufnehmen
                measuredTime = timeValue.toInt();
                stats.addMeasurement(measuredTime / 1000.0);
                
                // Messung in SPIFFS loggen
                logMeasurement(measuredTime);
            }
            
            timingInProgress = false; // Timing beendet
            currentState = WAITING_FOR_TIMING_COMPLETE;
            displayStartTime = millis(); // Für Cooldown-Timer
            
            // Zeige Ergebnis für 2 Sekunden
            setTrafficLight(false, true, false); // Nur Gelb
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
    
    // Heartbeat senden
    if (clientConnected && millis() - lastHeartbeatSent > HEARTBEAT_INTERVAL_MS)
    {
        client.println("HEARTBEAT");
        lastHeartbeatSent = millis();
        Serial.println("ESP1: Heartbeat gesendet");
    }
}

// Interrupt Service Routine für Echo-Pin
void IRAM_ATTR echoISR() {
    static unsigned long startTime = 0;
    if (digitalRead(echoPin1)) {
        startTime = micros();
    } else {
        pulseDuration = micros() - startTime;
        measurementReady = true;
    }
}

// Median-Filter für robustere Messungen
float measureDistanceWithMedianFilter(int trigPin, int echoPin, int samples) {
    float measurements[samples];
    
    for (int i = 0; i < samples; i++) {
        measurements[i] = measureDistance(trigPin, echoPin);
        if (measurements[i] < 0) {
            measurements[i] = MAX_VALID_DISTANCE + 1; // Ungültige Werte ans Ende sortieren
        }
        delayMicroseconds(500);
    }
    
    // Bubble Sort für Median
    for (int i = 0; i < samples - 1; i++) {
        for (int j = 0; j < samples - i - 1; j++) {
            if (measurements[j] > measurements[j + 1]) {
                float temp = measurements[j];
                measurements[j] = measurements[j + 1];
                measurements[j + 1] = temp;
            }
        }
    }
    
    // Median zurückgeben
    float median = measurements[samples / 2];
    return (median > MAX_VALID_DISTANCE) ? -1.0f : median;
}

// SPIFFS initialisieren
void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("ESP1: SPIFFS Mount fehlgeschlagen");
        return;
    }
    
    Serial.println("ESP1: SPIFFS erfolgreich gemountet");
    
    // Alte Logs löschen wenn zu groß
    File file = SPIFFS.open("/measurements.csv");
    if (file && file.size() > 100000) { // 100KB Limit
        file.close();
        SPIFFS.remove("/measurements.csv");
        Serial.println("ESP1: Alte Logs gelöscht");
    } else if (file) {
        file.close();
    }
}

// Messung in SPIFFS loggen
void logMeasurement(unsigned long time) {
    File file = SPIFFS.open("/measurements.csv", FILE_APPEND);
    if (!file) {
        Serial.println("ESP1: Fehler beim Öffnen der Log-Datei");
        return;
    }
    
    // Format: Timestamp, Messzeit(ms), Client-Status, Referenzdistanz
    file.printf("%lu,%lu,%s,%.2f\n", 
        millis(), 
        time, 
        clientConnected ? "OK" : "NO_CLIENT",
        referenceDistance1
    );
    
    file.close();
    Serial.println("ESP1: Messung geloggt");
}