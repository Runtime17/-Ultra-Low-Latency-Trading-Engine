#pragma once
#include <string>
#include <cstdint>
namespace ull {
struct NewOrder{ std::string sym; double px; int qty; bool buy; };
struct Fill{ std::string clOrdId; double px; int qty; uint64_t ts_ns; };
class IOrderRouter{ public: virtual ~IOrderRouter()=default; virtual std::string send(const NewOrder&)=0; virtual bool pollFill(Fill&)=0; };
}