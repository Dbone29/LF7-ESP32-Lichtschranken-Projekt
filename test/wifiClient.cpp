#include <WiFi.h>
#include <WiFiClient.h>

// SSID und Passwort des SoftAP Netzwerks
const char *ssid = "MeinESP32AP";         // Muss mit der SSID im SoftAP Code übereinstimmen
const char *password = "meinPasswort123"; // Muss mit dem Passwort im SoftAP Code übereinstimmen

// IP-Adresse des SoftAP (Servers)
IPAddress serverIP(192, 168, 4, 1); // Muss mit der local_ip im SoftAP Code übereinstimmen
const uint16_t serverPort = 80;     // Muss mit dem Server Port im SoftAP Code übereinstimmen

// WiFi Client Objekt
WiFiClient client;

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.print("Verbinde mit WLAN ");
    Serial.println(ssid);

    // Mit dem SoftAP Netzwerk verbinden
    WiFi.begin(ssid, password);

    // Warten auf die Verbindung
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WLAN verbunden!");
    Serial.print("Eigene IP-Adresse: ");
    Serial.println(WiFi.localIP());

    // Verbindung zum Server herstellen
    Serial.print("Verbinde zum Server: ");
    Serial.print(serverIP);
    Serial.print(":");
    Serial.println(serverPort);

    if (!client.connect(serverIP, serverPort))
    {
        Serial.println("Verbindung zum Server fehlgeschlagen!");
        return; // Setup beenden, wenn keine Verbindung möglich ist
    }

    Serial.println("Verbunden mit dem Server!");

    // Daten an den Server senden
    client.println("Hallo vom ESP32 STA!");
    client.println("Hier sind einige Daten.");
    client.println(); // Senden Sie eine leere Zeile, um das Ende der Nachricht zu markieren (optional, abhängig vom Server-Protokoll)
}

void loop()
{
    // Prüfen, ob Daten vom Server verfügbar sind
    if (client.available())
    {
        String serverResponse = client.readStringUntil('\r'); // Beispiel: bis Carriage Return
        Serial.print("Antwort vom Server empfangen: ");
        Serial.println(serverResponse);
    }

    // Optional: Senden Sie periodisch Daten oder warten Sie auf Benutzereingabe
    // Dieses Beispiel sendet nur einmal in Setup
}
