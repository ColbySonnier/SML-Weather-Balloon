"""
SML-Weather-Balloon-Project Weather Balloon Ground Station
Parses incoming RadioLib JSON packets from the ESP32+RFM96 receiver
and displays data on a WebSocket dashboard.
"""

import asyncio
import csv
import json
import re
import threading
import time
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path

import serial
import websockets

# Configuration
SERIAL_PORT = "/dev/cu.usbserial-0001"
SERIAL_BAUD = 115200
WS_HOST     = "localhost"
WS_PORT     = 8765
HTTP_PORT   = 8080
LOG_FILE    = Path(__file__).parent / "telemetry_log.csv"
DASHBOARD   = Path(__file__).parent / "dashboard.html"


connected_clients: set = set()
serial_connected = False
packet_count = 0

# Parser state machine
_IDLE, _GOT_DATA, _GOT_RSSI, _GOT_SNR = range(4)
_state    = _IDLE
_json_str = ""
_pending  = {}

_TAG    = r'\[[^\]]+\]'
RE_DATA = re.compile(_TAG + r'\s+Data:\s*(.*)')
RE_RSSI = re.compile(_TAG + r'\s+RSSI:\s*([-\d.]+)\s*dBm')
RE_SNR  = re.compile(_TAG + r'\s+SNR:\s*([-\d.]+)\s*dB')
RE_FREQ = re.compile(_TAG + r'\s+Frequency error:\s*([-\d.]+)\s*Hz')

CSV_FIELDS = [
    "id", "timestamp",
    "temperature", "humidity", "pressure", "altitude",
    "latitude", "longitude",
    "accel_x", "accel_y", "accel_z",
    "rot_x", "rot_y", "rot_z",
    "rssi", "snr", "freq_error", 
]


def _parse_json(json_str: str) -> dict:
    # Arduino doesn't have quotes on keys: {Temperature:22.5} = {"Temperature":22.5}
    quoted = re.sub(r'(\b[A-Za-z_]\w*)\s*:', r'"\1":', json_str)
    try:
        data = json.loads(quoted)
    except json.JSONDecodeError:
        return {}
    accel = data.get("Acceleration", [0, 0, 0])
    rot   = data.get("Rotation",     [0, 0, 0])
    return {
        "temperature": float(data.get("Temperature", 0)),
        "humidity":    float(data.get("Humidity",    0)),
        "pressure":    float(data.get("Pressure",    0)),
        "altitude":    float(data.get("Altitude",    0)),
        "latitude":    float(data.get("Latitude",    0)),
        "longitude":   float(data.get("Longitude",   0)),
        "accel_x":     float(accel[0]) if len(accel) > 0 else 0.0,
        "accel_y":     float(accel[1]) if len(accel) > 1 else 0.0,
        "accel_z":     float(accel[2]) if len(accel) > 2 else 0.0,
        "rot_x":       float(rot[0])   if len(rot)   > 0 else 0.0,
        "rot_y":       float(rot[1])   if len(rot)   > 1 else 0.0,
        "rot_z":       float(rot[2])   if len(rot)   > 2 else 0.0,
    }


async def broadcast(msg: dict):
    if not connected_clients:
        return
    data = json.dumps(msg)
    await asyncio.gather(
        *[c.send(data) for c in list(connected_clients)],
        return_exceptions=True,
    )


