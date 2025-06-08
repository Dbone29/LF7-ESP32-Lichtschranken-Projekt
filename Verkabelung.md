# Detaillierte Verkabelungsanleitung

## 🔌 ESP32 #1 (Server mit Ampel)

### Ultraschallsensor HC-SR04
| Sensor-Pin | ESP32-Pin | Farbe (Empfehlung) | Bemerkung |
|------------|-----------|--------------------|-----------| 
| VCC | 5V | Rot | Sensor benötigt 5V |
| GND | GND | Schwarz | Gemeinsame Masse |
| Trig | GPIO 5 | Gelb | Trigger-Signal (Output) |
| Echo | GPIO 18 | Grün | Echo-Empfang (Input) |

### Ampel-LEDs
| LED | Anode (+) | Kathode (-) | Widerstand | Bemerkung |
|-----|-----------|-------------|------------|-----------|
| Rot | GPIO 25 | GND | 220Ω | In Reihe mit LED |
| Gelb | GPIO 26 | GND | 220Ω | In Reihe mit LED |
| Grün | GPIO 27 | GND | 220Ω | In Reihe mit LED |

## 🔌 ESP32 #2 (Client mit Display)

### Ultraschallsensor HC-SR04
| Sensor-Pin | ESP32-Pin | Farbe (Empfehlung) | Bemerkung |
|------------|-----------|--------------------|-----------| 
| VCC | 5V | Rot | Sensor benötigt 5V |
| GND | GND | Schwarz | Gemeinsame Masse |
| Trig | GPIO 12 | Gelb | Boot-sicherer Pin |
| Echo | GPIO 14 | Grün | Interrupt-fähig |

### I2C LCD Display (20x4)
| Display-Pin | ESP32-Pin | Farbe (Empfehlung) | Bemerkung |
|-------------|-----------|--------------------|-----------| 
| VCC | 5V* | Rot | *oder 3.3V je nach Modul |
| GND | GND | Schwarz | Gemeinsame Masse |
| SDA | GPIO 21 | Blau | I2C Data (Standard) |
| SCL | GPIO 22 | Weiß | I2C Clock (Standard) |

## ⚡ Stromversorgung

### Spannungsebenen
- **ESP32**: Arbeitet mit 3.3V Logik
- **HC-SR04**: Benötigt 5V für volle Reichweite
- **LCD Display**: Je nach Modell 3.3V oder 5V
- **LEDs**: 3.3V mit Vorwiderständen

### Stromaufnahme (typisch)
| Komponente | Strom | Bemerkung |
|------------|-------|-----------|
| ESP32 | 80-150mA | WiFi aktiv |
| HC-SR04 | 15mA | Pro Sensor |
| LCD + Backlight | 20-60mA | Je nach Helligkeit |
| LED | 20mA | Pro LED |
| **Gesamt** | ~250mA | Pro ESP32-Einheit |

## 🔧 Aufbau-Tipps

### Breadboard-Layout
1. **Stromschienen**: Rot = 5V/3.3V, Blau/Schwarz = GND
2. **Komponenten-Platzierung**: ESP32 mittig für kurze Kabelwege
3. **LED-Widerstände**: Direkt an LED-Anoden löten
4. **Jumper-Kabel**: Verschiedene Farben für bessere Übersicht

### Mechanische Montage
- **Sensor-Höhe**: 50-100cm über Boden optimal
- **Ausrichtung**: Exakt horizontal und parallel
- **Abstand**: Mindestens 1m zwischen den Sensoren
- **Vibrationsdämpfung**: Schaumstoff unter Sensoren

### Häufige Fehlerquellen
| Problem | Ursache | Lösung |
|---------|---------|---------|
| ESP32 startet nicht | GPIO 12 beim Boot HIGH | Sensor kurz abziehen |
| Display flackert | Schlechte Stromversorgung | Kondensator 100µF parallel |
| Sensor ungenau | Reflexionen | Umgebung mit Stoff abdecken |
| WiFi instabil | Zu schwaches Netzteil | Mindestens 1A verwenden |

## 📐 Schaltplan-Hinweise

### Wichtige GPIO-Einschränkungen
- **GPIO 0, 2, 15**: Boot-Pins (nicht verwenden)
- **GPIO 6-11**: Intern für Flash (nicht verfügbar)
- **GPIO 34-39**: Nur Input (keine Outputs)
- **GPIO 12**: Boot-Pin (LOW für normale Boot)

### I2C-Besonderheiten
- **Clock-Stretching**: Manche Displays benötigen es
- **Bus-Geschwindigkeit**: 50kHz für 5V-Kompatibilität
- **Pull-Ups**: 4.7kΩ optimal für 3.3V/5V Mix

## ✅ Checkliste vor Inbetriebnahme

- [ ] Alle Masse-Verbindungen (GND) verbunden?
- [ ] 5V und 3.3V nicht vertauscht?
- [ ] LED-Widerstände eingebaut?
- [ ] I2C-Adressen-Jumper am Display gesetzt?
- [ ] Sensor-Ausrichtung geprüft?
- [ ] WiFi-Credentials in beiden Codes gleich?
- [ ] Serial Monitor auf 115200 Baud?