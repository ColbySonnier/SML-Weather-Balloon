# SML Weather Balloon Project

A hands-on telemetry system built for high-altitude weather balloon flights. The payload collects atmospheric, GPS, and IMU data via an ESP32 and transmits it over LoRa radio to a ground station running a real-time web dashboard.

Built as part of the CAPE (Cajun Advanced Picosatellite Experiment) program at UL Lafayette.

---

## System Overview
```
[ ESP32 Payload ] →(LoRa 915MHz)→ [ ESP32 Receiver ] →(Serial)→ [ Python Ground Station ] → [ Web Dashboard ]
```

**Payload sensors:**
- Temperature, humidity — Thermistor/ HTU21D-F
- Barometric pressure + altitude — BMP180
- GPS coordinates — NEO-6M
- Accelerometer + gyroscope (IMU) — GY-521

**Radio link:**
- RFM96 LoRa transceiver
- 915 MHz, 82mm antenna

---

## Ground Station

The ground station is a Python application that reads incoming JSON packets from the receiver over serial, parses them through a state machine, logs them to CSV, and broadcasts over WebSocket to a browser dashboard.

**Stack:**
- `pyserial` — serial ingestion
- `asyncio` + `websockets` — real-time broadcast
- `http.server` — serves the dashboard locally
- Vanilla JS + Chart.js — live frontend

**Dashboard panels:**
- Live temperature with scrolling chart
- Altitude, pressure, humidity
- GPS coordinates
- LoRa signal quality (RSSI, SNR, frequency error)
- Accelerometer and gyroscope bar displays
- Rolling packet log

---

## Data Logging

Every received packet is appended to `telemetry_log.csv` with the following fields:

`id, timestamp, temperature, humidity, pressure, altitude, latitude, longitude, accel_x, accel_y, accel_z, rot_x, rot_y, rot_z, rssi, snr, freq_error`

---

## Project Context

This project was built and presented at UL Lafayette's Engineering & Technology Week, placing **2nd in the Departmental Project competition** for the EE department.

It is also a practical foundation for future CubeSat telemetry work under the CAPE program.

---

## Hardware

| Component  | Role                      |
|------------|---------------------------|
| ESP32      | Payload MCU + receiver MCU |
| RFM96      | LoRa radio transceiver    |
| BMP 180    | Pressure, altitude        |
| Thermister | Temperature               |
| HTU21D-F   | Humidity                  |
| GY-521     | Accelerometer, gyroscope  |
| UART GPS module | Position                  |

---

*UL Lafayette · Cajun Advanced Picosatellite Experiment · Electrical Engineering*