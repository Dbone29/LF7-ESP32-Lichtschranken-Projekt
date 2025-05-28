# Lichtschranken-Zeitmessungssystem mit ESP32

Ein drahtloses Zeitmessungssystem basierend auf zwei ESP32-Mikrocontrollern und Ultraschallsensoren zur präzisen Messung von Durchgangszeiten zwischen zwei Messpunkten.

## 🎯 Projektübersicht

Dieses System simuliert eine Lichtschranke mit zwei Ultraschallsensoren und misst die Zeit, die ein Objekt benötigt, um von der ersten zur zweiten "Schranke" zu gelangen. Die Kommunikation zwischen beiden ESP32 erfolgt über WiFi.

### Funktionsweise
1. **Start**: Objekt durchbricht die erste Schranke (ESP32 #1)
2. **Ampelsequenz**: Grün → Gelb → Rot → Alle LEDs an
3. **Zeitmessung**: Startet automatisch nach der Ampelsequenz
4. **Stop**: Objekt durchbricht die zweite Schranke (ESP32 #2)
5. **Anzeige**: Gemessene Zeit wird auf dem Display angezeigt

## 🔧 Hardware-Komponenten

### ESP32 #1 (Server/Sender)
- **ESP32 Entwicklungsboard**
- **HC-SR04 Ultraschallsensor** (Sensor 1)
- **3 LEDs** (Rot, Gelb, Grün) + Vorwiderstände (220Ω)
- **Breadboard und Verbindungskabel**

### ESP32 #2 (Client/Empfänger)
- **ESP32 Entwicklungsboard**
- **HC-SR04 Ultraschallsensor** (Sensor 2)
- **HW-61 I2C LCD Display** (20x4 Zeichen)
- **Breadboard und Verbindungskabel**

## 📡 Systemarchitektur

```
ESP32 #1 (Server)           WiFi            ESP32 #2 (Client)
┌─────────────────┐       ◄─────►          ┌─────────────────┐
│ • HC-SR04       │                        │ • HC-SR04       │
│ • Ampel LEDs    │    192.168.4.x         │ • LCD Display   │
│ • Access Point  │                        │ • WiFi Client   │
└─────────────────┘                        └─────────────────┘
```

## 🔌 Verkabelung

### ESP32 #1 (Server)
```
HC-SR04 Ultraschallsensor:
├── VCC → 5V
├── GND → GND  
├── Trig → GPIO 5
└── Echo → GPIO 18

Ampel LEDs:
├── Rot → GPIO 25 (+ 220Ω Vorwiderstand)
├── Gelb → GPIO 26 (+ 220Ω Vorwiderstand)
├── Grün → GPIO 27 (+ 220Ω Vorwiderstand)
└── Alle GND → GND
```

### ESP32 #2 (Client)
```
HC-SR04 Ultraschallsensor:
├── VCC → 5V
├── GND → GND
├── Trig → GPIO 12
└── Echo → GPIO 14

HW-61 I2C LCD Display:
├── VCC → 3.3V (oder 5V)
├── GND → GND
├── SDA → GPIO 21
└── SCL → GPIO 22
```

## 📚 Benötigte Bibliotheken

Installieren Sie folgende Libraries über den Arduino IDE Library Manager:

```cpp
// Für ESP32 #2 (Client)
#include <WiFi.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>  // by Frank de Brabander

// Für ESP32 #1 (Server)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
```

## ⚙️ Installation & Setup

### 1. Arduino IDE Vorbereitung
```bash
# ESP32 Board Package installieren
# In Arduino IDE: File → Preferences → Additional Board Manager URLs:
# https://dl.espressif.com/dl/package_esp32_index.json

# Board auswählen: ESP32 Dev Module
# Upload Speed: 921600
# Flash Mode: QIO
```

### 2. Code Upload
1. **ESP32 #1**: Laden Sie `ESP32-Server.cpp` hoch
2. **ESP32 #2**: Laden Sie `ESP32-Client.cpp` hoch

### 3. WiFi-Konfiguration
```cpp
// In beiden Dateien anpassbar:
const char *ssid_ap = "MeinESP32AP";
const char *password_ap = "meinPasswort123";
IPAddress serverIP(192, 168, 4, 1);
```

## 🚀 Inbetriebnahme

### 1. Start-Sequenz
1. **ESP32 #1 einschalten** → Access Point wird gestartet
2. **ESP32 #2 einschalten** → Verbindet sich automatisch mit ESP32 #1
3. **Kalibrierung** → Beide Sensoren messen Referenzdistanz
4. **System bereit** → Grüne LED leuchtet, Display zeigt "Bereit!"

### 2. Messung durchführen
1. Objekt vor **Sensor 1** platzieren
2. **Ampelsequenz** startet automatisch
3. Nach Rot-Phase: **Objekt entfernen** → Zeitmessung beginnt
4. Objekt vor **Sensor 2** platzieren → **Zeitmessung stoppt**
5. **Ergebnis** wird 5 Sekunden auf Display angezeigt

## 📊 Technische Spezifikationen

| Parameter | Wert |
|-----------|------|
| **Messbereich** | 2 - 400 cm |
| **Messgenauigkeit** | ± 0.3 cm |
| **Zeitauflösung** | 1 ms |
| **WiFi-Reichweite** | ~10-50m (je nach Umgebung) |
| **Max. Messzeit** | 30 Sekunden |
| **Display** | 20x4 Zeichen LCD |
| **Kalibrierung** | 15 Messungen pro Sensor |

## 🔍 Troubleshooting

### Display zeigt nichts
```cpp
// I2C-Adresse prüfen (Standard: 0x27 oder 0x3F)
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Ggf. auf 0x3F ändern
```

### Verbindungsprobleme
- WiFi-Reichweite prüfen
- Serial Monitor (115200 baud) für Debug-Infos nutzen
- ESP32 #1 zuerst starten, dann ESP32 #2

### Ungenaue Messungen
- Sensoren parallel ausrichten
- Referenzdistanz neu kalibrieren (Neustart)
- Hindernisse im Messbereich entfernen

### I2C Scanner (bei Display-Problemen)
```cpp
#include <Wire.h>
void setup() {
  Wire.begin();
  Serial.begin(115200);
  for(byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if(Wire.endTransmission() == 0) {
      Serial.print("I2C device at 0x");
      Serial.println(i, HEX);
    }
  }
}
```

## 🎛️ System-Status LEDs

| LED-Kombination | Bedeutung |
|----------------|-----------|
| 🟢 | System bereit, warten auf Objekt |
| 🟡 | Objekt erkannt, Verzögerung aktiv |
| 🔴 | Warten auf Objektentfernung |
| 🔴🟡🟢 | Zeitmessung läuft |
| 🔴 (blinkend) | Systemfehler |

## 📈 Erweiterte Features

- **Automatische Kalibrierung** beim Start
- **Hysterese-Funktion** verhindert Fehlauslösungen
- **Timeout-Schutz** bei hängenden Messungen
- **Fehlerbehandlung** mit automatischer Wiederherstellung
- **Live-Zeitanzeige** während der Messung

## 📝 Lizenz & Mitwirken

Dieses Projekt steht unter MIT-Lizenz. Verbesserungen und Pull Requests sind willkommen!

## 🔗 Zusätzliche Ressourcen

- [ESP32 Dokumentation](https://docs.espressif.com/projects/esp32/)
- [HC-SR04 Datenblatt](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf)
- [Arduino IDE Setup](https://www.arduino.cc/en/software)

---

**Entwickelt für präzise Zeitmessungen in Sport, Automation und Bildung.**