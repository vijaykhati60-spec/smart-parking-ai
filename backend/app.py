
from flask import Flask, request, jsonify, send_from_directory
from pathlib import Path
import mysql.connector
import hashlib
import secrets
import hmac
import subprocess
import json
import os

PROJECT_FOLDER = Path(__file__).resolve().parent.parent
BACKEND_FOLDER = Path(__file__).resolve().parent
C_PROGRAM = BACKEND_FOLDER / "parking.exe"

PARKING_RATE = 20

app = Flask(
    __name__,
    static_folder=str(PROJECT_FOLDER),
    static_url_path=""
)

DB_CONFIG = {
    "host": "localhost",
    "user": "parkease_app",
    "password": "ParkEase_App_2026!",
    "database": "parkease_db"
}


def get_db():
    return mysql.connector.connect(**DB_CONFIG)


# ---------- PASSWORD HELPERS ----------

def make_password(password):
    salt = secrets.token_hex(16)
    iterations = 200000

    digest = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode(),
        bytes.fromhex(salt),
        iterations
    ).hex()

    return f"PBKDF2${iterations}${salt}${digest}"


def check_password(password, saved):
    try:
        scheme, iterations, salt, expected = saved.split("$")

        actual = hashlib.pbkdf2_hmac(
            "sha256",
            password.encode(),
            bytes.fromhex(salt),
            int(iterations)
        ).hex()

        return (
            scheme == "PBKDF2"
            and hmac.compare_digest(actual, expected)
        )
    except (ValueError, AttributeError):
        return False



# ---------- CALL THE C PROGRAM ----------

def call_c_program(action, vehicle):
    if not C_PROGRAM.is_file():
        return None, (
            "C program not found. Compile backend/parking.c "
            "to create backend/parking.exe first."
        )

    # Make the MSYS2 connector DLL available.
    env = os.environ.copy()
    connector_bin = Path(r"C:\msys64\ucrt64\bin")
    env["PATH"] = str(connector_bin) + os.pathsep + env.get("PATH", "")

    try:
        completed = subprocess.run(
            [str(C_PROGRAM), action, vehicle],
            capture_output=True,
            text=True,
            timeout=20,
            check=False,
            env=env
        )

        output = completed.stdout.strip()

        if not output:
            detail = completed.stderr.strip()
            return None, (
                "C program returned no response. "
                + (detail if detail else "Check MySQL and connector DLL.")
            )

        try:
            result = json.loads(output.splitlines()[-1])
        except json.JSONDecodeError:
            return None, (
                "C program returned an invalid response: " + output
            )

        # Convert C response keys into the format Flask expects.
        if "success" in result:
            result["ok"] = result["success"]

        if "message" in result and "error" not in result:
            result["error"] = result["message"]

        return result, None

    except subprocess.TimeoutExpired:
        return None, "C parking program timed out."

    except OSError as error:
        return None, f"Could not start C program: {error}"


# ---------- WEBSITE ----------

@app.route("/")
def home():
    return send_from_directory(PROJECT_FOLDER, "index.html")


# ---------- HEALTH CHECK ----------

@app.route("/api/health")
def health():
    db_ok = False
    db_error = None

    try:
        db = get_db()
        db.ping(reconnect=False, attempts=1, delay=0)
        db_ok = True
        db.close()
    except mysql.connector.Error as error:
        db_error = str(error)

    return jsonify({
        "ok": db_ok,
        "message": "ParkEase backend running" if db_ok
                   else "Database connection failed",
        "database_connected": db_ok,
        "c_program_found": C_PROGRAM.is_file(),
        "database_error": db_error
    }), (200 if db_ok else 503)


# ---------- REGISTER ----------

@app.route("/api/register", methods=["POST"])
def register():
    data = request.get_json(silent=True) or {}

    username = str(data.get("username", "")).strip()
    password = str(data.get("password", ""))

    if not username or not password:
        return jsonify({
            "error": "Enter username and password"
        }), 400

    db = get_db()
    cur = db.cursor()

    try:
        cur.execute(
            "SELECT user_id FROM users WHERE username=%s",
            (username,)
        )

        if cur.fetchone():
            return jsonify({
                "error": "Username already exists"
            }), 409

        cur.execute(
            "INSERT INTO users (username, password_hash) "
            "VALUES (%s, %s)",
            (username, make_password(password))
        )

        db.commit()

        return jsonify({
            "message": "Registration successful"
        })

    except mysql.connector.Error as error:
        db.rollback()
        return jsonify({"error": str(error)}), 400

    finally:
        cur.close()
        db.close()


