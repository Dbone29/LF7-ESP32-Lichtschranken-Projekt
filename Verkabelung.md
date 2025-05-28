# Hardware-Verkabelung für Lichtschranken-Projekt

## ESP32 #1 (Server/Ampel)
```
HC-SR04 Ultraschallsensor:
- VCC → 5V
- GND → GND  
- Trig → GPIO 5
- Echo → GPIO 18

Ampel LEDs:
- Rot → GPIO 25 (+ Vorwiderstand 220Ω)
- Gelb → GPIO 26 (+ Vorwiderstand 220Ω)  
- Grün → GPIO 27 (+ Vorwiderstand 220Ω)
- Alle GND → GND
```

## ESP32 #2 (Client/Display)
```
HC-SR04 Ultraschallsensor:
- VCC → 5V
- GND → GND
- Trig → GPIO 12
- Echo → GPIO 14

HW-61 I2C LCD Display:
- VCC → 3.3V (oder 5V)
- GND → GND
- SDA → GPIO 21 (Standard I2C SDA)
- SCL → GPIO 22 (Standard I2C SCL)
```

## Wichtige Hinweise:

### Pull-Up Widerstände
- I2C Bus benötigt 4.7kΩ Pull-Up Widerstände auf SDA/SCL
- Oft bereits auf LCD-Modul vorhanden

### Stromversorgung
- HC-SR04 benötigt 5V für optimale Reichweite
- ESP32 läuft mit 3.3V, toleriert aber 5V Input
- Separate 5V Versorgung empfohlen für Sensoren

### I2C Adresse LCD
- Standardadresse: 0x27 oder 0x3F
- Mit I2C Scanner testen falls unklar:
```cpp
// I2C Scanner Code
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

### Bibliotheken installieren
```
Benötigte Libraries:
- LiquidCrystal_I2C (by Frank de Brabander)
- WiFi (ESP32 Standard)
```

### Mechanischer Aufbau
- **Sensor-Abstand**: 30-200cm für zuverlässige Messung
- **Ausrichtung**: Sensoren parallel, gleiche Höhe
- **Schutz**: Sensoren vor direkter Sonneneinstrahlung schützen
- **Befestigung**: Stabile Montage verhindert Vibrationen

### Troubleshooting
1. **Display zeigt nichts**: I2C Adresse prüfen, Verkabelung kontrollieren
2. **Ungenaue Messungen**: Sensor-Position überprüfen, Hindernisse entfernen
3. **Verbindungsabbrüche**: WLAN-Reichweite prüfen, Störquellen eliminieren
4. **Falsche Triggerung**: Referenzdistanz neu kalibrieren

### Performance-Optimierung
- **Loop-Delays reduziert**: 20ms statt 50ms für bessere Responsivität
- **Mehr Kalibrierungs-Samples**: 15 statt 10 für genauere Referenz
- **Hysterese**: 15% Überschreitung für sauberes Triggering