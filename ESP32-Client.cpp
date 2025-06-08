// ESP32_2_Client_Sensor.ino - Verbesserte Version

#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// Alternative: ESP32-LiquidCrystal-I2C oder ESP32LiquidCrystal

// WiFi Client Settings
const char *ssid_ap = "MeinESP32AP";
const char *password_ap = "meinPasswort123";

// Server Settings
IPAddress serverIP(192, 168, 4, 1);
const uint16_t serverPort = 80;

WiFiClient client;

// Display Settings (HW-61 I2C LCD)
LiquidCrystal_I2C lcd(0x3F, 20, 4); // I2C address 0x3F, 20x4 display
// Häufige Adressen: 0x27, 0x3F, 0x26, 0x20

// Sensor 2 Pins (Geändert um Konflikte zu vermeiden)
const int trigPin2 = 12; // Geändert von 5
const int echoPin2 = 14; // Geändert von 18
#define SOUND_SPEED 0.034f

// Constants
const unsigned long RECONNECT_DELAY_MS = 2000;
const unsigned long LOOP_DELAY_MS = 20; // Reduziert für bessere Genauigkeit
const unsigned long CONNECTION_TIMEOUT_MS = 15000;
const float MIN_VALID_DISTANCE = 2.0f;
const float MAX_VALID_DISTANCE = 400.0f;
const int REFERENCE_SAMPLES = 15; // Mehr Samples für bessere Genauigkeit

// States
enum StateClient
{
    WAITING_FOR_CONNECTION,
    IDLE_WAITING_FOR_START,
    TIMING_IN_PROGRESS,
    DISPLAYING_RESULT
};
StateClient clientState = WAITING_FOR_CONNECTION;

// Variables
float referenceDistance2 = -1.0f;
float triggerThreshold2 = -1.0f;
unsigned long timingStartTime = 0;
unsigned long lastMeasuredTime = 0;
unsigned long lastConnectionAttempt = 0;
unsigned long displayStartTime = 0;
const unsigned long DISPLAY_DURATION_MS = 5000; // 5 Sekunden anzeigen
bool displayAvailable = false;

// Function Prototypes
float measureDistanceClient(int trigPin, int echoPin);
void connectToWiFiAndServer();
bool establishInitialReferenceDistanceClient();
void updateDisplay(const String &line1, const String &line2 = "", const String &line3 = "", const String &line4 = "");
void initializeDisplay();
void handleConnectionLoss();
void scanI2CDevices();

void setup()
{
    Serial.begin(115200);
    delay(100);

    // Sensor Setup
    pinMode(trigPin2, OUTPUT);
    pinMode(echoPin2, INPUT);

    // Display Setup
    initializeDisplay();

    Serial.println("\nESP2: Client Setup gestartet");
    updateDisplay("ESP2 Client", "Initialisierung...", "", "");

    clientState = WAITING_FOR_CONNECTION;
}

void scanI2CDevices() {
    Serial.println("ESP2: Scanne I2C Geräte...");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("ESP2: I2C Gerät gefunden bei Adresse 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("ESP2: Keine I2C Geräte gefunden!");
    } else {
        Serial.print("ESP2: ");
        Serial.print(deviceCount);
        Serial.println(" I2C Geräte gefunden");
    }
}

void initializeDisplay()
{
    Serial.println("ESP2: Initialisiere Display mit 5V...");
    Wire.begin(21, 22); // SDA=21, SCL=22
    Wire.setTimeout(1000); // 1 Sekunde Timeout
    Wire.setClock(50000); // Noch langsamer: 50kHz für 5V Display
    
    delay(500); // Display-Startup Zeit
    
    // Scanne I2C Geräte
    scanI2CDevices();
    
    // Teste nur gängige Adressen
    uint8_t addresses[] = {0x27, 0x3F, 0x20, 0x26};
    displayAvailable = false;
    
    for (int i = 0; i < 4; i++) {
        Serial.print("ESP2: Teste 5V-Display bei 0x");
        Serial.println(addresses[i], HEX);
        
        Wire.beginTransmission(addresses[i]);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("ESP2: 5V-Display gefunden bei 0x");
            Serial.println(addresses[i], HEX);
            
            lcd = LiquidCrystal_I2C(addresses[i], 20, 4);
            delay(100);
            lcd.init();
            delay(100);
            lcd.backlight();
            delay(100);
            
            // Test schreiben
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("5V Display OK!");
            lcd.setCursor(0, 1);
            lcd.print("Lichtschranke v1.0");
            
            displayAvailable = true;
            delay(2000);
            break;
        } else {
            Serial.print("ESP2: Fehler ");
            Serial.print(error);
            Serial.print(" bei 0x");
            Serial.println(addresses[i], HEX);
        }
        delay(100);
    }
    
    if (!displayAvailable) {
        Serial.println("ESP2: Kein 5V-Display gefunden - prüfe Verkabelung:");
        Serial.println("  VCC -> 5V (nicht 3.3V!)");
        Serial.println("  GND -> GND");
        Serial.println("  SDA -> GPIO 21");
        Serial.println("  SCL -> GPIO 22");
    } else {
        Serial.println("ESP2: 5V-Display erfolgreich initialisiert!");
    }
}

