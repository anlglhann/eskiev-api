#include "crow_all.h"
#include <fstream>
#include <mutex>
#include <string>
#include <cstdlib>

static std::mutex g_fileMutex;

static std::string data_path() {
    const char* p = std::getenv("DATA_PATH");
    return std::string(p ? p : "/var/data/reservations.csv");
}

static void ensure_csv_header() {
    std::lock_guard<std::mutex> lk(g_fileMutex);
    std::ifstream in(data_path());
    if (in.good()) return;

    std::ofstream out(data_path(), std::ios::out);
    out << "name,phone,date,time,people,note\n";
}

static std::string csv_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

int main() {
    crow::SimpleApp app;

    // CORS helpers
    auto add_cors_with_origin = [](const crow::request& req, crow::response& res) {
        std::string origin = req.get_header_value("Origin");
        res.add_header("Access-Control-Allow-Origin", origin.empty() ? "*" : origin);
        res.add_header("Vary", "Origin");
        res.add_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
    };

    auto add_cors_any = [](crow::response& res) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
    };

    // Health
    CROW_ROUTE(app, "/health").methods("GET"_method)
    ([&](const crow::request& req){
        crow::response res;
        add_cors_with_origin(req, res);
        res.code = 200;
        res.set_header("Content-Type", "application/json");
        res.write("{\"ok\":true}");
        return res; // res.end() YOK
    });
   
  

    // Preflight for reservations
    CROW_ROUTE(app, "/reservations").methods("OPTIONS"_method)
    ([&](const crow::request& req){
        crow::response res;
        add_cors_with_origin(req, res);
        res.code = 200;
        return res;
    });
   

    // Create reservation
    CROW_ROUTE(app, "/reservations").methods("POST"_method)
    ([&](const crow::request& req, crow::response& res){
        add_cors_with_origin(req, res);

        auto body = crow::json::load(req.body);
        if (!body) {
            res.code = 400;
            res.write("{\"error\":\"invalid_json\"}");
            return res.end();
        }

        auto get = [&](const char* k)->std::string{
            if (!body.has(k)) return "";
            return body[k].s();
        };

        std::string name = get("name");
        std::string phone = get("phone");
        std::string date = get("date");
        std::string time = get("time");
        std::string people = get("people");
        std::string note = get("note");

        if (name.empty() || phone.empty() || date.empty() || time.empty() || people.empty()) {
            res.code = 400;
            res.write("{\"error\":\"missing_fields\"}");
            return res.end();
        }

        ensure_csv_header();

        {
            std::lock_guard<std::mutex> lk(g_fileMutex);
            std::ofstream out(data_path(), std::ios::app);
            out << csv_escape(name) << ","
                << csv_escape(phone) << ","
                << csv_escape(date) << ","
                << csv_escape(time) << ","
                << csv_escape(people) << ","
                << csv_escape(note) << "\n";
        }

        res.code = 200;
        res.write("{\"ok\":true}");
        return res.end();
    });

    // If you ever need a generic OPTIONS handler:
    // (Not required, but shows how to use add_cors_any)
    CROW_ROUTE(app, "/").methods("OPTIONS"_method)
    ([&](crow::response& res){
        add_cors_any(res);
        res.code = 200;
        res.end();
    });

    int port = 8080;
    if (const char* p = std::getenv("PORT")) port = std::atoi(p);

    app.port(port).multithreaded().run();
    return 0;
}