def _parse_line(line: str, loop: asyncio.AbstractEventLoop):
    global _state, _json_str, _pending, packet_count
    if line:
        print(f"[parse|{_state}] {line[:80]!r}")

    if _state == _IDLE:
        m = RE_DATA.search(line)
        if m:
            _json_str = m.group(1).strip()
            _state = _GOT_DATA
        return

    if _state == _GOT_DATA:
        m = RE_RSSI.search(line)
        if m:
            _pending["rssi"] = float(m.group(1))
            _state = _GOT_RSSI
        return

    if _state == _GOT_RSSI:
        m = RE_SNR.search(line)
        if m:
            _pending["snr"] = float(m.group(1))
            _state = _GOT_SNR
        return

    if _state == _GOT_SNR:
        m = RE_FREQ.search(line)
        if m:
            _pending["freq_error"] = float(m.group(1))
            _pending.update(_parse_json(_json_str))
            packet_count += 1
            packet = {
                "type":      "packet",
                "id":        packet_count,
                "timestamp": datetime.utcnow().strftime("%H:%M:%S.%f")[:-3],
                **_pending,
            }
            _pending  = {}
            _json_str = ""
            _state    = _IDLE

            _log_csv(packet)
            print(
                f"[#{packet['id']:4d}] "
                f"T={packet.get('temperature', 0):.1f}°C  "
                f"Alt={packet.get('altitude', 0):.1f}m  "
                f"({packet.get('latitude', 0):.5f}, {packet.get('longitude', 0):.5f})  "
                f"RSSI={packet['rssi']:.0f}dBm  SNR={packet['snr']:.1f}dB"
            )
            asyncio.run_coroutine_threadsafe(broadcast(packet), loop)


def _log_csv(packet: dict):
    write_header = not LOG_FILE.exists()
    with LOG_FILE.open("a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_FIELDS, extrasaction="ignore")
        if write_header:
            w.writeheader()
        w.writerow(packet)


def serial_reader(loop: asyncio.AbstractEventLoop):
    global serial_connected
    while True:
        try:
            with serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1,
                               dsrdtr=False, rtscts=False) as ser:
                serial_connected = True
                print(f"[serial] Connected to {SERIAL_PORT} @ {SERIAL_BAUD} baud")
                asyncio.run_coroutine_threadsafe(
                    broadcast({"type": "serial_status", "connected": True}), loop
                )
                while True:
                    raw = ser.readline()
                    print(f"[raw] len={len(raw)} bytes={raw[:60]!r}")
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    _parse_line(line, loop)
        except serial.SerialException as e:
            if serial_connected:
                serial_connected = False
                print(f"[serial] Disconnected: {e}")
                asyncio.run_coroutine_threadsafe(
                    broadcast({"type": "serial_status", "connected": False}), loop
                )
            time.sleep(2)


async def ws_handler(websocket):
    connected_clients.add(websocket)
    print(f"[ws] +client  ({len(connected_clients)} total)")
    await websocket.send(json.dumps({
        "type":             "init",
        "serial_connected": serial_connected,
        "packet_count":     packet_count,
    }))
    try:
        async for _ in websocket:
            pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"[ws] -client  ({len(connected_clients)} total)")


MIME = {
    ".html": "text/html; charset=utf-8",
    ".png":  "image/png",
    ".jpg":  "image/jpeg",
    ".ico":  "image/x-icon",
    ".css":  "text/css",
    ".js":   "application/javascript",
}

class DashboardHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/" or path == "/index.html":
            target = DASHBOARD
        else:
            target = DASHBOARD.parent / path.lstrip("/")
        try:
            data = target.read_bytes()
            ext  = target.suffix.lower()
            ctype = MIME.get(ext, "application/octet-stream")
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except FileNotFoundError:
            self.send_error(404, f"{target.name} not found")

    def log_message(self, *_):
        pass


async def main():
    loop = asyncio.get_running_loop()
    threading.Thread(target=serial_reader, args=(loop,), daemon=True).start()
    http = HTTPServer((WS_HOST, HTTP_PORT), DashboardHandler)
    threading.Thread(target=http.serve_forever, daemon=True).start()
    print(f"[http] Dashboard → http://{WS_HOST}:{HTTP_PORT}")
    print(f"[ws]   WebSocket → ws://{WS_HOST}:{WS_PORT}")
    async with websockets.serve(ws_handler, WS_HOST, WS_PORT):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[main] Shutdown.")
