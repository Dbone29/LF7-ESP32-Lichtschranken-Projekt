# ESP32 Lichtschranken-Zeitmessung

Präzises drahtloses Zeitmessungssystem mit zwei ESP32-Mikrocontrollern für Sport, Automation und Bildung.

## 🎯 Projektübersicht

Dieses System implementiert eine kontaktlose Zeitmessung mit Ultraschallsensoren, die als "Lichtschranken" fungieren. Die Kommunikation erfolgt über ein dediziertes WiFi-Netzwerk für maximale Zuverlässigkeit.

### Anwendungsbereiche
- **Sport**: Zeitmessung bei Sprint-Übungen oder Parcours
- **Bildung**: Physik-Experimente zur Geschwindigkeitsmessung
- **Automation**: Überwachung von Durchlaufzeiten in Produktionslinien
- **Verkehr**: Geschwindigkeitsmessung von Fahrzeugen

### Funktionsweise
1. **Bereitschaft**: Grüne LED signalisiert Messbereitschaft
2. **Objekterkennung**: Erste Schranke erkennt Objekt → Ampelsequenz startet
3. **Ampelsequenz**: Grün erlischt → 0.5s Pause → Gelb → 2s später Rot
4. **Zeitmessung**: Beginnt wenn Objekt die erste Schranke verlässt (alle LEDs an)
5. **Stopp**: Zweite Schranke erkennt Objekt → Zeit wird gestoppt und angezeigt

## 🔧 Hardware-Komponenten

### ESP32 #1 (Server/Ampelsteuerung)
- **ESP32 DevKit** (z.B. ESP32-WROOM-32)
- **HC-SR04 Ultraschallsensor**
- **3x LED** (Rot, Gelb, Grün)
- **3x 220Ω Widerstände** (LED-Vorwiderstände)
- **Breadboard & Jumper-Kabel**

### ESP32 #2 (Client/Zeitanzeige)
- **ESP32 DevKit** (z.B. ESP32-WROOM-32)
- **HC-SR04 Ultraschallsensor**
- **20x4 I2C LCD Display** (5V-kompatibel)
- **Breadboard & Jumper-Kabel**
- **Optional**: Pull-up Widerstände (4.7kΩ) für I2C

## 📡 Systemarchitektur

```
ESP32 #1 (Server)                        ESP32 #2 (Client)
┌─────────────────────┐                ┌─────────────────────┐
│ • Ultraschallsensor │                │ • Ultraschallsensor │
│ • Ampel-LEDs        │     WiFi       │ • LCD-Display       │
│ • Access Point      │ ◄────────────► │ • WiFi Station      │
│ • State Machine     │  192.168.4.x   │ • Zeitmessung       │
│ • Datenlogging      │                │ • Heartbeat-Monitor │
└─────────────────────┘                └─────────────────────┘

Kommunikationsprotokoll:
• START_TIMER: Server → Client (Zeitmessung starten)
• STOP_TIMER:xxxxx: Client → Server (Zeit in ms)
• HEARTBEAT/ACK: Verbindungsüberwachung (5s Intervall)
• CLIENT_READY: Client meldet Bereitschaft
```

## 🔌 Pin-Belegung

### ESP32 #1 (Server)

| Komponente | Pin | ESP32 GPIO | Hinweis |
|------------|-----|------------|---------|
| **HC-SR04** | | | |
| VCC | → | 5V | Sensor benötigt 5V |
| GND | → | GND | |
| Trig | → | GPIO 5 | Trigger-Signal |
| Echo | → | GPIO 18 | Echo-Empfang |
| **LEDs** | | | |
| Rot (Anode) | → | GPIO 25 | über 220Ω Widerstand |
| Gelb (Anode) | → | GPIO 26 | über 220Ω Widerstand |
| Grün (Anode) | → | GPIO 27 | über 220Ω Widerstand |
| Alle Kathoden | → | GND | Gemeinsame Masse |

### ESP32 #2 (Client)

| Komponente | Pin | ESP32 GPIO | Hinweis |
|------------|-----|------------|---------|
| **HC-SR04** | | | |
| VCC | → | 5V | Sensor benötigt 5V |
| GND | → | GND | |
| Trig | → | GPIO 12 | Andere Pins als Server |
| Echo | → | GPIO 14 | zur Vermeidung von Konflikten |
| **I2C LCD** | | | |
| VCC | → | 5V | Display benötigt 5V! |
| GND | → | GND | |
| SDA | → | GPIO 21 | I2C Daten |
| SCL | → | GPIO 22 | I2C Clock |

