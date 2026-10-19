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

    CROW_ROUTE(app, "/hero.jpg")([](const crow::request&, crow::response& res) {
        res.set_static_file_info("public/hero.jpg");
        res.end();
    });

    // 2. Health Check
    CROW_ROUTE(app, "/api/health")([]() {
        crow::response res(200, "{\"status\": \"running\"}");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 2b. Flight Catalog
    CROW_ROUTE(app, "/api/flights")
    ([]() {
        crow::json::wvalue resp = crow::json::wvalue::list();
        
        resp[0]["flight_id"] = "AI101";
        resp[0]["airline"] = "Air India";
        resp[0]["airline_code"] = "AI";
        resp[0]["from"] = "DEL";
        resp[0]["from_name"] = "New Delhi (DEL)";
        resp[0]["to"] = "BOM";
        resp[0]["to_name"] = "Mumbai (BOM)";
        resp[0]["price"] = 4500.0;
        resp[0]["departure"] = "06:00 AM";
        resp[0]["arrival"] = "08:15 AM";
        resp[0]["duration"] = "2h 15m";
        resp[0]["aircraft"] = "Airbus A320neo";

        resp[1]["flight_id"] = "6E202";
        resp[1]["airline"] = "IndiGo";
        resp[1]["airline_code"] = "6E";
        resp[1]["from"] = "BOM";
        resp[1]["from_name"] = "Mumbai (BOM)";
        resp[1]["to"] = "BLR";
        resp[1]["to_name"] = "Bengaluru (BLR)";
        resp[1]["price"] = 3200.0;
        resp[1]["departure"] = "10:30 AM";
        resp[1]["arrival"] = "12:15 PM";
        resp[1]["duration"] = "1h 45m";
        resp[1]["aircraft"] = "Airbus A321neo";

        resp[2]["flight_id"] = "UK808";
        resp[2]["airline"] = "Vistara";
        resp[2]["airline_code"] = "UK";
        resp[2]["from"] = "DEL";
        resp[2]["from_name"] = "New Delhi (DEL)";
        resp[2]["to"] = "BLR";
        resp[2]["to_name"] = "Bengaluru (BLR)";
        resp[2]["price"] = 8500.0;
        resp[2]["departure"] = "07:15 AM";
        resp[2]["arrival"] = "10:05 AM";
        resp[2]["duration"] = "2h 50m";
        resp[2]["aircraft"] = "Boeing 787-9 Dreamliner";

        crow::response res(200, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 3. Dijkstra Route Optimization
    CROW_ROUTE(app, "/api/flights/optimize")
    ([&graph](const crow::request& req) {
        auto from = req.url_params.get("from");
        auto to = req.url_params.get("to");

        if (!from || !to) {
            crow::response res(400, "{\"error\": \"Provide both 'from' and 'to' parameters.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        RouteResult res_route = graph.findCheapestRoute(from, to);
        if (res_route.total_cost < 0) {
            crow::response res(404, "{\"error\": \"No route found.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        crow::json::wvalue resp;
        resp["total_cost"] = res_route.total_cost;
        for (size_t i = 0; i < res_route.path.size(); ++i) resp["path"][i] = res_route.path[i];
        for (size_t i = 0; i < res_route.flight_ids.size(); ++i) resp["flights"][i] = res_route.flight_ids[i];
        
        crow::response res(200, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 4. Get Occupied Seats for a Flight
    CROW_ROUTE(app, "/api/flights/<string>/seats")
    ([db](std::string flight_id) {
        std::string query = "SELECT seat_number FROM bookings WHERE flight_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            crow::response res(500, "{\"error\": \"Failed to query seats.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
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
        crow::response res(200, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 4b. Get All Bookings
    CROW_ROUTE(app, "/api/bookings")
    ([db]() {
        std::string query = "SELECT booking_id, flight_id, passenger_name, seat_number, created_at FROM bookings ORDER BY booking_id DESC LIMIT 100;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            crow::response res(500, "{\"error\": \"Failed to query bookings.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        crow::json::wvalue resp = crow::json::wvalue::list();
        int idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            resp[idx]["booking_id"] = sqlite3_column_int(stmt, 0);
            const unsigned char* f = sqlite3_column_text(stmt, 1);
            const unsigned char* p = sqlite3_column_text(stmt, 2);
            const unsigned char* s = sqlite3_column_text(stmt, 3);
            const unsigned char* c = sqlite3_column_text(stmt, 4);
            resp[idx]["flight_id"] = f ? reinterpret_cast<const char*>(f) : "";
            resp[idx]["passenger_name"] = p ? reinterpret_cast<const char*>(p) : "";
            resp[idx]["seat_number"] = s ? reinterpret_cast<const char*>(s) : "";
            resp[idx]["created_at"] = c ? reinterpret_cast<const char*>(c) : "";
            idx++;
        }
        sqlite3_finalize(stmt);

        crow::response res(200, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 4c. Cancel Booking
    CROW_ROUTE(app, "/api/bookings/<int>").methods(crow::HTTPMethod::Delete)
    ([db](int booking_id) {
        std::string del_query = "DELETE FROM bookings WHERE booking_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, del_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            crow::response res(500, "{\"error\": \"Database error.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        sqlite3_bind_int(stmt, 1, booking_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            crow::response res(500, "{\"error\": \"Failed to delete booking.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        crow::json::wvalue resp;
        resp["status"] = "CANCELLED";
        resp["booking_id"] = booking_id;
        crow::response res(200, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // 5. Book Seat with Collision Handling (Safe Prepared Statements)
    CROW_ROUTE(app, "/api/book").methods(crow::HTTPMethod::Post)
    ([db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("flight_id") || !body.has("passenger_name") || !body.has("seat_number")) {
            crow::response res(400, "{\"error\": \"Invalid JSON payload.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        std::string flight = body["flight_id"].s();
        std::string name = body["passenger_name"].s();
        std::string seat = body["seat_number"].s();

        std::string insert_query = "INSERT INTO bookings (flight_id, passenger_name, seat_number) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            crow::response res(500, "{\"error\": \"Database error.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        sqlite3_bind_text(stmt, 1, flight.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, seat.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_CONSTRAINT) {
            crow::response res(409, "{\"error\": \"Seat " + seat + " is already booked on flight " + flight + "!\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        } else if (rc != SQLITE_DONE) {
            crow::response res(500, "{\"error\": \"Failed to complete transaction.\"}");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        crow::json::wvalue resp;
        resp["status"] = "CONFIRMED";
        resp["flight_id"] = flight;
        resp["passenger"] = name;
        resp["seat"] = seat;
        crow::response res(201, resp);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    std::cout << "Starting server on port 18080...\n";
    app.loglevel(crow::LogLevel::Debug);
    app.port(18080).run();

    sqlite3_close(db);
    return 0;
}