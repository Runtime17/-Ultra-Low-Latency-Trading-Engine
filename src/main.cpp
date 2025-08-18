#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <deque>
#include <fstream>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;

using tcp = asio::ip::tcp;

static bool extract_bid_ask(const std::string& payload, double& bid, double& ask) {
    auto find_value = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":\"";
        auto pos = payload.find(pattern);
        if (pos == std::string::npos) return {};
        pos += pattern.size();
        auto end = payload.find('\"', pos);
        if (end == std::string::npos) return {};
        return payload.substr(pos, end - pos);
    };

    std::string b = find_value("b");
    std::string a = find_value("a");
    if (b.empty() || a.empty()) return false;

    try {
        bid = std::stod(b);
        ask = std::stod(a);
        return true;
    } catch (...) {
        return false;
    }
}

static std::string iso8601_now_utc() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    std::time_t t = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

int main(int argc, char** argv) {
    // Args: [symbol] [window] [csv_path]
    std::string symbol = (argc > 1) ? argv[1] : "btcusdt";
    int window = (argc > 2) ? std::max(1, std::atoi(argv[2])) : 100;
    std::string csv_path = (argc > 3) ? argv[3] : "spreads.csv";

    std::string host   = "stream.binance.com";
    std::string port   = "9443";
    std::string target = "/ws/" + symbol + "@bookTicker";

    std::ofstream csv(csv_path, std::ios::app);
    if (!csv) {
        std::cerr << "Failed to open CSV file: " << csv_path << "\n";
        return 1;
    }
    // Write header if file empty
    if (csv.tellp() == 0) {
        csv << "timestamp,symbol,bid,ask,spread,mid,bps,roll_mean_spread\n";
    }

    std::deque<double> window_vals;
    double rolling_sum = 0.0;

    try {
        asio::io_context ioc;
        asio::ssl::context ctx(asio::ssl::context::tls_client);
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(asio::ssl::verify_peer);

        for (;;) {
            beast::error_code ec;
            tcp::resolver resolver(ioc);
            auto const results = resolver.resolve(host, port, ec);
            if (ec) {
                std::cerr << "Resolve error: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            beast::ssl_stream<beast::tcp_stream> ssl_stream(ioc, ctx);
            if(!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str())) {
                beast::error_code ec2{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
                std::cerr << "SNI error: " << ec2.message() << "\n";
            }

            beast::get_lowest_layer(ssl_stream).connect(results, ec);
            if (ec) {
                std::cerr << "Connect error: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            ssl_stream.handshake(asio::ssl::stream_base::client, ec);
            if (ec) {
                std::cerr << "TLS handshake error: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            ws::stream<beast::ssl_stream<beast::tcp_stream>> ws_stream(std::move(ssl_stream));
            ws_stream.set_option(ws::stream_base::timeout::suggested(beast::role_type::client));
            ws_stream.set_option(ws::stream_base::decorator(
                [](ws::request_type& req) {
                    req.set(http::field::user_agent, std::string("live-spread-cpp/2.0"));
                }
            ));

            ws_stream.handshake(host, target, ec);
            if (ec) {
                std::cerr << "WS handshake error: " << ec.message() << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            std::cout << "Connected to wss://" << host << ":" << port << target
                      << " | window=" << window
                      << " | csv=" << csv_path << "\n";

            beast::flat_buffer buffer;
            for (;;) {
                buffer.clear();
                ws_stream.read(buffer, ec);
                if (ec) {
                    std::cerr << "WS read error: " << ec.message() << " (reconnecting)\n";
                    break;
                }

                std::string payload = beast::buffers_to_string(buffer.data());
                double bid = 0.0, ask = 0.0;
                if (extract_bid_ask(payload, bid, ask)) {
                    double spread = ask - bid;
                    double mid    = (ask + bid) / 2.0;
                    double bps    = (mid > 0.0) ? (spread / mid) * 10000.0 : 0.0;

                    // Update rolling window
                    rolling_sum += spread;
                    window_vals.push_back(spread);
                    if ((int)window_vals.size() > window) {
                        rolling_sum -= window_vals.front();
                        window_vals.pop_front();
                    }
                    double roll_mean = rolling_sum / static_cast<double>(window_vals.size());

                    // Log to console
                    std::cout << std::fixed << std::setprecision(2)
                              << "bid=" << bid
                              << " ask=" << ask
                              << " spread=" << spread
                              << " roll_mean=" << roll_mean
                              << " (" << bps << " bps)"
                              << std::endl;

                    // Write to CSV
                    csv << iso8601_now_utc() << ","
                        << symbol << ","
                        << std::setprecision(10) << bid << ","
                        << std::setprecision(10) << ask << ","
                        << std::setprecision(10) << spread << ","
                        << std::setprecision(10) << mid << ","
                        << std::setprecision(6) << bps << ","
                        << std::setprecision(10) << roll_mean
                        << "\n";
                    csv.flush();
                }
            }

            beast::error_code ec_close;
            ws_stream.close(ws::close_code::normal, ec_close);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