void updateDisplay(const String &line1, const String &line2, const String &line3, const String &line4)
{
    // Nur wenn Display verfügbar ist
    if (!displayAvailable) {
        Serial.println("ESP2: " + line1 + (line2.length() > 0 ? " | " + line2 : ""));
        return;
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    if (line2.length() > 0)
    {
        lcd.setCursor(0, 1);
        lcd.print(line2);
    }
    if (line3.length() > 0)
    {
        lcd.setCursor(0, 2);
        lcd.print(line3);
    }
    if (line4.length() > 0)
    {
        lcd.setCursor(0, 3);
        lcd.print(line4);
    }
}

void connectToWiFiAndServer()
{
    unsigned long currentTime = millis();

    // Verhindere zu häufige Verbindungsversuche
    if (currentTime - lastConnectionAttempt < RECONNECT_DELAY_MS)
    {
        return;
    }
    lastConnectionAttempt = currentTime;

    // WiFi Verbindung prüfen/aufbauen
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("ESP2: Verbinde mit WLAN '");
        Serial.print(ssid_ap);
        Serial.println("'...");
        updateDisplay("Verbinde WLAN...", ssid_ap, "", "");

        WiFi.begin(ssid_ap, password_ap);

        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED &&
               (millis() - startTime) < CONNECTION_TIMEOUT_MS)
        {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("\nESP2: WLAN-Verbindung fehlgeschlagen.");
            updateDisplay("WLAN Fehler!", "Erneut versuchen...", "", "");
            return;
        }

        Serial.println("\nESP2: WLAN verbunden!");
        Serial.print("ESP2: IP: ");
        Serial.println(WiFi.localIP());
        updateDisplay("WLAN verbunden", "IP: " + WiFi.localIP().toString(), "", "");
        delay(1000);
    }

    // Server Verbindung aufbauen
    if (!client.connected())
    {
        Serial.println("ESP2: Verbinde zum Server...");
        updateDisplay("Verbinde Server...", serverIP.toString() + ":" + String(serverPort), "", "");

        if (client.connect(serverIP, serverPort))
        {
            Serial.println("ESP2: Mit Server verbunden!");

            if (!establishInitialReferenceDistanceClient())
            {
                Serial.println("ESP2: WARNUNG - Referenzdistanz Fallback");
                referenceDistance2 = 50.0f;
                updateDisplay("Sensor Warnung!", "Fallback: 50cm", "Pruefen Sie Sensor", "");
                delay(2000);
            }

            triggerThreshold2 = referenceDistance2 / 2.0f;
            Serial.print("ESP2: Referenz: ");
            Serial.print(referenceDistance2);
            Serial.print("cm, Trigger: ");
            Serial.print(triggerThreshold2);
            Serial.println("cm");

            client.println("CLIENT_READY");
            clientState = IDLE_WAITING_FOR_START;
            updateDisplay("Bereit!", "Warte auf Start...",
                          "Referenz: " + String(referenceDistance2, 1) + "cm",
                          "Trigger: " + String(triggerThreshold2, 1) + "cm");
        }
        else
        {
            Serial.println("ESP2: Server-Verbindung fehlgeschlagen!");
            updateDisplay("Server Fehler!", "Verbindung fehlgesch.", "", "");
        }
    }
}

bool establishInitialReferenceDistanceClient()
{
    Serial.println("ESP2: Messe Referenzdistanz...");
    updateDisplay("Kalibrierung...", "Messe Referenz", "Bereich freihalten!", "");

    float totalDist = 0;
    int validSamples = 0;

    for (int i = 0; i < REFERENCE_SAMPLES; i++)
    {
        float dist = measureDistanceClient(trigPin2, echoPin2);
        if (dist > MIN_VALID_DISTANCE && dist < MAX_VALID_DISTANCE)
        {
            totalDist += dist;
            validSamples++;
        }

        // Fortschritt anzeigen
        if (displayAvailable) {
            lcd.setCursor(0, 3);
            lcd.print("Sample " + String(i + 1) + "/" + String(REFERENCE_SAMPLES));
        }
        delay(100);
    }

    if (validSamples >= REFERENCE_SAMPLES / 2)
    { // Mindestens 50% gültige Messungen
        referenceDistance2 = totalDist / validSamples;
        Serial.print("ESP2: Referenzdistanz: ");
        Serial.print(referenceDistance2);
        Serial.println("cm");
        return true;
    }
    else
    {
        Serial.println("ESP2: Kalibrierung fehlgeschlagen!");
        return false;
    }
}

