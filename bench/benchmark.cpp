// Throughput + latency benchmark on a randomised order stream.
// The random seed is fixed so results are reproducible.
#include "orderbook/OrderBook.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace ob;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    const std::size_t N = argc > 1 ? std::stoul(argv[1]) : 1'000'000;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> action(0, 99);
    std::normal_distribution<double> priceNoise(0.0, 20.0);  // ticks around mid
    std::uniform_int_distribution<Quantity> qty(1, 500);

    // Pre-generate the stream so we only time the engine, not the RNG.
    struct Event { bool isCancel; Order order; };
    std::vector<Event> events;
    events.reserve(N);
    std::vector<OrderId> live;
    OrderId nextId = 1;
    const Price mid = 10000;

    for (std::size_t i = 0; i < N; ++i) {
        int a = action(rng);
        if (a < 25 && !live.empty()) {  // 25% cancels
            std::size_t k = rng() % live.size();
            events.push_back({true, {live[k], Side::Buy, OrderType::Limit, 0, 0}});
            live[k] = live.back();
            live.pop_back();
        } else {
            Side s = (a & 1) ? Side::Buy : Side::Sell;
            Price p = mid + static_cast<Price>(priceNoise(rng)) + (s == Side::Buy ? -5 : 5);
            OrderType t = a > 95 ? OrderType::Market : OrderType::Limit;
            events.push_back({false, {nextId, s, t, p, qty(rng)}});
            live.push_back(nextId++);
        }
    }

    OrderBook book;
    std::vector<double> latNs;
    latNs.reserve(N);
    std::size_t trades = 0;

    auto start = Clock::now();
    for (const auto& e : events) {
        auto t0 = Clock::now();
        if (e.isCancel) book.cancel(e.order.id);
        else            trades += book.submit(e.order).size();
        latNs.push_back(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
    }
    double secs = std::chrono::duration<double>(Clock::now() - start).count();

    std::sort(latNs.begin(), latNs.end());
    auto pct = [&](double p) { return latNs[static_cast<std::size_t>(p * (latNs.size() - 1))]; };

    std::cout << "Events processed : " << N << "\n"
              << "Trades generated : " << trades << "\n"
              << "Resting orders   : " << book.orderCount() << "\n"
              << "Total time       : " << secs << " s\n"
              << "Throughput       : " << static_cast<long long>(N / secs) << " events/s\n"
              << "Latency p50      : " << pct(0.50) << " ns\n"
              << "Latency p99      : " << pct(0.99) << " ns\n"
              << "Latency p99.9    : " << pct(0.999) << " ns\n";
}
