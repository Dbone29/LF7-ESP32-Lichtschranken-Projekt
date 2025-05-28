
# Lichtschranken Projekt

Schalsensor 2 % Tolleranz

Distanz muss sich halbieren, damit die "Lischtschranke" auslöst.
Wird Gelb nach einer halben Sekunde (10 Ticks)
2 Sekunden bis Ampel Grün

Wenn die Distanz wieder größer wird, leuchten alle Farben gleichzeitig und die Zeit geht los.
Zeit hört erst auf, wenn zweite "Lischtschranke" durchbrochen wird.


Für die Kommunikation zwischen den beiden ESP32 findet über wifi statt. Der erste ESP32 hat eine Ampel led und einen Ultraschall-Sensor.
Der zweite esp32 hat einen Ultraschall-Sensor und später ein LCD-Display, auf dem die Zeit gezeigt wird. (LCD Display fürs erste ignorieren)


