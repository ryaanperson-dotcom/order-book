# Limit Order Book Matching Engine (C++17)

A limit order book of the kind every stock exchange runs at its core: it takes buy and sell orders, matches them by **price-time priority**, and produces trades.

## Features

- Order types: **Limit** (rests on the book), **Market**, **IOC** (immediate-or-cancel), **FOK** (fill-or-kill)
- Price-time priority: best price fills first, and among equal prices the oldest order fills first
- O(1) cancel and reduce by order ID
- Integer tick prices, so there are no floating point rounding errors
- 10 unit tests with no external dependencies
- Reproducible benchmark (fixed random seed)

## Benchmark

1,000,000 randomised events (about 72% limit, 25% cancel, 3% market), single thread, Release build:

| Metric      | Result          |
|-------------|-----------------|
| Throughput  | ~3.5M events/s  |
| Latency p50 | ~160 ns         |
| Latency p99 | ~870 ns         |

These numbers are from a Linux x86-64 machine with GCC 13. Your numbers will vary by machine.

## Design

| Component       | Structure                                | Why |
|-----------------|------------------------------------------|-----|
| Price level     | `std::list<Order>` (FIFO queue)          | Time priority, and erasing from the middle on cancel doesn't invalidate other iterators |
| Bid side        | `std::map<Price, Level, std::greater>`   | Sorted high to low, so the best bid is `begin()` |
| Ask side        | `std::map<Price, Level, std::less>`      | Sorted low to high, so the best ask is `begin()` |
| Order lookup    | `std::unordered_map<OrderId, Locator>`   | Jumps straight to an order for O(1) cancel |

Matching rules:

- A trade always executes at the **resting (maker) order's price**. A buyer willing to pay $100.50 who hits an ask at $100.00 pays $100.00.
- **FOK** checks the available liquidity *before* touching the book, so a failed FOK leaves the book unchanged.
- **Reduce** keeps the order's place in the queue. Increasing size isn't allowed, because a real exchange makes you lose priority for that (cancel and resubmit).

## Build and run

Requires CMake 3.16+ and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

./build/tests       # unit tests
./build/demo        # walk-through example
./build/benchmark   # performance test (optional arg: number of events)
```

On Windows with Visual Studio, the executables are in `build/Release/`.

## Example output (demo)

```
Initial book:
        PRICE      QTY
  ASK   100.20      250
  ASK   100.10      200
  ----------------------
  BID    99.90      300
  BID    99.80      300

Aggressive BUY 300 @ $100.20 arrives...
  TRADE 150 @ $100.10  (buy #7 / sell #4)
  TRADE 50 @ $100.10  (buy #7 / sell #6)
  TRADE 100 @ $100.20  (buy #7 / sell #5)
```

## Possible extensions

- Replace `std::map` with a flat array indexed by price tick for better cache performance
- Use a memory pool instead of per-order `std::list` allocations
- Add Python bindings (pybind11) to replay real market data
