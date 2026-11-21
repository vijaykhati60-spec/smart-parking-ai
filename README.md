# ParkEase — Smart Parking Management System

ParkEase is a smart parking management system built from three parts: a browser dashboard
(HTML/CSS/JavaScript), a Flask REST API, and a C engine that manages slots with core data
structures (a stack for parked vehicles and a circular queue for waiting vehicles), backed by
MySQL.

## Features

- Register and log in with salted, hashed passwords
- Live parking slot status and waiting queue
- Park and release vehicles with automatic fee calculation (₹20/hour)
- Waiting queue when the lot is full
- Exit history and revenue tracking
- Flask serves the web UI and invokes the compiled C program for slot operations

## Project structure

```text
.
├── index.html          # Web dashboard
├── script.js           # Frontend logic and API calls
├── style.css           # Dashboard styling
├── backend/
│   ├── app.py          # Flask API server
│   ├── demo_app.py     # In-memory demo server (no MySQL required)
│   ├── parking.c       # C engine (stack + circular queue + MySQL)
│   ├── parking_backup.c
│   ├── db_test.c
│   ├── parking.exe     # Compiled C engine used by app.py
│   ├── parkease_gui.exe
│   └── db_test.exe
└── demo/               # Standalone interactive UI demo
```

## Setup

Prerequisites: Python 3, Flask, `mysql-connector-python`, MySQL, and a C compiler with the
MySQL connector library (MSYS2/UCRT64 on Windows).

1. Create the MySQL database and application user referenced in `backend/app.py`.
2. Compile the C engine:

   ```bash
   cd backend
   gcc parking.c -o parking.exe $(mysql_config --cflags --libs)
   ```

3. Install dependencies and start the server:

   ```bash
   pip install flask mysql-connector-python
   cd backend
   python app.py
   ```

4. Open http://localhost:5000.

`backend/app.py` expects Windows paths for the MySQL connector DLL
(`C:\msys64\ucrt64\bin`) and for `backend/parking.exe`.

## Live demo

**🔗 ParkEase app:** https://work-1-hgyewkuurhwpeytp.prod-runtime.all-hands.dev/

The hosted demo runs the same dashboard and API as `backend/app.py`, backed by an in-memory
store (`backend/demo_app.py`) so it works without MySQL or the Windows `parking.exe`. Data
resets when the server restarts.

- Demo login: `demo` / `demo123` (or click **Login / Register** to create an account)
- Try **Vehicle Entry** / **Vehicle Exit** to see slots, the waiting queue, and the bill update

Run the demo locally:

```bash
pip install flask
cd backend
python demo_app.py
```

Then open http://localhost:12000.

## Interactive UI demo

A separate zero-dependency demo lives in [`demo/index.html`](demo/index.html). It simulates
48 parking slots across four zones with live arrivals/departures, occupancy forecasts, and a
reservation flow.

```bash
cd demo
python3 -m http.server 8000
```

## Security note

Database credentials are currently hardcoded in `backend/app.py` and `backend/parking.c`.
Move them to environment variables before sharing the repository publicly.

## Tech stack

- **Frontend:** HTML, CSS, vanilla JavaScript
- **API:** Python (Flask)
- **Core engine:** C (stack, circular queue, linked records)
- **Database:** MySQL

## License

To be decided.
