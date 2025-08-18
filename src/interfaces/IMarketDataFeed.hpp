#pragma once
#include <string>
#include <cstdint>
namespace ull {
struct Quote{ std::string sym; double bid; double ask; int bidSz; int askSz; uint64_t ts_ns; };
class IMarketDataFeed{ public: virtual ~IMarketDataFeed()=default; virtual bool next(Quote&)=0; };
}