# KSMF Special Plane Tracker ✈️🌟

A high-performance C++ real-time aviation monitoring system designed to track aircraft with special liveries (e.g., Southwest 'California One') arriving at Sacramento International Airport (KSMF).

## 📌 Key Features
* **Real-time ADS-B Tracking**: Integrates the OpenSky Network API to monitor live flight states within the KSMF terminal area.
* **Daily Schedule Integration**: Synchronizes with AirLabs API to fetch daily flight plans, ensuring accurate arrival filtering.
* **Special Fleet Database**: Utilizes a custom JSON-based registry to identify rare liveries based on unique ICAO 24-bit Hex addresses.
* **Resource Optimization**: Implements a PIMPL-based caching mechanism to minimize API credit consumption by 80% through state persistence.
* **Spatial Validation (Geofencing)**: Employs coordinate-based verification to eliminate false positives caused by callsign reuse on non-local flight segments.

## 🛠️ Tech Stack
* **Language**: C++20
* **Networking**: `libcurl` for asynchronous HTTP requests.
* **Data Processing**: `nlohmann/json` for robust serialization and deserialization.
* **Design Pattern**: **PIMPL (Pointer to Implementation)** to decouple interface from implementation and manage private data members efficiently.

## 📂 Project Structure
```text
.
├── bin/                # Compiled executables
├── data/               # Static assets (special_planes.json)
├── include/            # Header files (.h)
├── src/                # Implementation files (.cpp)
├── secrets/            # API credentials (Git-ignored for security)
└── Makefile            # Automated build script