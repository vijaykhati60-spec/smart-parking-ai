"""In-memory demo server for ParkEase.

Mirrors the API exposed by app.py so the real frontend can be explored without
MySQL or the Windows parking.exe binary. All data resets when the process stops.
"""

import hashlib
import hmac
import math
import os
import secrets
import threading
from datetime import datetime, timedelta
from pathlib import Path

from flask import Flask, jsonify, request, send_from_directory

PROJECT_ROOT = Path(__file__).resolve().parent.parent
PARKING_RATE = 20
SLOT_COUNT = 5
QUEUE_LIMIT = 10

app = Flask(__name__, static_folder=str(PROJECT_ROOT), static_url_path="")

lock = threading.Lock()
users = {}
parked = {}
waiting = []
history = []
next_history_id = 1

DEMO_BADGE = """
<div style="position:fixed;right:16px;bottom:16px;z-index:9999;
background:#111a2e;color:#e8eefc;border:1px solid #38bdf8;border-radius:999px;
padding:8px 14px;font:600 12px/1 system-ui,sans-serif;box-shadow:0 8px 24px rgba(0,0,0,.35)">
Demo mode · in-memory data (resets on restart)
</div>
"""


def make_password(password):
    salt = secrets.token_hex(16)
    iterations = 200000
    digest = hashlib.pbkdf2_hmac(
        "sha256", password.encode(), bytes.fromhex(salt), iterations
    ).hex()
    return f"PBKDF2${iterations}${salt}${digest}"


def check_password(password, saved):
    try:
        scheme, iterations, salt, expected = saved.split("$")
        actual = hashlib.pbkdf2_hmac(
            "sha256", password.encode(), bytes.fromhex(salt), int(iterations)
        ).hex()
        return scheme == "PBKDF2" and hmac.compare_digest(actual, expected)
    except (ValueError, AttributeError):
        return False


def slot_rows():
    rows = []
    for slot_id in range(1, SLOT_COUNT + 1):
        vehicle = next(
            (v for v, rec in parked.items() if rec["slot_id"] == slot_id), None
        )
        rows.append(
            {
                "slot_id": slot_id,
                "status": "Occupied" if vehicle else "Available",
                "vehicle_number": vehicle,
            }
        )
    return rows


def free_slot():
    taken = {rec["slot_id"] for rec in parked.values()}
    for slot_id in range(1, SLOT_COUNT + 1):
        if slot_id not in taken:
            return slot_id
    return None


def seed():
    global next_history_id

    users["demo"] = make_password("demo123")
    now = datetime.now()
    for slot_id, vehicle in ((1, "UK07AB1234"), (2, "MH12CD5678"), (4, "DL8CAF9012")):
        parked[vehicle] = {
            "slot_id": slot_id,
            "entry_time": now - timedelta(hours=slot_id + 1, minutes=15),
        }
    waiting.append(
        {"queue_id": 1, "vehicle_number": "KA05XY4321", "joined_at": now - timedelta(minutes=12)}
    )
    history.append(
        {
            "history_id": 1,
            "vehicle_number": "GJ01KL2468",
            "slot_id": 3,
            "entry_time": (now - timedelta(hours=5)).isoformat(sep=" "),
            "exit_time": (now - timedelta(hours=3)).isoformat(sep=" "),
            "fee": 40.0,
        }
    )
    next_history_id = 2


seed()


@app.route("/")
def home():
    html = (PROJECT_ROOT / "index.html").read_text(encoding="utf-8")
    return html.replace("</body>", DEMO_BADGE + "</body>")


@app.route("/api/health")
def health():
    return jsonify(
        {
            "ok": True,
            "message": "ParkEase demo backend running",
            "database_connected": True,
            "c_program_found": True,
            "demo_mode": True,
        }
    )


@app.route("/api/register", methods=["POST"])
def register():
    data = request.get_json(silent=True) or {}
    username = str(data.get("username", "")).strip()
    password = str(data.get("password", ""))

    if not username or not password:
        return jsonify({"error": "Enter username and password"}), 400

    with lock:
        if username in users:
            return jsonify({"error": "Username already exists"}), 409
        users[username] = make_password(password)

    return jsonify({"message": "Registration successful"})


