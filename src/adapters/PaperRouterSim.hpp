#pragma once
#include <queue>
#include <chrono>
#include "interfaces/IOrderRouter.hpp"
namespace ull {
class PaperRouterSim: public IOrderRouter{
  std::queue<Fill> q_; uint64_t seq_=0;
  static uint64_t nowNs(){ using namespace std::chrono; return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count(); }
public:
  std::string send(const NewOrder& o) override { auto id = std::string("C") + std::to_string(++seq_); q_.push(Fill{id,o.px,o.qty,nowNs()}); return id; }
  bool pollFill(Fill& f) override { if(q_.empty()) return false; f=q_.front(); q_.pop(); return true; }
};
}