⚠️ **Wichtig**: Das LCD-Display benötigt 5V Versorgungsspannung, funktioniert aber mit 3.3V I2C-Signalen des ESP32.

## 📚 Software-Voraussetzungen

### Arduino IDE Einrichtung

1. **ESP32 Board Package installieren**:
   - Öffnen Sie: `Datei → Voreinstellungen`
   - Fügen Sie diese URL bei "Zusätzliche Boardverwalter-URLs" ein:
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Öffnen Sie: `Werkzeuge → Board → Boardverwalter`
   - Suchen Sie "ESP32" und installieren Sie "ESP32 by Espressif Systems"

2. **Benötigte Bibliotheken**:
   - **LiquidCrystal_I2C** by Frank de Brabander (über Library Manager)
   - Alle anderen Bibliotheken sind im ESP32-Paket enthalten

### Board-Einstellungen
```
Board: "ESP32 Wrover Module" oder "ESP32 Dev Module"
Upload Speed: 115200
Flash Frequency: 80MHz
Flash Mode: QIO
Partition Scheme: Default 4MB with spiffs
```

## ⚙️ Installation & Konfiguration

### 1. Hardware-Aufbau
1. Bauen Sie die Schaltungen gemäß Pin-Belegung auf
2. Überprüfen Sie alle Verbindungen (besonders 5V/3.3V)
3. Testen Sie die LEDs mit einem einfachen Blink-Sketch

### 2. Software-Upload

**ESP32 #1 (Server)**:
1. Öffnen Sie `ESP32-Server.cpp` in Arduino IDE
2. Wählen Sie den richtigen COM-Port
3. Upload-Taste drücken
4. Serial Monitor öffnen (115200 Baud)

**ESP32 #2 (Client)**:
1. Öffnen Sie `ESP32-Client.cpp` in Arduino IDE
2. Wählen Sie den richtigen COM-Port
3. Upload-Taste drücken
4. Serial Monitor öffnen (115200 Baud)

### 3. Anpassbare Parameter

```cpp
// WiFi-Konfiguration (in beiden Dateien)
const char *ssid_ap = "MeinESP32AP";      // Netzwerkname ändern
const char *password_ap = "meinPasswort123"; // Sicheres Passwort wählen!

// Timing-Parameter (Server)
const unsigned long YELLOW_PENDING_DELAY_MS = 500;     // Verzögerung vor Gelb
const unsigned long RED_PENDING_DELAY_AFTER_YELLOW_MS = 2000; // Gelb-Dauer

// Sensor-Empfindlichkeit
const float HYSTERESIS_FACTOR = 1.15f;     // 15% Hysterese gegen Prellen
const int REFERENCE_SAMPLES = 15;          // Anzahl Kalibrierungsmessungen
```

## 🚀 Betriebsanleitung

### Systemstart

1. **ESP32 #1 (Server) einschalten**
   - LED-Test: Alle LEDs leuchten kurz auf
   - Access Point wird erstellt
   - Sensor-Kalibrierung (Rot+Gelb leuchten)
   - Grüne LED = System bereit

2. **ESP32 #2 (Client) einschalten**
   - Display zeigt "ESP2 Client - Initialisierung..."
   - Automatische WiFi-Verbindung
   - Sensor-Kalibrierung mit Fortschrittsanzeige
   - Display zeigt "Bereit!" mit Referenzwerten

### Messablauf

1. **Vorbereitung**
   - Stellen Sie sicher, dass beide Sensoren freie Sicht haben
   - Objekt sollte größer als 10cm² sein für zuverlässige Erkennung
   - Idealer Abstand: 20-100cm von den Sensoren

2. **Messung starten**
   - Objekt langsam vor Sensor 1 bewegen
   - Grüne LED erlischt → Objekt wurde erkannt
   - Ampelsequenz läuft automatisch ab

3. **Zeitmessung**
   - Bei Rot: Objekt von Sensor 1 entfernen
   - Alle LEDs leuchten = Zeitmessung läuft
   - Display zeigt Live-Zeit in Sekunden
   - Objekt zu Sensor 2 bewegen

4. **Ergebnis**
   - Zeit wird automatisch gestoppt
   - Display zeigt Ergebnis in Sekunden und Millisekunden
   - Nach 5 Sekunden: System bereit für nächste Messung

