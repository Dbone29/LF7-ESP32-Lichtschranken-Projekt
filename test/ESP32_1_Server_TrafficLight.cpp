// ESP32_1_Server_TrafficLight.ino

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

// WiFi AP Settings
const char *ssid = "MeinESP32AP";
const char *password = "meinPasswort123";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
WiFiServer server(80);
WiFiClient client; // Global client object for the connected ESP32

// Sensor 1 Pins (Update with your actual ESP32 pins)
const int trigPin1 = 5;    // Example pin
const int echoPin1 = 18;   // Example pin
#define SOUND_SPEED 0.034f // cm/microsecond

// LED Pins (Traffic Light - Update with your actual ESP32 pins)
const int rledPin = 25; // Example pin for Red LED
const int yledPin = 26; // Example pin for Yellow LED
const int gledPin = 27; // Example pin for Green LED

// States for ESP1
enum State
{
    IDLE_GREEN,
    OBJECT_DETECTED_YELLOW_PENDING,
    YELLOW_ON_RED_PENDING,
    RED_ON_WAITING_FOR_OBJECT_LEAVE,
    TIMING_STARTED_ALL_ON // Waiting for STOP signal from ESP2
};
State currentState = IDLE_GREEN;

// Distances
float referenceDistance1 = -1.0f; // Initial distance reading before an object arrives
float currentDistance1 = -1.0f;
float triggerThreshold1; // referenceDistance1 / 2

// Timers
unsigned long objectDetectedTime = 0;
unsigned long yellowLightOnTime = 0;
const unsigned long YELLOW_PENDING_DELAY_MS = 500;            // 0.5 seconds
const unsigned long RED_PENDING_DELAY_AFTER_YELLOW_MS = 2000; // 2 seconds

// Function Prototypes
float measureDistance(int trigPin, int echoPin);
void setTrafficLight(bool red, bool yellow, bool green);
void handleClientCommunication();
bool establishInitialReferenceDistance();

void setup()
{
    Serial.begin(115200);
    delay(100); // Wait for serial to initialize

    // Sensor 1
    pinMode(trigPin1, OUTPUT);
    pinMode(echoPin1, INPUT);

    // LEDs
    pinMode(rledPin, OUTPUT);
    pinMode(yledPin, OUTPUT);
    pinMode(gledPin, OUTPUT);

    Serial.println("\nESP1: Konfiguriere SoftAP...");
    WiFi.softAPConfig(local_ip, gateway, subnet);
    if (WiFi.softAP(ssid, password))
    {
        Serial.println("ESP1: SoftAP gestartet.");
        Serial.print("ESP1: SSID: ");
        Serial.println(ssid);
        Serial.print("ESP1: IP-Adresse des SoftAP: ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("ESP1: Fehler beim Starten des SoftAP!");
        while (1)
            delay(1000); // Halt
    }

    server.begin();
    Serial.println("ESP1: TCP Server gestartet auf Port 80.");
    Serial.println("ESP1: Warte auf eingehende Verbindungen...");

    if (!establishInitialReferenceDistance())
    {
        Serial.println("ESP1: FEHLER bei der initialen Distanzmessung. Bitte Sensor prüfen und neu starten.");
        // Loop indefinitely or set a default and try to continue
        referenceDistance1 = 50.0; // Fallback default
        Serial.print("ESP1: Setze Referenzdistanz auf Default: ");
        Serial.print(referenceDistance1);
        Serial.println(" cm");
    }

    triggerThreshold1 = referenceDistance1 / 2.0f;
    Serial.print("ESP1: Trigger Distanz (Hälfte der Referenz): ");
    Serial.print(triggerThreshold1);
    Serial.println(" cm");

    setTrafficLight(false, false, true); // Start with Green ON
    currentState = IDLE_GREEN;
}

bool establishInitialReferenceDistance()
{
    Serial.println("ESP1: Messe initiale Referenzdistanz (Sensor 1)...");
    float totalDist = 0;
    int validSamples = 0;
    for (int i = 0; i < 10; i++)
    { // Take 10 samples
        float dist = measureDistance(trigPin1, echoPin1);
        if (dist > 0 && dist < 400)
        { // Consider readings between 0 and 400cm valid
            totalDist += dist;
            validSamples++;
        }
        delay(100); // Wait a bit between samples
    }

    if (validSamples > 0)
    {
        referenceDistance1 = totalDist / validSamples;
        Serial.print("ESP1: Initiale Referenzdistanz: ");
        Serial.print(referenceDistance1);
        Serial.println(" cm");
        return true;
    }
    else
    {
        referenceDistance1 = -1.0f; // Indicate failure
        return false;
    }
}

float measureDistance(int trigPin, int echoPin)
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    // Timeout for pulseIn: 25000 microseconds (approx 4.25m max range)
    long duration = pulseIn(echoPin, HIGH, 25000);
    if (duration == 0)
    { // Timeout or no pulse
        return -1.0f;
    }
    return (duration * SOUND_SPEED / 2.0f);
}

void setTrafficLight(bool red, bool yellow, bool green)
{
    digitalWrite(rledPin, red ? HIGH : LOW);
    digitalWrite(yledPin, yellow ? HIGH : LOW);
    digitalWrite(gledPin, green ? HIGH : LOW);
}

