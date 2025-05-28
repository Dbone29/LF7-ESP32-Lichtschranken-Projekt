#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

// SSID und Passwort für das SoftAP Netzwerk
const char *ssid = "MeinESP32AP";
const char *password = "meinPasswort123";

// Lokale IP Adresse, die der SoftAP haben wird
IPAddress local_ip(192, 168, 4, 1);
// Gateway Adresse
IPAddress gateway(192, 168, 4, 1);
// Subnetzmaske
IPAddress subnet(255, 255, 255, 0);

// TCP Server auf Port 80
WiFiServer server(80);

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.print("Konfiguriere SoftAP...");

    // SoftAP konfigurieren
    WiFi.softAPConfig(local_ip, gateway, subnet);

    // SoftAP starten
    WiFi.softAP(ssid, password);

    Serial.println(" SoftAP gestartet");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("IP-Adresse des SoftAP: ");
    Serial.println(WiFi.softAPIP());

    // TCP Server starten
    server.begin();
    Serial.println("TCP Server gestartet auf Port 80");
    Serial.println("Warte auf eingehende Verbindungen...");
}

void loop()
{
    // Nach neuen Clients suchen
    WiFiClient client = server.available();

    if (client)
    {
        Serial.println("Neuer Client verbunden");

        // Warten, bis der Client Daten sendet oder die Verbindung geschlossen wird
        while (client.connected())
        {
            if (client.available())
            {
                // Daten vom Client lesen (optional)
                String clientData = client.readStringUntil('\r'); // Beispiel: bis Carriage Return
                Serial.print("Daten vom Client empfangen: ");
                Serial.println(clientData);

                // Antwort an den Client senden
                client.println("Hallo, du bist mit dem ESP32 SoftAP verbunden!");
                client.println("Empfangen: " + clientData);
                break; // Nach dem Senden der Antwort die Schleife verlassen
            }
            delay(1); // Kleine Pause
        }

        // Verbindung schliessen
        client.stop();
        Serial.println("Client Verbindung geschlossen");
    }
}
