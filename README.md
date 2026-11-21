# ParkEase — Smart Parking Management System

ParkEase is a smart parking management system for a small parking lot. It combines a browser
dashboard, a Flask REST API, and a C engine that manages slots with classic data structures —
a **stack** for parked vehicles and a **circular queue** for waiting vehicles — all backed by
MySQL.

- **5 parking slots** (`SIZE = 5`)
- **10 waiting positions** (`MAX_WAIT = 10`)
- **₹20 per hour** (`RATE = 20`), minimum one billed hour

## Live demo

**🔗 https://work-1-hgyewkuurhwpeytp.prod-runtime.all-hands.dev/**

- Login: `demo` / `demo123` (or click **Login / Register** to create an account)
- Try **Vehicle Entry** and **Vehicle Exit** to watch slots, the waiting queue, the bill, and history update

The hosted demo runs the same dashboard and API as `backend/app.py`, backed by an in-memory
store (`backend/demo_app.py`) so it works without MySQL or the Windows `parking.exe`. Data
resets when the server restarts.

## Permanent deployment

The repository ships with deployment files, so the demo can run on a permanent URL:

| File | Purpose |
| --- | --- |
| `requirements.txt` | Python dependencies (Flask, Gunicorn, MySQL connector) |
| `Procfile` | Process command for Render/Heroku-style hosts |
| `render.yaml` | Render Blueprint (free web service) |
| `Dockerfile` | Container image for Railway, Fly.io, or any Docker host |

### Render (free permanent URL)

1. Open https://dashboard.render.com/blueprints and connect this repository.
2. Render reads `render.yaml` and creates the `parkease-demo` web service.
3. Your permanent URL will be `https://parkease-demo.onrender.com` (or the name Render assigns).

Free instances sleep after inactivity, so the first request may take a few seconds.

### Docker (any host)

```bash
docker build -t parkease .
docker run -p 8000:8000 parkease
```

Then open http://localhost:8000.

The demo keeps data in memory, so it resets on restart or redeploy. To persist data, deploy the
MySQL-backed `backend/app.py` instead and point it at a managed MySQL instance.

## Features

- Register and log in with salted PBKDF2-SHA256 password hashes
- Dashboard with total, available, and occupied slot counters
- Live slot cards showing the vehicle parked in each slot
- Vehicle entry with automatic slot allocation
- Vehicle exit with an itemised parking bill (entry/exit time, duration, billed hours, rate, total)
- FIFO waiting queue that automatically promotes the next vehicle when a slot frees up
- Exit history with past bills and fees
- C engine usable from the command line, independent of the web app
- Standalone console version with no database dependency (`parking_backup.c`)

## Tech stack

| Layer | Technology |
| --- | --- |
| Frontend | HTML, CSS, vanilla JavaScript |
| API | Python 3, Flask |
| Core engine | C (stack + circular queue), MySQL C connector |
| Database | MySQL 8 |
| Hosting (demo) | Flask dev server with in-memory store |

## Project structure

```text
.
├── index.html              # Login page + dashboard
├── script.js               # Frontend logic and API calls
├── style.css               # Dashboard styling
├── requirements.txt        # Python dependencies
├── Procfile                # Process command for Render/Heroku-style hosts
├── render.yaml             # Render Blueprint (free web service)
├── Dockerfile              # Container image for Railway/Fly/any Docker host
├── backend/
│   ├── app.py              # Flask API server (MySQL + C engine)
│   ├── demo_app.py         # In-memory demo server (no MySQL required)
│   ├── parking.c           # C engine: stack, circular queue, MySQL, JSON output
│   ├── parking_backup.c    # Standalone console version (no database)
│   ├── db_test.c           # MySQL connectivity test
│   ├── parking.exe         # Compiled C engine used by app.py
│   ├── parkease_gui.exe    # Prebuilt Windows GUI build
│   └── db_test.exe         # Compiled connectivity test
└── demo/
    └── index.html          # Separate zero-dependency UI demo
```

## How it works

```text
Browser (index.html + script.js)
        │  fetch /api/*
        ▼
Flask API (backend/app.py) ──► MySQL (users, slots, vehicles, queue, history)
        │  subprocess
        ▼
C engine (parking.exe park|leave VEHICLE) ──► JSON result
```

`app.py` serves the frontend, validates requests, and delegates every parking operation to
`parking.exe`. The C program opens the database, updates the slot, vehicle, queue, and history
tables, then prints a small JSON object that Flask returns to the browser.

## Data structures

`parking.c` mirrors the lot in memory and keeps it in sync with MySQL:

- **Stack** (`char parking[SIZE][REG]`, `top`) — LIFO record of currently parked vehicles.
- **Circular queue** (`char waiting[MAX_WAIT][REG]`, `front`, `rear`, `count`) — FIFO list of
  vehicles waiting for a slot. Indexes wrap with `% MAX_WAIT`.
- Vehicle numbers are limited to 20 characters and validated as letters/digits only.

When a vehicle leaves, the slot becomes `Available`; if the queue is not empty, the vehicle at
the front is promoted into that slot immediately.

## Database schema

The queries in `app.py` and `parking.c` use these tables. This is a reference schema — adjust
it if your existing `parkease_db` differs.

