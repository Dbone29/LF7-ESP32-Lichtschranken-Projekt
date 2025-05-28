// ESP32_2_Client_Sensor.ino

#include <WiFi.h>
#include <WiFiClient.h>

// WiFi Client Settings to connect to ESP1's AP
const char *ssid_ap = "MeinESP32AP";         // Must match ESP1's AP SSID
const char *password_ap = "meinPasswort123"; // Must match ESP1's AP Password

// IP-Address of the Server (ESP32_1)
IPAddress serverIP(192, 168, 4, 1); // Default AP IP of ESP1
const uint16_t serverPort = 80;     // Port on ESP1

WiFiClient client;

// Sensor 2 Pins (Update with your actual ESP32 pins)
const int trigPin2 = 5;    // Example pin, ensure it's different or on a separate ESP32
const int echoPin2 = 18;   // Example pin
#define SOUND_SPEED 0.034f // cm/microsecond

// States for ESP2
enum StateClient
{
    WAITING_FOR_CONNECTION,
    IDLE_WAITING_FOR_START,
    TIMING_IN_PROGRESS
};
StateClient clientState = WAITING_FOR_CONNECTION;

// Distances for Sensor 2
float referenceDistance2 = -1.0f;
float currentDistance2 = -1.0f;
float triggerThreshold2;

// Timer
unsigned long timingStartTime = 0;

// Function Prototypes
float measureDistanceClient(int trigPin, int echoPin);
void connectToWiFiAndServer();
bool establishInitialReferenceDistanceClient();

void setup()
{
    Serial.begin(115200);
    delay(100);

    // Sensor 2
    pinMode(trigPin2, OUTPUT);
    pinMode(echoPin2, INPUT);

    Serial.println("\nESP2: Client Setup");
    clientState = WAITING_FOR_CONNECTION;
}

void connectToWiFiAndServer()
{
    if (WiFi.status() == WL_CONNECTED && client.connected())
    {
        return; // Already connected
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("ESP2: Verbinde mit WLAN '");
        Serial.print(ssid_ap);
        Serial.println("'...");
        WiFi.begin(ssid_ap, password_ap);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30)
        { // Try for ~15 seconds
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("ESP2: WLAN-Verbindung fehlgeschlagen.");
            clientState = WAITING_FOR_CONNECTION;
            return;
        }
        Serial.println("ESP2: WLAN verbunden!");
        Serial.print("ESP2: Eigene IP-Adresse: ");
        Serial.println(WiFi.localIP());
    }

    if (!client.connected())
    {
        Serial.print("ESP2: Verbinde zum Server: ");
        Serial.print(serverIP);
        Serial.print(":");
        Serial.println(serverPort);
        if (client.connect(serverIP, serverPort))
        {
            Serial.println("ESP2: Verbunden mit dem Server!");
            if (!establishInitialReferenceDistanceClient())
            {
                Serial.println("ESP2: FEHLER bei der initialen Distanzmessung S2. Bitte Sensor prüfen.");
                referenceDistance2 = 50.0; // Fallback
                Serial.print("ESP2: Setze Referenzdistanz S2 auf Default: ");
                Serial.print(referenceDistance2);
                Serial.println(" cm");
            }
            triggerThreshold2 = referenceDistance2 / 2.0f;
            Serial.print("ESP2: Trigger Distanz S2: ");
            Serial.print(triggerThreshold2);
            Serial.println(" cm");

            client.println("CLIENT_READY"); // Inform server
            clientState = IDLE_WAITING_FOR_START;
        }
        else
        {
            Serial.println("ESP2: Verbindung zum Server fehlgeschlagen!");
            clientState = WAITING_FOR_CONNECTION;
        }
    }
}

bool establishInitialReferenceDistanceClient()
{
    Serial.println("ESP2: Messe initiale Referenzdistanz (Sensor 2)...");
    float totalDist = 0;
    int validSamples = 0;
    for (int i = 0; i < 10; i++)
    { // Take 10 samples
        float dist = measureDistanceClient(trigPin2, echoPin2);
        if (dist > 0 && dist < 400)
        { // Consider readings between 0 and 400cm valid
            totalDist += dist;
            validSamples++;
        }
        delay(100);
    }

    if (validSamples > 0)
    {
        referenceDistance2 = totalDist / validSamples;
        Serial.print("ESP2: Initiale Referenzdistanz S2: ");
        Serial.print(referenceDistance2);
        Serial.println(" cm");
        return true;
    }
    else
    {
        referenceDistance2 = -1.0f; // Indicate failure
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
    long duration = pulseIn(echoPin, HIGH, 25000); // Timeout 25ms
    if (duration == 0)
    {
        return -1.0f;
    }
    return (duration * SOUND_SPEED / 2.0f);
}

void loop()
{
    if (clientState == WAITING_FOR_CONNECTION || !client.connected())
    {
        connectToWiFiAndServer(); // Attempt to connect/reconnect
        delay(1000);              // Wait a bit before retrying connection
        return;
    }

    // Listen for messages from server (ESP1)
    if (client.available())
    {
        String serverData = client.readStringUntil('\n');
        serverData.trim();
        Serial.print("ESP2: Daten vom Server empfangen: '");
        Serial.print(serverData);
        Serial.println("'");

        if (serverData.equals("START_TIMER") && clientState == IDLE_WAITING_FOR_START)
        {
            Serial.println("ESP2: 'START_TIMER' Befehl empfangen. Starte Zeitmessung.");
            timingStartTime = millis();
            clientState = TIMING_IN_PROGRESS;
        }
        else
        {
            // Potentially handle other commands from server if any
        }
    }

    if (clientState == TIMING_IN_PROGRESS)
    {
        currentDistance2 = measureDistanceClient(trigPin2, echoPin2);
        // Serial.print("ESP2 Distanz: "); Serial.print(currentDistance2); Serial.println(" cm");

        // Toleranz: Distanz muss sich halbieren
        if (currentDistance2 > 0 && currentDistance2 <= triggerThreshold2)
        {
            unsigned long currentTime = millis();
            unsigned long elapsedTime = currentTime - timingStartTime;
            Serial.print("ESP2: Objekt an zweiter Lichtschranke erkannt! Zeit: ");
            Serial.print(elapsedTime);
            Serial.println(" ms");

            if (client.connected())
            {
                String message = "STOP_TIMER:" + String(elapsedTime);
                client.println(message); // Send stop signal and time
                Serial.print("ESP2: '");
                Serial.print(message);
                Serial.println("' an Server gesendet.");
            }
            else
            {
                Serial.println("ESP2: Client nicht verbunden. STOP_TIMER nicht gesendet.");
            }
            clientState = IDLE_WAITING_FOR_START; // Reset for next cycle
            // Here you would update the LCD display with elapsedTime
            // lcd.clear();
            // lcd.setCursor(0,0);
            // lcd.print("Zeit: ");
            // lcd.print(elapsedTime);
            // lcd.print("ms");
        }
    }

    delay(50); // Loop delay
}
