#include "crow_all.h"
#include <fstream>
#include <mutex>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>

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

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

static std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                cur.push_back('"');
                i++;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

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

    CROW_ROUTE(app, "/admin/reservations").methods("GET"_method)
([](const crow::request& req) {
    const char* admin_key = std::getenv("ADMIN_KEY");
    std::string key = req.url_params.get("key") ? req.url_params.get("key") : "";

    if (!admin_key || key != admin_key) {
        crow::response r;
        r.code = 401;
        r.set_header("Content-Type", "application/json");
        r.write("{\"error\":\"unauthorized\"}");
        return r;
    }

    std::ifstream in(data_path());
    if (!in.good()) {
        crow::response r;
        r.code = 200;
        r.set_header("Content-Type", "application/json");
        r.write("[]");
        return r;
    }

    std::string header;
    std::getline(in, header); // ilk satır başlık

    std::string line;
    std::string json = "[";
    bool first = true;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto cols = parse_csv_line(line);
        if (cols.size() < 6) continue;

        if (!first) json += ",";
        first = false;

        json += "{";
        json += "\"name\":\""   + json_escape(cols[0]) + "\",";
        json += "\"phone\":\""  + json_escape(cols[1]) + "\",";
        json += "\"date\":\""   + json_escape(cols[2]) + "\",";
        json += "\"time\":\""   + json_escape(cols[3]) + "\",";
        json += "\"people\":\"" + json_escape(cols[4]) + "\",";
        json += "\"note\":\""   + json_escape(cols[5]) + "\"";
        json += "}";
    }

    json += "]";

    crow::response r;
    r.code = 200;
    r.set_header("Content-Type", "application/json");
    r.write(json);
    return r;
});

    int port = 8080;
    if (const char* p = std::getenv("PORT")) port = std::atoi(p);

    app.port(port).multithreaded().run();
    return 0;
}