float measureDistanceClient(int trigPin, int echoPin)
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

void handleConnectionLoss()
{
    if (!client.connected() && clientState != WAITING_FOR_CONNECTION)
    {
        Serial.println("ESP2: Verbindung verloren!");
        updateDisplay("Verbindung verloren!", "Reconnecting...", "", "");
        clientState = WAITING_FOR_CONNECTION;
        timingStartTime = 0; // Reset timing if in progress
    }
}

void loop()
{
    handleConnectionLoss();

    if (clientState == WAITING_FOR_CONNECTION)
    {
        connectToWiFiAndServer();
        delay(1000);
        return;
    }

    // Server Nachrichten verarbeiten
    if (client.available())
    {
        String serverData = client.readStringUntil('\n');
        serverData.trim();
        Serial.print("ESP2: Server: '");
        Serial.print(serverData);
        Serial.println("'");

        if (serverData.equals("START_TIMER") && clientState == IDLE_WAITING_FOR_START)
        {
            Serial.println("ESP2: Zeitmessung gestartet!");
            timingStartTime = millis();
            clientState = TIMING_IN_PROGRESS;
            updateDisplay("MESSUNG LAEUFT!", "Zeit: 0.000s",
                          "Warte auf Objekt...", "Ref: " + String(referenceDistance2, 1) + "cm");
        }
        else if (serverData.equals("START_TIMER") && clientState != IDLE_WAITING_FOR_START)
        {
            Serial.println("ESP2: WARNUNG - START_TIMER ignoriert, nicht bereit!");
            Serial.print("ESP2: Aktueller State: ");
            Serial.println(clientState);
        }
    }

    // Hauptlogik je nach State
    switch (clientState)
    {
    case TIMING_IN_PROGRESS:
    {
        float currentDistance2 = measureDistanceClient(trigPin2, echoPin2);
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - timingStartTime;

        // Live-Update der Zeit auf Display (alle 100ms)
        static unsigned long lastDisplayUpdate = 0;
        if (currentTime - lastDisplayUpdate >= 100)
        {
            updateDisplay("MESSUNG LAEUFT!",
                          "Zeit: " + String(elapsedTime / 1000.0, 3) + "s",
                          "Warte auf Objekt...",
                          "Dist: " + String(currentDistance2, 1) + "cm");
            lastDisplayUpdate = currentTime;
        }

        // Objekt erkannt?
        if (currentDistance2 > 0 && currentDistance2 <= triggerThreshold2)
        {
            lastMeasuredTime = elapsedTime;
            Serial.print("ESP2: Objekt erkannt! Zeit: ");
            Serial.print(lastMeasuredTime);
            Serial.println("ms");

            if (client.connected())
            {
                String message = "STOP_TIMER:" + String(lastMeasuredTime);
                client.println(message);
                Serial.print("ESP2: Gesendet: '");
                Serial.print(message);
                Serial.println("'");
            }
            else
            {
                Serial.println("ESP2: FEHLER - Nicht verbunden!");
            }

            clientState = DISPLAYING_RESULT;
            displayStartTime = millis();

            // Ergebnis anzeigen
            updateDisplay("ERGEBNIS:",
                          "Zeit: " + String(lastMeasuredTime / 1000.0, 3) + "s",
                          "= " + String(lastMeasuredTime) + "ms",
                          "Druecke Reset...");
        }
        break;
    }

    case DISPLAYING_RESULT:
    {
        // Ergebnis für bestimmte Zeit anzeigen, dann zurück zu IDLE
        if (millis() - displayStartTime >= DISPLAY_DURATION_MS)
        {
            clientState = IDLE_WAITING_FOR_START;
            updateDisplay("Bereit!", "Warte auf Start...",
                          "Letzte Zeit: " + String(lastMeasuredTime / 1000.0, 3) + "s",
                          "Ref: " + String(referenceDistance2, 1) + "cm");
        }
        break;
    }

    case IDLE_WAITING_FOR_START:
        // Nichts zu tun, warten auf START_TIMER
        break;

    default:
        break;
    }

    delay(LOOP_DELAY_MS);
}