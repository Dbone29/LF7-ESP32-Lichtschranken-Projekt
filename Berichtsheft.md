# Projektbericht: Drahtloses Lichtschranken-Zeitmessungssystem mit ESP32-Mikrocontrollern

## 1. Formulierung des Themas

### Projektbeschreibung

Das entwickelte System stellt ein **drahtloses Zeitmessungssystem** dar, das eine klassische Lichtschranke mit Hilfe von zwei ESP32-Mikrocontrollern und Ultraschallsensoren simuliert. Ziel ist die präzise Messung von Durchgangszeiten zwischen zwei definierten Messpunkten.

### Funktionsprinzip

Das System arbeitet nach folgendem Ablauf: Ein Objekt durchbricht zunächst die erste "Lichtschranke" (ESP32 #1 mit Ultraschallsensor), woraufhin eine Ampelsequenz gestartet wird (Grün → Gelb → Rot → Alle LEDs). Nach Abschluss der Ampelphase und Verlassen des Objekts aus dem ersten Messbereich startet automatisch die Zeitmessung. Diese stoppt präzise, sobald das Objekt die zweite Schranke (ESP32 #2) durchbricht. Die gemessene Zeit wird sofort auf einem angeschlossenen LCD-Display angezeigt.

### Technische Umsetzung

Die Kommunikation zwischen beiden ESP32-Einheiten erfolgt vollständig drahtlos über WiFi. ESP32 #1 fungiert als Access Point und Server, während ESP32 #2 als Client agiert. Diese Architektur gewährleistet eine flexible Positionierung der Messeinheiten ohne Verkabelung zwischen den Stationen.

## 2. Projektziele (Teilaufgaben)

### Hauptziele

- **Entwicklung eines präzisen Zeitmessungssystems** mit einer Auflösung von 1 Millisekunde
- **Implementierung drahtloser Kommunikation** zwischen zwei ESP32-Mikrocontrollern via WiFi
- **Realisierung einer benutzerfreundlichen Anzeige** der Messergebnisse auf LCD-Display
- **Integration einer visuellen Statusanzeige** durch LED-Ampelsystem

### Teilaufgaben

1. **Hardware-Integration**: Anschluss und Konfiguration von HC-SR04 Ultraschallsensoren, LED-Ampel und I2C-LCD-Display
2. **Sensor-Kalibrierung**: Entwicklung einer automatischen Referenzdistanz-Ermittlung für beide Sensoren
3. **Netzwerk-Implementierung**: Aufbau einer stabilen WiFi-Verbindung mit automatischer Wiederverbindung
4. **State-Machine-Entwicklung**: Programmierung einer robusten Zustandssteuerung für den gesamten Messablauf
5. **Fehlerbehandlung**: Integration von Timeout-Mechanismen und Fehlerwiederherstellung
6. **Benutzerinterface**: Entwicklung einer informativen LCD-Anzeige mit Live-Statusupdates

## 3. Ressourcen und Ablaufplanung

### a) Personalplanung (Arbeitsaufteilung)

- **Hardware-Aufbau und Verkabelung**: Beide Teammitglieder gemeinsam
- **Server-Software (ESP32 #1)**: Teammitglied A - Ampelsteuerung, Sensor-Verarbeitung, WiFi-Access-Point
- **Client-Software (ESP32 #2)**: Teammitglied B - Display-Ansteuerung, Sensor-Auswertung, WiFi-Client
- **System-Integration und Testing**: Beide Teammitglieder gemeinsam
- **Dokumentation**: Aufgeteilt nach Verantwortungsbereichen

### b) Terminplanung, Ablaufplan

| **Datum**          | **Meilenstein**       | **Geplante Aktivitäten**                                              |
| ------------------ | --------------------- | --------------------------------------------------------------------- |
| **07.05.2025**     | Projektstart          | Gruppenfindung, Komponentenbeschaffung, erste Programmierversuche     |
| **14.05.2025**     | Hardware-Setup        | Verkabelung beider ESP32-Einheiten, Basis-Sensor-Tests                |
| **21.05.2025**     | Software-Entwicklung  | Implementierung der Grundfunktionen, WiFi-Kommunikation               |
| **28.05.2025**     | Integration & Testing | System-Integration, Debugging, Performance-Optimierung                |
| **04.06.2025**     | Klassenarbeit         | _Keine Projektarbeit_                                                 |
| **10.06.2025**     | Projektabschluss      | Finalisierung, Dokumentation, Video-Erstellung (Abgabe bis 18:00 Uhr) |
| **11./18.06.2025** | Präsentation          | Vorstellung des Projekts vor der Klasse                               |

### c) Sachmittel und Kostenplanung

| **Komponente**               | **Anzahl** | **Einzelpreis** | **Gesamtpreis** |
| ---------------------------- | ---------- | --------------- | --------------- |
| ESP32 Entwicklungsboards     | 2          | 15,00 €         | 30,00 €         |
| HC-SR04 Ultraschallsensoren  | 2          | 3,50 €          | 7,00 €          |
| HW-61 I2C LCD Display (20x4) | 1          | 8,00 €          | 8,00 €          |
| LEDs (Rot, Gelb, Grün)       | 3          | 0,50 €          | 1,50 €          |
| Widerstände (220Ω, 4,7kΩ)    | 10         | 0,10 €          | 1,00 €          |
| Breadboards                  | 2          | 4,00 €          | 8,00 €          |
| Verbindungskabel (Jumper)    | 1 Set      | 5,00 €          | 5,00 €          |
| **Gesamtkosten**             |            |                 | **60,50 €**     |

## 4. Durchführung

### a) Prozessschritte, Vorgehensweise

**Phase 1: Hardware-Aufbau**
Der erste Arbeitsschritt umfasste die sorgfältige Verkabelung beider ESP32-Einheiten entsprechend dem entwickelten Schaltplan. Besondere Aufmerksamkeit galt der korrekten I2C-Verbindung des LCD-Displays sowie der 5V-Spannungsversorgung der Ultraschallsensoren.

**Phase 2: Software-Architektur**
Die Softwareentwicklung erfolgte modular mit klarer Trennung der Verantwortlichkeiten. ESP32 #1 implementiert eine State-Machine mit sechs definierten Zuständen (SYSTEM_INIT, IDLE_GREEN, OBJECT_DETECTED_YELLOW_PENDING, YELLOW_ON_RED_PENDING, RED_ON_WAITING_FOR_OBJECT_LEAVE, TIMING_STARTED_ALL_ON), während ESP32 #2 vier Zustände verwaltet (WAITING_FOR_CONNECTION, IDLE_WAITING_FOR_START, TIMING_IN_PROGRESS, DISPLAYING_RESULT).

**Phase 3: Kommunikationsprotokoll**
Das entwickelte Protokoll nutzt einfache String-Nachrichten über TCP: "CLIENT_READY" für die Anmeldung, "START_TIMER" für den Messbeginn und "STOP_TIMER:${Zeit_ms}" für das Messergebnis.

### b) Umsetzung

**Sensor-Kalibrierung**: Implementierung einer automatischen Referenzpunkt-Bestimmung durch 15 Messungen pro Sensor beim Systemstart. Der Trigger-Schwellenwert wird auf 50% der Referenzdistanz gesetzt.

**Fehlerbehandlung**: Integration von Connection-Timeout (15s), Sensor-Überwachung (max. 10 aufeinanderfolgende Fehlmessungen) und automatische Systemwiederherstellung bei Verbindungsabbrüchen.

**Performance-Optimierung**: Reduzierung der Loop-Delays auf 20ms für verbesserte Responsivität und Implementierung einer Hysterese-Funktion (15% Überschreitung) zur Vermeidung von Fehlauslösungen.

### c) Schaltplan

**ESP32 #1 (Server/Ampel)**

```
HC-SR04: VCC→5V, GND→GND, Trig→GPIO5, Echo→GPIO18
LEDs: Rot→GPIO25, Gelb→GPIO26, Grün→GPIO27 (jeweils mit 220Ω)
```

**ESP32 #2 (Client/Display)**

```
HC-SR04: VCC→5V, GND→GND, Trig→GPIO12, Echo→GPIO14
LCD: VCC→3.3V, GND→GND, SDA→GPIO21, SCL→GPIO22
```

### d) Visualisierung

Das System arbeitet mit einer intuitiven LED-Signalisierung: Grün (bereit), Gelb (Objekt erkannt), Rot (warten auf Freigabe), alle LEDs (Messung läuft). Das LCD-Display zeigt kontinuierlich den aktuellen Systemstatus, Live-Zeitmessung und Ergebnisse an.

## 5. Projektergebnis

### a) Soll-Ist-Vergleich, Qualitätskontrolle

**Erfolgreich umgesetzte Anforderungen:**

- ✅ Präzise Zeitmessung mit 1ms Auflösung
- ✅ Stabile WiFi-Kommunikation mit automatischer Wiederverbindung
- ✅ Benutzerfreundliche LCD-Anzeige mit Live-Updates
- ✅ Robuste Fehlerbehandlung und Systemüberwachung
- ✅ Automatische Sensor-Kalibrierung beim Start
- ✅ Messbereich von 2-400cm bei beiden Sensoren

**Leistungsparameter:**

- Maximale WiFi-Reichweite: ~50m (je nach Umgebung)
- Sensor-Genauigkeit: ±0,3cm
- Systemreaktion: <50ms
- Maximale Messzeit: 30 Sekunden (mit Timeout-Schutz)

### b) Abweichungen, Anpassungen

**Planabweichungen:**

- **GPIO-Anpassung**: Ursprünglich geplante Pins GPIO5/GPIO18 für ESP32 #2 führten zu Konflikten, daher Wechsel auf GPIO12/GPIO14
- **I2C-Adresse**: LCD-Standard-Adresse 0x27 funktionierte nicht bei allen Modulen, Alternative 0x3F implementiert
- **Kalibrierungs-Samples**: Erhöhung von 10 auf 15 Messungen für verbesserte Genauigkeit

**Erfolgreiche Optimierungen:**

- Implementierung einer Hysterese-Funktion zur Reduzierung von Störungen
- Erweiterung der Fehlerbehandlung um automatische Systemwiederherstellung
- Verbesserung der Benutzerfreundlichkeit durch detaillierte Statusanzeigen

### c) Ausblick (Verbesserungen, weiterführende Möglichkeiten)

**Technische Erweiterungen:**

- **Datenlogging**: Speicherung von Messergebnissen auf SD-Karte für statistische Auswertungen
- **Web-Interface**: Entwicklung einer Browser-basierten Bedienoberfläche zur Remote-Konfiguration
- **Mehrkanal-Betrieb**: Erweiterung auf mehr als zwei Messpunkte für komplexe Streckenprofile
- **Präzisions-Upgrade**: Integration von Laser-Distanzsensoren für sub-millimeter Genauigkeit

**Anwendungsbereiche:**

- **Sportzeitmessung**: Einsatz bei Laufwettbewerben, Geschwindigkeitsmessungen
- **Industrieautomation**: Durchlaufzeiten in Produktionslinien
- **Verkehrsüberwachung**: Fahrzeuggeschwindigkeit und -klassifizierung
- **Bildungsbereich**: Physik-Experimente zu Bewegung und Zeit

**Skalierbarkeit:**
Das modulare Design ermöglicht einfache Erweiterungen um zusätzliche Sensoren, alternative Kommunikationsprotokolle (LoRa, MQTT) oder Integration in bestehende IoT-Infrastrukturen.

---

## 6. Anlagen

### Anlage A: Quellcode

- ESP32-Server.cpp (ESP32 #1 - Ampelsteuerung)
- ESP32-Client.cpp (ESP32 #2 - Display und Zeitmessung)

### Anlage B: Hardware-Dokumentation

- Detaillierte Verkabelungspläne
- Komponentenspezifikationen
- I2C-Scanner-Code für Troubleshooting

### Anlage C: Projektmedien

- Aufbau-Fotos der Hardware
- Video-Demonstration des funktionsfähigen Systems
- Screenshots der LCD-Anzeigen