# ---------- LOGIN ----------

@app.route("/api/login", methods=["POST"])
def login():
    data = request.get_json(silent=True) or {}

    username = str(data.get("username", "")).strip()
    password = str(data.get("password", ""))

    if not username or not password:
        return jsonify({
            "error": "Enter username and password"
        }), 400

    db = get_db()
    cur = db.cursor()

    try:
        cur.execute(
            "SELECT user_id, username, password_hash "
            "FROM users WHERE username=%s",
            (username,)
        )

        user = cur.fetchone()

        if not user or not check_password(password, user[2]):
            return jsonify({
                "error": "Invalid username or password"
            }), 401

        return jsonify({
            "message": "Login successful",
            "user": {
                "user_id": user[0],
                "username": user[1]
            }
        })

    finally:
        cur.close()
        db.close()


# ---------- PARKING SLOTS ----------

@app.route("/api/slots")
def slots():
    db = get_db()
    cur = db.cursor(dictionary=True)

    try:
        cur.execute("""
            SELECT
                p.slot_id,
                p.status,
                (
                    SELECT v.vehicle_number
                    FROM vehicles v
                    WHERE v.slot_id = p.slot_id
                      AND v.status = 'Parked'
                    ORDER BY v.vehicle_id DESC
                    LIMIT 1
                ) AS vehicle_number
            FROM parking_slots p
            ORDER BY p.slot_id
        """)

        return jsonify(cur.fetchall())

    finally:
        cur.close()
        db.close()


# ---------- WAITING QUEUE ----------

@app.route("/api/queue")
def queue():
    db = get_db()
    cur = db.cursor(dictionary=True)

    try:
        cur.execute("""
            SELECT queue_id, vehicle_number, joined_at
            FROM waiting_queue
            ORDER BY queue_id ASC
        """)

        result = cur.fetchall()

        for item in result:
            if item["joined_at"]:
                item["joined_at"] = item["joined_at"].isoformat(
                    sep=" "
                )

        return jsonify(result)

    finally:
        cur.close()
        db.close()


# ---------- PARK VEHICLE THROUGH C ----------

@app.route("/api/park", methods=["POST"])
def park():
    data = request.get_json(silent=True) or {}

    vehicle = str(
        data.get("vehicle_number", "")
    ).strip().upper()

    if not vehicle:
        return jsonify({
            "error": "Enter vehicle number"
        }), 400

    result, error = call_c_program("park", vehicle)

    if error:
        return jsonify({"error": error}), 500

    if not result.get("ok"):
        return jsonify({
            "error": result.get("error", "Parking failed")
        }), 409

    result.pop("ok", None)
    return jsonify(result)


# ---------- LEAVE VEHICLE THROUGH C ----------

@app.route("/api/leave", methods=["POST"])
def leave():
    data = request.get_json(silent=True) or {}

    vehicle = str(
        data.get("vehicle_number", "")
    ).strip().upper()

    if not vehicle:
        return jsonify({
            "error": "Enter vehicle number"
        }), 400

    result, error = call_c_program("leave", vehicle)

    if error:
        return jsonify({"error": error}), 500

    if not result.get("ok"):
        return jsonify({
            "error": result.get("error", "Exit failed")
        }), 409

    result.pop("ok", None)
    return jsonify(result)


# ---------- EXIT HISTORY / PARKING BILLS ----------

@app.route("/api/history")
def history():
    db = get_db()
    cur = db.cursor(dictionary=True)

    try:
        cur.execute("""
            SELECT
                history_id,
                vehicle_number,
                slot_id,
                entry_time,
                exit_time,
                fee
            FROM exit_history
            ORDER BY history_id DESC
        """)

        result = cur.fetchall()

        for item in result:
            if item["entry_time"]:
                item["entry_time"] = item["entry_time"].isoformat(
                    sep=" "
                )

            if item["exit_time"]:
                item["exit_time"] = item["exit_time"].isoformat(
                    sep=" "
                )

            if item["fee"] is not None:
                item["fee"] = float(item["fee"])

        return jsonify(result)

    finally:
        cur.close()
        db.close()


if __name__ == "__main__":
    app.run(debug=True)