## 📊 Technische Daten

### Leistungsdaten
| Parameter | Wert | Bemerkung |
|-----------|------|------------|
| **Messbereich** | 2 - 400 cm | HC-SR04 Spezifikation |
| **Distanz-Genauigkeit** | ± 0.3 cm | Bei stabilen Bedingungen |
| **Zeitauflösung** | 1 ms | Software-limitiert |
| **Abtastrate** | 50 Hz | 20ms Loop-Delay |
| **Max. Messzeit** | 30 Sekunden | Timeout-Schutz |
| **Min. Objektgröße** | ~10 cm² | Für zuverlässige Erkennung |

### System-Features
| Feature | Beschreibung |
|---------|-------------|
| **Median-Filter** | 5 Messungen pro Datenpunkt |
| **Hysterese** | 15% zur Vermeidung von Fehlauslösungen |
| **Heartbeat** | 5s Intervall für Verbindungsüberwachung |
| **Auto-Recovery** | Automatische Wiederherstellung nach Fehler |
| **Datenlogging** | CSV-Format auf SPIFFS (100KB Rotation) |
| **Statistik** | Min/Max/Durchschnitt in Echtzeit |

## 🔍 Fehlerbehebung

### Häufige Probleme und Lösungen

#### Display zeigt nichts an
- **Ursache**: Falsche I2C-Adresse oder Verkabelung
- **Lösung**:
  1. Überprüfen Sie die Verkabelung (besonders VCC → 5V!)
  2. Der Code scannt automatisch gängige Adressen
  3. Prüfen Sie Serial Monitor für gefundene I2C-Geräte
  4. Bei Bedarf I2C-Adresse im Code anpassen

#### "Sensor ausgefallen" Fehler
- **Ursache**: Sensor-Verbindung oder Hindernisse
- **Lösung**:
  1. Verkabelung prüfen (5V und GND)
  2. Freie Sicht des Sensors sicherstellen
  3. Mindestabstand 20cm zu Wänden einhalten
  4. System neustarten für neue Kalibrierung

#### Verbindung bricht ständig ab
- **Ursache**: Schwaches WiFi-Signal oder Interferenz
- **Lösung**:
  1. ESP32s näher zusammenstellen (<10m)
  2. WiFi-Kanal in der Umgebung prüfen
  3. Metallische Hindernisse entfernen
  4. Externes Antenne verwenden (falls möglich)

#### Zeitmessung startet nicht
- **Ursache**: Timing-Flag blockiert
- **Lösung**:
  1. Warten bis vorherige Messung komplett abgeschlossen
  2. Auf "Bereit!" im Display warten
  3. Bei Blockierung: Beide ESP32 neustarten

#### Fehlerhafte Zeitmessungen
- **Ursache**: Objekt zu klein oder zu schnell
- **Lösung**:
  1. Größeres Objekt verwenden (min. 10cm²)
  2. Langsamer bewegen bei Sensor-Durchgang
  3. Sensoren genau ausrichten
  4. Umgebungslicht/Reflexionen minimieren

### Debug-Modus aktivieren

Für detaillierte Fehleranalyse nutzen Sie den Serial Monitor:

1. Öffnen Sie Serial Monitor (115200 Baud)
2. Beobachten Sie die Status-Meldungen
3. Typische Meldungen:
   - `ESP1: State=1` → System im IDLE_GREEN Zustand
   - `ESP1: Referenzdistanz: XX.Xcm` → Kalibrierungswert
   - `ESP1: WARNUNG - Zeitmessung läuft noch!` → Vorherige Messung nicht abgeschlossen

## 🎛️ LED-Signale & Status

### Normale Betriebszustände

| LED-Status | Bedeutung | Aktion |
|------------|-----------|--------|
| 🟢 Grün | System bereit | Objekt vor Sensor 1 platzieren |
| ⚫ Alle aus | Objekt erkannt | 0.5s Pause vor Gelb |
| 🟡 Gelb | Vorwarnung | 2s bis Rot |
| 🔴 Rot | Startposition | Objekt jetzt entfernen! |
| 🔴🟡🟢 Alle | Zeitmessung aktiv | Zu Sensor 2 bewegen |
| 🟡 Gelb (solo) | Ergebnis empfangen | 2s Cooldown |

### Fehlerzustände

