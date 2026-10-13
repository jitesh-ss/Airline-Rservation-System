#include "crow_all.h"
#include <sqlite3.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <algorithm>

struct Edge {
    std::string to_airport;
    std::string flight_id;
    double price;
};

struct RouteResult {
    double total_cost;
    std::vector<std::string> path;
    std::vector<std::string> flight_ids;
};

class FlightGraph {
public:
    std::unordered_map<std::string, std::vector<Edge>> adj;

    void addFlight(const std::string& from, const std::string& to, const std::string& id, double price) {
        adj[from].push_back({to, id, price});
    }

    RouteResult findCheapestRoute(const std::string& start, const std::string& target) {
        using CostNode = std::pair<double, std::string>;
        std::priority_queue<CostNode, std::vector<CostNode>, std::greater<CostNode>> pq;
        std::unordered_map<std::string, double> dist;
        std::unordered_map<std::string, std::pair<std::string, std::string>> parent;

        pq.push({0.0, start});
        dist[start] = 0.0;

        while (!pq.empty()) {
            auto [cost, u] = pq.top();
            pq.pop();

            if (u == target) break;
            if (cost > dist[u]) continue;

            for (const auto& edge : adj[u]) {
                double new_cost = cost + edge.price;
                if (!dist.count(edge.to_airport) || new_cost < dist[edge.to_airport]) {
                    dist[edge.to_airport] = new_cost;
                    parent[edge.to_airport] = {u, edge.flight_id};
                    pq.push({new_cost, edge.to_airport});
                }
            }
        }

        RouteResult res{dist.count(target) ? dist[target] : -1.0, {}, {}};
        if (res.total_cost < 0) return res;

        std::string curr = target;
        while (curr != start) {
            res.path.push_back(curr);
            res.flight_ids.push_back(parent[curr].second);
            curr = parent[curr].first;
        }
        res.path.push_back(start);
        std::reverse(res.path.begin(), res.path.end());
        std::reverse(res.flight_ids.begin(), res.flight_ids.end());
        return res;
    }
};

void initDatabase(sqlite3* db) {
    const char* schema = 
        "CREATE TABLE IF NOT EXISTS bookings ("
        "  booking_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  flight_id TEXT NOT NULL,"
        "  passenger_name TEXT NOT NULL,"
        "  seat_number TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE(flight_id, seat_number)"
        ");";
    char* err = nullptr;
    sqlite3_exec(db, schema, nullptr, nullptr, &err);
    if (err) {
        std::cerr << "SQLite error: " << err << "\n";
        sqlite3_free(err);
    }
}

int main() {
    sqlite3* db;
    if (sqlite3_open("airline.db", &db)) {
        std::cerr << "Failed to open SQLite database.\n";
        return 1;
    }
    initDatabase(db);

    crow::SimpleApp app;
    FlightGraph graph;

    graph.addFlight("DEL", "BOM", "AI101", 4500.0);
    graph.addFlight("BOM", "BLR", "6E202", 3200.0);
    graph.addFlight("DEL", "BLR", "UK808", 8500.0);

    // 1. Serve Frontend
    CROW_ROUTE(app, "/")([](const crow::request&, crow::response& res) {
        res.set_static_file_info("public/index.html");
        res.end();
    });

    // 2. Health Check
    CROW_ROUTE(app, "/api/health")([]() {
        return crow::response(200, "{\"status\": \"running\"}");
    });

    // 3. Dijkstra Route Optimization
    CROW_ROUTE(app, "/api/flights/optimize")
    ([&graph](const crow::request& req) {
        auto from = req.url_params.get("from");
        auto to = req.url_params.get("to");

        if (!from || !to) {
            return crow::response(400, "{\"error\": \"Provide both 'from' and 'to' parameters.\"}");
        }

        RouteResult res = graph.findCheapestRoute(from, to);
        if (res.total_cost < 0) {
            return crow::response(404, "{\"error\": \"No route found.\"}");
        }

        crow::json::wvalue resp;
        resp["total_cost"] = res.total_cost;
        for (size_t i = 0; i < res.path.size(); ++i) resp["path"][i] = res.path[i];
        for (size_t i = 0; i < res.flight_ids.size(); ++i) resp["flights"][i] = res.flight_ids[i];
        return crow::response(200, resp);
    });

    // 4. Get Occupied Seats for a Flight
    CROW_ROUTE(app, "/api/flights/<string>/seats")
    ([db](std::string flight_id) {
        std::string query = "SELECT seat_number FROM bookings WHERE flight_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "{\"error\": \"Failed to query seats.\"}");
        }

        sqlite3_bind_text(stmt, 1, flight_id.c_str(), -1, SQLITE_STATIC);

        std::vector<std::string> occupied;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* seat = sqlite3_column_text(stmt, 0);
            if (seat) occupied.push_back(reinterpret_cast<const char*>(seat));
        }
        sqlite3_finalize(stmt);

        crow::json::wvalue resp = crow::json::wvalue::empty_object();
        resp["occupied_seats"] = crow::json::wvalue::list{};
        for (size_t i = 0; i < occupied.size(); ++i) {
            resp["occupied_seats"][i] = occupied[i];
        }
        return crow::response(200, resp);
    });

    // 5. Book Seat with Collision Handling (Safe Prepared Statements)
    CROW_ROUTE(app, "/api/book").methods(crow::HTTPMethod::POST)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("flight_id") || !body.has("passenger_name") || !body.has("seat_number")) {
            return crow::response(400, "{\"error\": \"Invalid JSON payload.\"}");
        }

        std::string flight = body["flight_id"].s();
        std::string name = body["passenger_name"].s();
        std::string seat = body["seat_number"].s();

        std::string insert_query = "INSERT INTO bookings (flight_id, passenger_name, seat_number) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "{\"error\": \"Database error.\"}");
        }

        sqlite3_bind_text(stmt, 1, flight.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, seat.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_CONSTRAINT) {
            return crow::response(409, "{\"error\": \"Seat " + seat + " is already booked on flight " + flight + "!\"}");
        } else if (rc != SQLITE_DONE) {
            return crow::response(500, "{\"error\": \"Failed to complete transaction.\"}");
        }

        crow::json::wvalue resp;
        resp["status"] = "CONFIRMED";
        resp["flight_id"] = flight;
        resp["passenger"] = name;
        resp["seat"] = seat;
        return crow::response(201, resp);
    });

    std::cout << "Starting server on port 18080...\n";
    app.port(18080).multithreaded().run();

    sqlite3_close(db);
    return 0;
}