```sql
CREATE DATABASE IF NOT EXISTS parkease_db;
USE parkease_db;

CREATE TABLE users (
  user_id       INT AUTO_INCREMENT PRIMARY KEY,
  username      VARCHAR(50)  NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL
);

CREATE TABLE parking_slots (
  slot_id INT PRIMARY KEY,
  status  ENUM('Available','Occupied') NOT NULL DEFAULT 'Available'
);

CREATE TABLE vehicles (
  vehicle_id     INT AUTO_INCREMENT PRIMARY KEY,
  vehicle_number VARCHAR(20) NOT NULL,
  slot_id        INT NULL,
  status         ENUM('Parked','Waiting','Exited') NOT NULL,
  entry_time     DATETIME DEFAULT CURRENT_TIMESTAMP,
  exit_time      DATETIME NULL,
  fee            INT NOT NULL DEFAULT 0
);

CREATE TABLE waiting_queue (
  queue_id       INT AUTO_INCREMENT PRIMARY KEY,
  vehicle_number VARCHAR(20) NOT NULL,
  joined_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE exit_history (
  history_id     INT AUTO_INCREMENT PRIMARY KEY,
  vehicle_number VARCHAR(20) NOT NULL,
  slot_id        INT NULL,
  entry_time     DATETIME,
  exit_time      DATETIME,
  fee            INT
);

INSERT INTO parking_slots (slot_id, status) VALUES
  (1,'Available'), (2,'Available'), (3,'Available'), (4,'Available'), (5,'Available');
```

Fee calculation used by the C engine:

```sql
GREATEST(1, CEIL(TIMESTAMPDIFF(MINUTE, entry_time, NOW()) / 60)) * 20
```

## API reference

| Method | Endpoint | Body | Purpose |
| --- | --- | --- | --- |
| GET | `/` | — | Serves `index.html` |
| GET | `/api/health` | — | API, database, and C binary status |
| POST | `/api/register` | `{ "username", "password" }` | Create an account |
| POST | `/api/login` | `{ "username", "password" }` | Authenticate |
| GET | `/api/slots` | — | All slots with `slot_id`, `status`, `vehicle_number` |
| GET | `/api/queue` | — | Waiting vehicles in FIFO order |
| POST | `/api/park` | `{ "vehicle_number" }` | Park a vehicle or queue it if full |
| POST | `/api/leave` | `{ "vehicle_number" }` | Exit, bill the vehicle, promote the queue |
| GET | `/api/history` | — | Past exits with fees |

`/api/park` and `/api/leave` return `409` for business-rule failures (already parked, lot and
queue full, vehicle not found) and `400` for invalid input.

## Setup

### Prerequisites

- Python 3.9+ with `flask` and `mysql-connector-python`
- MySQL 8 running on `localhost:3306`
- Windows + MSYS2/UCRT64 (or any toolchain with the MySQL C connector) to build and run
  `parking.exe`

### 1. Database

Create the database, user, and tables using the schema above, then grant access:

```sql
CREATE USER 'parkease_app'@'localhost' IDENTIFIED BY 'your-password';
GRANT ALL PRIVILEGES ON parkease_db.* TO 'parkease_app'@'localhost';
FLUSH PRIVILEGES;
```

Update `DB_CONFIG` in `backend/app.py` (and the credentials in `backend/parking.c` and
`backend/db_test.c`) to match.

### 2. Build the C engine

From `backend/`, with MSYS2 UCRT64:

```bash
gcc parking.c -o parking.exe -I"C:/msys64/ucrt64/include/mysql" -L"C:/msys64/ucrt64/lib" -lmysqlclient
gcc db_test.c -o db_test.exe -I"C:/msys64/ucrt64/include/mysql" -L"C:/msys64/ucrt64/lib" -lmysqlclient
```

`app.py` prepends `C:\msys64\ucrt64\bin` to `PATH` so the MySQL connector DLL can be found; if
your install lives elsewhere, update that line.

### 3. Run the web app

```bash
pip install flask mysql-connector-python
cd backend
python app.py
```

Open http://localhost:5000.

### 4. Run the demo (no database needed)

```bash
pip install flask
cd backend
python demo_app.py
```

Open http://localhost:12000. Demo login: `demo` / `demo123`.

## Command-line usage

Database-backed engine (prints JSON):

```bash
cd backend
./parking.exe park UK07AB1234
./parking.exe leave UK07AB1234
```

Connectivity check:

```bash
./db_test.exe
# Connected to parkease_db successfully!
```

Standalone console version, no MySQL:

```bash
gcc parking_backup.c -o parking_backup.exe
./parking_backup.exe
```

Its menu offers: park vehicle, leave parking (with a bill based on hours entered), display
parked vehicles, display waiting queue, and exit.

`parkease_gui.exe` is a prebuilt Windows GUI build; run it directly on Windows. Its source is
not included in this repository.

## Security notes

Database credentials are currently hardcoded in `backend/app.py`, `backend/parking.c`, and
`backend/db_test.c`. Before making the repository public:

- Rotate the MySQL password
- Move credentials to environment variables (for example `DB_HOST`, `DB_USER`, `DB_PASS`,
  `DB_NAME`) and read them in Python and C
- Keep the repo private if the credentials cannot be rotated

Passwords stored by the web app are salted and hashed with PBKDF2-SHA256 (200,000 iterations);
plain-text passwords are never stored.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| `Could not start C program` | `parking.exe` missing from `backend/` or not executable |
| `Database connection failed` on `/api/health` | MySQL not running, or `DB_CONFIG` mismatch |
| `mysql.connector` DLL load error on Windows | `C:\msys64\ucrt64\bin` not on `PATH` |
| Port already in use | Stop the other process or change the Flask port |
| Slot counts look wrong | `parking_slots` table must contain exactly the rows you want to expose |

## License

To be decided.