| LED-Status | Bedeutung | Lösung |
|------------|-----------|--------|  
| 🔴 Rot blinkend (5x) | Systemfehler | Serial Monitor prüfen |
| 🔴🟡 Rot+Gelb | Kalibrierung | Messbereich freihalten |
| 🔴🟡🟢 Alle (Startup) | LED-Test | Normal beim Start |

## 📈 Erweiterte Funktionen

### Implementierte Features

#### Robuste Sensorik
- **Median-Filter**: Eliminiert Ausreißer durch 5-fach Messung
- **Automatische Kalibrierung**: Kompensiert Umgebungsbedingungen
- **Hysterese (15%)**: Verhindert Prellen bei Grenzwerten
- **Gültigkeitsprüfung**: Erkennt fehlerhafte Messungen

#### Zuverlässige Kommunikation  
- **Heartbeat-Mechanismus**: Erkennt stille Verbindungsabbrüche
- **Auto-Reconnect**: Automatische Wiederverbindung
- **State-Synchronisation**: Server und Client bleiben synchron
- **Timeout-Protection**: Verhindert Systemblockaden

#### Datenanalyse
- **Live-Statistik**: Min/Max/Durchschnitt in Echtzeit
- **SPIFFS-Logging**: Persistente Speicherung im CSV-Format
- **Auto-Rotation**: Log-Dateien bei 100KB automatisch gelöscht
- **Zeitstempel**: Millisekunden-genaue Aufzeichnung

### Geplante Erweiterungen

1. **Web-Interface**: Statistiken über Browser abrufen
2. **Multi-Client**: Mehrere Zeitmess-Stationen parallel
3. **Bluetooth-Support**: Alternative Verbindungsmöglichkeit
4. **SD-Karten-Logger**: Erweiterte Datenspeicherung
5. **Geschwindigkeitsberechnung**: Bei bekannter Strecke

## 🏗️ Projektstruktur

```
LF7/
├── ESP32-Server.cpp      # Hauptcode Server (Ampel + Sensor 1)
├── ESP32-Client.cpp      # Hauptcode Client (Display + Sensor 2)
├── README.md            # Diese Dokumentation
├── Verkabelung.md       # Detaillierte Verkabelungsanleitung
├── Berichtsheft.md      # Projekt-Dokumentation
└── test/                # Test- und Beispielcode
    ├── AP.cpp           # Access Point Test
    ├── AmpelLed.cpp     # LED-Ampel Test
    ├── SchallSensor.cpp # Ultraschall-Sensor Test
    └── wifiClient.cpp   # WiFi-Verbindungstest
```

## 🔬 Messprinzip

### Ultraschall-Entfernungsmessung

1. **Trigger**: 10µs HIGH-Puls startet Messung
2. **Echo**: Sensor sendet 8x 40kHz Ultraschallpulse
3. **Laufzeit**: Zeit bis Echo zurückkommt wird gemessen
4. **Berechnung**: `Distanz = (Laufzeit × Schallgeschwindigkeit) / 2`
5. **Schallgeschwindigkeit**: 0.034 cm/µs bei 20°C

### Zeitmessung

1. **Referenzmessung**: Leerer Bereich wird kalibriert
2. **Schwellwert**: Trigger bei 50% der Referenzdistanz
3. **Start**: Wenn Objekt Sensor 1 verlässt
4. **Stop**: Wenn Objekt Sensor 2 erreicht
5. **Genauigkeit**: ±1ms durch 50Hz Abtastrate

## 📝 Lizenz

MIT License - Siehe LICENSE Datei für Details

## 🤝 Beiträge

Beiträge sind willkommen! Bitte erstellen Sie einen Pull Request mit:
- Klarer Beschreibung der Änderungen
- Test-Ergebnissen
- Aktualisierten Kommentaren

## 🔗 Weiterführende Links

- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [HC-SR04 Tutorial](https://randomnerdtutorials.com/esp32-hc-sr04-ultrasonic-arduino/)
- [I2C LCD Tutorial](https://randomnerdtutorials.com/esp32-esp8266-i2c-lcd-arduino-ide/)
- [Arduino ESP32 Forum](https://forum.arduino.cc/c/hardware/esp32/103)

---

**Ein Projekt für präzise, kontaktlose Zeitmessung mit Open-Source Hardware**  
*Entwickelt für Bildung, Sport und Maker-Projekte*