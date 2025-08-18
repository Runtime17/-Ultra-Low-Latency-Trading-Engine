#include "mpmc_ring.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

int main() {
  ull::MPMCRing<int> q(1024);

  // 2 producers, 1 consumer
  std::atomic<bool> done{false};
  std::atomic<size_t> produced{0}, consumed{0};

  auto producer = [&](int base) {
    for (int i = 0; i < 50000; ++i) {
      int v = base + i;
      while (!q.try_enqueue(v)) {
        std::this_thread::yield();
      }
      ++produced;
    }
  };

  auto consumer = [&]() {
    int v;
    size_t local = 0;
    while (!done.load(std::memory_order_relaxed) || !q.empty()) {
      if (q.try_dequeue(v)) {
        // do something "real": accumulate and print occasionally
        if (++local % 25000 == 0) {
          std::cout << "last=" << v << " total=" << local << "\n";
        }
        ++consumed;
      } else {
        std::this_thread::yield();
      }
    }
    std::cout << "consumer finished: " << local << "\n";
  };

  std::thread t1(producer, 0);
  std::thread t2(producer, 1000000);
  std::thread tc(consumer);

  t1.join();
  t2.join();
  done.store(true, std::memory_order_relaxed);
  tc.join();

  std::cout << "produced=" << produced.load() << " consumed=" << consumed.load() << "\n";
  return 0;
}
