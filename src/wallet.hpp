#pragma once
#include <unordered_map>
#include <string>
#include <iostream>

namespace ull {

struct Position {
    std::string sym;
    int qty = 0;
    double avg = 0.0;
};

class Wallet {
    double cash_{0.0};
    std::unordered_map<std::string, Position> pos_;

public:
    explicit Wallet(double c = 0.0) : cash_(c) {}

    double cash() const { return cash_; }

    /// Update wallet on a trade fill.
    /// @param sym  Symbol traded
    /// @param buy  true if buy, false if sell
    /// @param q    Quantity
    /// @param px   Execution price
    void on_fill(const std::string& sym, bool buy, int q, double px) {
        auto& p = pos_[sym];
        p.sym = sym;

        if (buy) {
            cash_ -= q * px;
            double notional = p.avg * p.qty + px * q;
            p.qty += q;
            p.avg = (p.qty ? notional / p.qty : 0.0);
        } else {
            cash_ += q * px;
            p.qty -= q;
            if (!p.qty) p.avg = 0.0;
        }
    }

    void print() const {
        std::cout << "Cash: " << cash_ << "\n";
        for (auto& kv : pos_)
            std::cout << kv.first
                      << " qty=" << kv.second.qty
                      << " avg=" << kv.second.avg << "\n";
    }
};

} // namespace ull
