# AQUASENSE ESP32 dashboard

## What is included

- `aquasense-esp32.ino`: ESP32 sensor firmware with Wi-Fi and WebSocket JSON broadcasts.
- `aquasense-dashboard.html`: browser dashboard that connects to `ws://<ESP32-IP>/ws`.

The firmware keeps the original two-point pH calibration (`A4.01`, `B6.86`) and LDR baseline command (`Z`) in ESP32 Preferences, with `R` clearing saved values. It retains thresholds of pH 6.5–8.5, temperature 5–35 °C, and LDR baseline change over 25%, plus the two 300 ms buzzer pulses separated by 150 ms when any alert is active. Readings are sent every 20 seconds.

The LDR is reported as a raw, light-related ADC reading. It is not a turbidity sensor. An approximate pH estimate is explicitly marked until both calibration points are saved. This prototype does not certify drinking-water safety.

## Required libraries

Install these through Arduino IDE Library Manager:

- OneWire by Paul Stoffregen
- DallasTemperature by Miles Burton
- WebSockets by Markus Sattler (provides `WebSocketsServer.h`)

`WiFi`, `Preferences`, and the ESP32 ADC functions are included with Espressif's Arduino-ESP32 board package.

## Arduino IDE setup

1. Install the Espressif ESP32 board package in Arduino IDE Boards Manager.
2. Select your ESP32 board and its USB port.
3. Open `aquasense-esp32.ino` and replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` with your 2.4 GHz Wi-Fi network credentials.
4. Install the libraries listed above, then compile and upload.
5. Open Serial Monitor at **115200 baud** with **Newline** line ending. Record the ESP32 IP shown after it joins Wi-Fi.

## Wiring (as in the original sketch)

- DS18B20 data/yellow → GPIO 4, with a 4.7 kΩ pull-up to 3.3 V.
- pH module Po → 10 kΩ → GPIO 34; GPIO 34 → 10 kΩ → GND (voltage divider).
- LDR module AO → GPIO 35.
- Active buzzer + → GPIO 26; buzzer − → GND.
- Ensure all modules and ESP32 share ground; do not exceed ESP32 input voltage limits.

## Network and dashboard

The ESP32 connects to the Wi-Fi network as a station and prints its assigned IP address. Open the HTML file in a browser on a device connected to the same local network, enter that IP, then click **Connect**. The browser opens `ws://<IP>/ws` on port 80. The ESP32 is not serving the HTML page; the HTML may be opened locally. Avoid guest Wi-Fi/client isolation, which can prevent devices from reaching each other. If the router changes the ESP32 IP, enter its new address and reconnect. The dashboard retries unexpected disconnects every five seconds; clicking Disconnect stops retries.

## Calibration commands

With the pH probe in pH 4.01 buffer, send `A4.01` in Serial Monitor. Rinse the probe, place it in pH 6.86 buffer, and send `B6.86`. Use the buffer values appropriate to your calibration solutions. Place the LDR in the intended clean-water/reference setup and send `Z` to store its raw baseline. Send `R` to erase both stored calibration points and the baseline.

## Test procedure (no hardware test is implied)

1. Compile/upload, confirm the ESP32 reports a Wi-Fi IP and the WebSocket connection appears in the dashboard.
2. Confirm live messages arrive about every 20 seconds and inspect the pH calibration label and LDR baseline availability.
3. Test calibration and persistence by entering `A4.01`, `B6.86`, and `Z`, then reset/restart the board and verify the saved state remains. Use `R` only when you intend to erase it.
4. To check alert behavior, safely present readings beyond configured thresholds or adjust test thresholds temporarily. Confirm alert text and buzzer pulses; restore the intended thresholds afterward.
5. Click Disconnect and verify the dashboard stays disconnected; click Connect to start again.

No hardware compilation, Wi-Fi connection, sensor calibration, or buzzer test was performed while preparing these files. Sensor readings depend on correct wiring, module behavior, calibration, and the environment. Use certified testing for drinking-water decisions.

## Dashboard indicator score

The dashboard calculates an equal-weight project score from 0 to 100: each of the three checks contributes one third. pH passes at 6.5–8.5; temperature passes at 5–35 °C; LDR passes when it stays within ±25% of the saved baseline, or when no baseline exists, when its raw reading is within 300–3800. A failed check changes the result to “needs attention”; an invalid sensor reading makes it incomplete. Until both pH calibration points are stored, an otherwise passing score is marked provisional because pH is approximate.

This is a transparent summary of the prototype’s configured checks, not a validated water-quality index or drinking-water safety grade. The LDR is not a calibrated turbidity instrument.
