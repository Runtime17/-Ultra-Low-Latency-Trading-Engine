#pragma once
#include <map>
#include <optional>
#include <cstdio>

namespace ull {

enum class Side { Bid, Ask };

struct Level {
    double price{};
    int    qty{};
};

class OrderBook {
    // Price -> quantity
    // std::map is ascending: lowest price first.
    // For bids we read from rbegin() to get the highest.
    std::map<double, int> bids_;
    std::map<double, int> asks_;

public:
    /// Add (or remove with negative qty) liquidity at price.
    /// If quantity at that price becomes <= 0, the level is erased.
    void add(Side s, double px, int q) {
        auto& book = (s == Side::Bid) ? bids_ : asks_;
        auto it = book.lower_bound(px);
        if (it == book.end() || it->first != px) {
            if (q > 0) book.emplace(px, q);
        } else {
            it->second += q;
            if (it->second <= 0) book.erase(it);
        }
    }

    /// Match an incoming (taker) order up to quantity q at price px.
    /// For taker=Bid: consume best asks <= px. For taker=Ask: consume best bids >= px.
    /// Returns filled quantity.
    int match(Side taker, double px, int q) {
        int filled = 0;

        if (q <= 0) return 0;

        if (taker == Side::Bid) {
            // Consume from the ask side: lowest asks first while ask <= px
            while (q > 0 && !asks_.empty()) {
                auto it = asks_.begin();             // best ask
                if (it->first > px) break;           // not marketable
                int take = (q < it->second) ? q : it->second;
                q      -= take;
                filled += take;
                it->second -= take;
                if (it->second == 0) asks_.erase(it);
            }
        } else {
            // Consume from the bid side: highest bids first while bid >= px
            while (q > 0 && !bids_.empty()) {
                auto it = std::prev(bids_.end());    // best bid
                if (it->first < px) break;           // not marketable
                int take = (q < it->second) ? q : it->second;
                q      -= take;
                filled += take;
                it->second -= take;
                if (it->second == 0) {
                    bids_.erase(it);
                }
            }
        }
        return filled;
    }

    /// Best level on a side, if present.
    std::optional<Level> best(Side s) const {
        if (s == Side::Bid) {
            if (bids_.empty()) return std::nullopt;
            const auto& it = *std::prev(bids_.end());   // highest price
            return Level{it.first, it.second};
        } else {
            if (asks_.empty()) return std::nullopt;
            const auto& it = *asks_.begin();            // lowest price
            return Level{it.first, it.second};
        }
    }

    /// Best bid/ask spread (ask - bid), if both sides exist.
    std::optional<double> spread() const {
        if (bids_.empty() || asks_.empty()) return std::nullopt;
        double bb = std::prev(bids_.end())->first;
        double ba = asks_.begin()->first;
        return ba - bb;
    }

    /// Mid price = best bid + spread/2, if both sides exist.
    std::optional<double> mid() const {
        if (bids_.empty() || asks_.empty()) return std::nullopt;
        double bb = std::prev(bids_.end())->first;
        double ba = asks_.begin()->first;
        return (bb + ba) * 0.5;
    }

    bool empty() const { return bids_.empty() && asks_.empty(); }
    void clear()       { bids_.clear(); asks_.clear(); }

    /// Print a simple snapshot to a FILE* (defaults to stdout).
    void snapshot(FILE* out = stdout, int depth = 5) const {
        std::fprintf(out, "--BIDS--\n");
        int printed = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && printed < depth; ++it, ++printed)
            std::fprintf(out, "%0.2f x %d\n", it->first, it->second);

        std::fprintf(out, "--ASKS--\n");
        printed = 0;
        for (auto it = asks_.begin(); it != asks_.end() && printed < depth; ++it, ++printed)
            std::fprintf(out, "%0.2f x %d\n", it->first, it->second);
    }
};

} // namespace ull
