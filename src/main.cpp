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

// --- CORS Middleware: Crow'un otomatik OPTIONS(204) cevabına bile header ekler ---
struct Cors {
    struct context {
        std::string origin;
    };

    void before_handle(crow::request& req, crow::response& /*res*/, context& ctx) {
        ctx.origin = req.get_header_value("Origin");
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        std::string origin = ctx.origin;
        if (origin.empty()) origin = req.get_header_value("Origin");
    
        const std::string dev_origin = "http://127.0.0.1:5500";
        if (!origin.empty()) res.add_header("Access-Control-Allow-Origin", origin);
        else res.add_header("Access-Control-Allow-Origin", dev_origin);
    
        res.add_header("Vary", "Origin");
        res.add_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        res.add_header("Access-Control-Max-Age", "86400");
    }
};

int main() {
    crow::App<Cors> app;

    CROW_ROUTE(app, "/health").methods("GET"_method)
    ([]{
        return crow::response{200, "{\"ok\":true}"};
    });

    CROW_ROUTE(app, "/reservations").methods("POST"_method)
    ([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body) return crow::response{400, "{\"error\":\"invalid_json\"}"};

        auto get = [&](const char* k)->std::string{
            if (!body.has(k)) return "";
            return body[k].s();
        };

        std::string name   = get("name");
        std::string phone  = get("phone");
        std::string date   = get("date");
        std::string time   = get("time");
        std::string people = get("people");
        std::string note   = get("note");

        if (name.empty() || phone.empty() || date.empty() || time.empty() || people.empty())
            return crow::response{400, "{\"error\":\"missing_fields\"}"};

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

        return crow::response{200, "{\"ok\":true}"};
    });

    int port = 8080;
    if (const char* p = std::getenv("PORT")) port = std::atoi(p);

    app.port(port).multithreaded().run();
    return 0;
}