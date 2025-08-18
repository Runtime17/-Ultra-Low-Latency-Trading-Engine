#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include "interfaces/IMarketDataFeed.hpp"
namespace ull {
class CsvReplayFeed: public IMarketDataFeed{
  std::ifstream fin;
public:
  explicit CsvReplayFeed(const std::string& path): fin(path){ std::string h; std::getline(fin,h);}
  bool next(Quote& q) override {
    if(!fin) return false; std::string line; if(!std::getline(fin,line)) return false;
    std::istringstream ss(line); std::string ts,sym,bid,ask,bSz,aSz;
    std::getline(ss,ts,','); std::getline(ss,sym,','); std::getline(ss,bid,','); std::getline(ss,ask,','); std::getline(ss,bSz,','); std::getline(ss,aSz,',');
    q.ts_ns=std::stoull(ts); q.sym=sym; q.bid=std::stod(bid); q.ask=std::stod(ask); q.bidSz=std::stoi(bSz); q.askSz=std::stoi(aSz);
    return true; }
};
}