@app.route("/api/login", methods=["POST"])
def login():
    data = request.get_json(silent=True) or {}
    username = str(data.get("username", "")).strip()
    password = str(data.get("password", ""))

    if not username or not password:
        return jsonify({"error": "Enter username and password"}), 400

    with lock:
        saved = users.get(username)

    if not saved or not check_password(password, saved):
        return jsonify({"error": "Invalid username or password"}), 401

    return jsonify(
        {"message": "Login successful", "user": {"user_id": 1, "username": username}}
    )


@app.route("/api/slots")
def slots():
    with lock:
        return jsonify(slot_rows())


@app.route("/api/queue")
def queue():
    with lock:
        rows = [
            {
                "queue_id": item["queue_id"],
                "vehicle_number": item["vehicle_number"],
                "joined_at": item["joined_at"].isoformat(sep=" "),
            }
            for item in waiting
        ]
    return jsonify(rows)


@app.route("/api/park", methods=["POST"])
def park():
    data = request.get_json(silent=True) or {}
    vehicle = str(data.get("vehicle_number", "")).strip().upper()

    if not vehicle:
        return jsonify({"error": "Enter vehicle number"}), 400
    if not vehicle.isalnum():
        return jsonify(
            {"error": "Enter a valid vehicle number using letters and numbers only."}
        ), 400

    with lock:
        if vehicle in parked or any(w["vehicle_number"] == vehicle for w in waiting):
            return jsonify({"error": "This vehicle is already parked or waiting."}), 409

        slot_id = free_slot()
        if slot_id is not None:
            parked[vehicle] = {"slot_id": slot_id, "entry_time": datetime.now()}
            return jsonify(
                {"message": "Vehicle parked successfully.", "slot_id": slot_id}
            )

        if len(waiting) >= QUEUE_LIMIT:
            return jsonify({"error": "Parking and waiting queue are both full."}), 409

        waiting.append(
            {
                "queue_id": (waiting[-1]["queue_id"] + 1 if waiting else 1),
                "vehicle_number": vehicle,
                "joined_at": datetime.now(),
            }
        )

    return jsonify({"message": "Parking is full. Vehicle added to the waiting queue."})


@app.route("/api/leave", methods=["POST"])
def leave():
    global next_history_id

    data = request.get_json(silent=True) or {}
    vehicle = str(data.get("vehicle_number", "")).strip().upper()

    if not vehicle:
        return jsonify({"error": "Enter vehicle number"}), 400

    with lock:
        record = parked.pop(vehicle, None)
        if record is None:
            return jsonify({"error": "Parked vehicle was not found in the database."}), 409

        exit_time = datetime.now()
        seconds = max(0, int((exit_time - record["entry_time"]).total_seconds()))
        billed_hours = max(1, math.ceil(seconds / 3600))
        fee = billed_hours * PARKING_RATE

        history.insert(
            0,
            {
                "history_id": next_history_id,
                "vehicle_number": vehicle,
                "slot_id": record["slot_id"],
                "entry_time": record["entry_time"].isoformat(sep=" "),
                "exit_time": exit_time.isoformat(sep=" "),
                "fee": float(fee),
            },
        )
        next_history_id += 1

        response = {
            "message": f"Vehicle exited successfully. Parking fee: Rs {fee}.",
            "vehicle_number": vehicle,
            "slot_id": record["slot_id"],
            "entry_time": record["entry_time"].isoformat(sep=" "),
            "exit_time": exit_time.isoformat(sep=" "),
            "duration_seconds": seconds,
            "billed_hours": billed_hours,
            "rate_per_hour": PARKING_RATE,
            "fee": fee,
        }

        if waiting:
            promoted = waiting.pop(0)
            parked[promoted["vehicle_number"]] = {
                "slot_id": record["slot_id"],
                "entry_time": exit_time,
            }
            response["next_vehicle_parked"] = promoted["vehicle_number"]
            response["freed_slot"] = record["slot_id"]

    return jsonify(response)


@app.route("/api/history")
def exit_history():
    with lock:
        return jsonify(list(history))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 12000)), debug=False)
