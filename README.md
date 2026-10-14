# ✈️ SkyRoute — Airline Reservation System

A **full-stack Airline Reservation System** built entirely in **C++** using the [Crow](https://crowcpp.org/) micro web-framework, **SQLite** for persistent storage, and **Dijkstra's shortest-path algorithm** for optimal route computation. The project demonstrates systems-level programming concepts (graph algorithms, embedded databases, RESTful APIs) with a modern, responsive web frontend.

---

## 🚀 Features

- **Cheapest Route Finder** — Uses **Dijkstra's Algorithm** on a weighted flight graph to compute the lowest-cost path between any two airports (with multi-hop support).
- **Interactive Seat Map** — Visual cabin layout (6 rows × 4 columns) with real-time seat availability. Click to select; booked seats are locked.
- **Seat Booking with Collision Handling** — Concurrent booking attempts on the same seat are safely handled via SQLite's `UNIQUE` constraint, returning a `409 Conflict` response.
- **Persistent Storage** — All bookings are stored in a local SQLite database (`airline.db`), surviving server restarts.
- **RESTful API** — Clean JSON API for route optimization, seat queries, and ticket booking.
- **Single-Page Frontend** — A responsive, dark-themed UI served directly by the C++ backend.

---

## 🛠️ Tech Stack

| Layer        | Technology                                                                 |
|--------------|----------------------------------------------------------------------------|
| **Language** | C++17                                                                      |
| **Web Framework** | [Crow](https://crowcpp.org/) (header-only, included in `include/`)   |
| **Database** | [SQLite3](https://www.sqlite.org/) (embedded, zero-config)                 |
| **Build**    | [CMake](https://cmake.org/) (≥ 3.15)                                      |
| **Algorithm**| Dijkstra's Shortest Path (min-heap priority queue)                         |
| **Frontend** | HTML5, CSS3 (custom properties), Vanilla JavaScript (Fetch API)            |

---

## 📂 Project Structure

```
Airline-Reservation-System/
├── CMakeLists.txt          # Build configuration
├── README.md               # Project documentation
├── .gitignore              # Ignores build/ and *.db files
├── include/
│   └── crow_all.h          # Crow web-framework (single-header)
├── public/
│   └── index.html          # Frontend UI (served at "/")
└── src/
    └── main.cpp            # Application entry point (server + logic)
```

---

## ⚙️ Prerequisites

Make sure you have the following installed:

- **C++17 compatible compiler** (GCC ≥ 7, Clang ≥ 5, or MSVC ≥ 19.14)
- **CMake** ≥ 3.15
- **SQLite3 development libraries**

### Installing Dependencies

**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libsqlite3-dev
```

**macOS (Homebrew):**
```bash
brew install cmake sqlite3
```

**Fedora / RHEL:**
```bash
sudo dnf install gcc-c++ cmake sqlite-devel
```

---

## 🔨 Build & Run

```bash
# 1. Clone the repository
git clone https://github.com/<your-username>/Airline-Reservation-System.git
cd Airline-Reservation-System

# 2. Create a build directory and compile
mkdir build && cd build
cmake ..
make

# 3. Run the server (from the project root, so it can find public/)
cd ..
./build/airline_server
```

The server starts on **http://localhost:18080**. Open it in your browser to use the application.

---

## 📡 API Endpoints

### 1. Health Check
```
GET /api/health
```
**Response:**
```json
{ "status": "running" }
```

### 2. Find Cheapest Route (Dijkstra)
```
GET /api/flights/optimize?from=DEL&to=BLR
```
**Response:**
```json
{
  "total_cost": 7700.0,
  "path": ["DEL", "BOM", "BLR"],
  "flights": ["AI101", "6E202"]
}
```

### 3. Get Occupied Seats for a Flight
```
GET /api/flights/{flight_id}/seats
```
**Example:** `GET /api/flights/AI101/seats`

**Response:**
```json
{
  "occupied_seats": ["1A", "3C"]
}
```

### 4. Book a Seat
```
POST /api/book
Content-Type: application/json
```
**Request Body:**
```json
{
  "flight_id": "AI101",
  "passenger_name": "Shashank",
  "seat_number": "2B"
}
```
**Success (201):**
```json
{
  "status": "CONFIRMED",
  "flight_id": "AI101",
  "passenger": "Shashank",
  "seat": "2B"
}
```
**Conflict (409) — Seat Already Booked:**
```json
{
  "error": "Seat 2B is already booked on flight AI101!"
}
```

---

## 🗺️ Available Flight Routes

The demo includes the following hardcoded routes:

| Flight ID | Origin         | Destination     | Price (₹) |
|-----------|----------------|-----------------|-----------|
| AI101     | DEL (New Delhi)| BOM (Mumbai)    | 4,500     |
| 6E202     | BOM (Mumbai)   | BLR (Bengaluru) | 3,200     |
| UK808     | DEL (New Delhi)| BLR (Bengaluru) | 8,500     |

> **Note:** Dijkstra's algorithm finds that `DEL → BOM → BLR` (₹7,700) is cheaper than the direct `DEL → BLR` (₹8,500).

---

## 💡 Key Concepts Demonstrated

- **Graph Algorithms** — Dijkstra's shortest path using `std::priority_queue` (min-heap).
- **Embedded Database** — SQLite3 with prepared statements and parameterized queries (SQL injection safe).
- **RESTful API Design** — Proper HTTP methods (`GET`, `POST`) and status codes (`200`, `201`, `400`, `404`, `409`, `500`).
- **Concurrency Handling** — Seat booking collision detection via database constraints.
- **Systems Programming** — Raw C++ with no heavy frameworks; manual memory management for SQLite.
- **Full-Stack C++** — Both backend API and static file serving from a single C++ binary.

---

## 🤝 Contributing

Contributions are welcome! Feel free to:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/add-new-routes`)
3. **Commit** your changes (`git commit -m "Add new flight routes"`)
4. **Push** to the branch (`git push origin feature/add-new-routes`)
5. **Open** a Pull Request

---

## 📄 License

This project is open-source and available under the [MIT License](LICENSE).

---

## 👤 Author

**Shashanka Biswas**
**Middi Jitesh**

---

> ⭐ If you found this project helpful, consider giving it a star!