void loop()
{
    // Check for new client connection if current one is not valid or not connected
    if (!client || !client.connected())
    {
        WiFiClient newClient = server.available();
        if (newClient)
        {
            Serial.println("ESP1: Neuer Client verbunden!");
            if (client && client.connected())
            { // If an old client was connected, stop it
                client.stop();
            }
            client = newClient; // Assign the new client
        }
    }

    currentDistance1 = measureDistance(trigPin1, echoPin1);
    // Add a small Serial print for debugging distance, can be removed later
    // Serial.print("ESP1 Distanz: "); Serial.print(currentDistance1); Serial.print(" cm, Zustand: "); Serial.println(currentState);

    if (currentDistance1 <= 0 && currentState != TIMING_STARTED_ALL_ON)
    {
        // Invalid reading, possibly too close or sensor error
        // Don't change state based on invalid readings unless waiting for client
        // Serial.println("ESP1: Ungültige Distanzmessung.");
        delay(50);                   // Short delay before retrying
        handleClientCommunication(); // Still handle client comms
        return;
    }

    switch (currentState)
    {
    case IDLE_GREEN:
        // Green light is ON
        // Toleranz: Distanz muss sich halbieren
        if (currentDistance1 > 0 && currentDistance1 <= triggerThreshold1)
        {
            Serial.println("ESP1: Objekt erkannt! -> Gelb-Phase startet bald.");
            objectDetectedTime = millis();
            setTrafficLight(false, false, false); // Turn off Green immediately
            currentState = OBJECT_DETECTED_YELLOW_PENDING;
        }
        break;

    case OBJECT_DETECTED_YELLOW_PENDING:
        // Green is OFF, waiting for 0.5s
        if (millis() - objectDetectedTime >= YELLOW_PENDING_DELAY_MS)
        {
            Serial.println("ESP1: Gelb AN -> Rot-Phase startet bald.");
            setTrafficLight(false, true, false); // Yellow ON
            yellowLightOnTime = millis();
            currentState = YELLOW_ON_RED_PENDING;
        }
        break;

    case YELLOW_ON_RED_PENDING:
        // Yellow is ON, waiting for 2s
        if (millis() - yellowLightOnTime >= RED_PENDING_DELAY_AFTER_YELLOW_MS)
        {
            Serial.println("ESP1: Rot AN. Warte bis Objekt Bereich verlässt.");
            setTrafficLight(true, false, false); // Red ON, Yellow OFF
            currentState = RED_ON_WAITING_FOR_OBJECT_LEAVE;
        }
        // Optional: if object leaves during yellow, what happens?
        // Current logic proceeds to Red.
        break;

    case RED_ON_WAITING_FOR_OBJECT_LEAVE:
        // Red is ON
        // "Wenn die Distanz wieder größer wird" (als die Hälfte der Referenz)
        if (currentDistance1 > 0 && currentDistance1 > triggerThreshold1 * 1.1)
        { // Object has left (added 10% hysteresis)
            Serial.println("ESP1: Objekt hat Bereich verlassen. Alle Lichter AN. Sende START_TIMER.");
            setTrafficLight(true, true, true); // All ON
            if (client && client.connected())
            {
                client.println("START_TIMER"); // Send command to ESP2
                Serial.println("ESP1: 'START_TIMER' an Client gesendet.");
            }
            else
            {
                Serial.println("ESP1: Client nicht verbunden. 'START_TIMER' konnte nicht gesendet werden.");
                // What to do here? Maybe reset? For now, just logs.
            }
            currentState = TIMING_STARTED_ALL_ON;
        }
        break;

    case TIMING_STARTED_ALL_ON:
        // All lights are ON, waiting for "STOP_TIMER" from client
        // Communication is handled in handleClientCommunication()
        // To prevent this state from re-evaluating distance and potentially
        // re-sending START_TIMER, we only process client messages here.
        break;
    }

    handleClientCommunication(); // Handle incoming messages in all states

    delay(50); // Loop delay; 20Hz. Adjust as needed. (50ms = 1 "tick" if 10 ticks = 0.5s)
}

void handleClientCommunication()
{
    if (client && client.connected() && client.available())
    {
        String clientData = client.readStringUntil('\n');
        clientData.trim();
        Serial.print("ESP1: Daten vom Client empfangen: '");
        Serial.print(clientData);
        Serial.println("'");

        if (clientData.startsWith("STOP_TIMER"))
        {
            Serial.println("ESP1: 'STOP_TIMER' vom Client empfangen.");
            // Optional: Extract time if ESP1 needs it for anything
            // String timeValue = "";
            // int colonIndex = clientData.indexOf(':');
            // if (colonIndex != -1) {
            //    timeValue = clientData.substring(colonIndex + 1);
            //    Serial.print("ESP1: Gemessene Zeit vom Client: ");
            //    Serial.println(timeValue);
            // }

            setTrafficLight(false, false, true); // Reset to Green ON
            currentState = IDLE_GREEN;
            Serial.println("ESP1: System zurückgesetzt. Ampel Grün.");
            // Optional: Re-calibrate reference distance for next cycle?
            // establishInitialReferenceDistance();
            // if(referenceDistance1 > 0) triggerThreshold1 = referenceDistance1 / 2.0f;
        }
        else if (clientData.startsWith("CLIENT_READY"))
        {
            Serial.println("ESP1: Client ist bereit.");
            // Can be used for handshake if needed
        }
    }
}
