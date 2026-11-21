# Smart Parking AI

An AI-powered smart parking management system that helps drivers and operators find, reserve, and manage parking spaces in real time.

## Overview

Smart Parking AI combines computer vision, occupancy prediction, and a management dashboard to:

- Detect available parking spaces from camera feeds
- Predict occupancy trends and peak hours
- Let drivers search, reserve, and pay for spots
- Give operators live dashboards, analytics, and alerts

## Planned Modules

| Module | Purpose |
| --- | --- |
| `detection/` | Vehicle and free-space detection from images/video |
| `occupancy/` | Occupancy prediction and forecasting models |
| `api/` | REST API for slots, reservations, and payments |
| `dashboard/` | Operator and driver web interface |
| `docs/` | Architecture, API reference, and setup guides |

## Demo

A zero-dependency interactive demo lives in [`demo/index.html`](demo/index.html). It simulates
48 parking slots across four zones with live arrivals/departures, AI occupancy forecasts, and a
reservation flow.

Run it locally:

```bash
cd demo
python3 -m http.server 8000
```

Then open http://localhost:8000.

## Getting Started

Setup instructions will be added as modules are implemented.

## Tech Stack (proposed)

- **Backend:** Python (FastAPI)
- **AI/CV:** PyTorch, OpenCV
- **Frontend:** React
- **Database:** PostgreSQL
- **Deployment:** Docker

## License

To be decided.
