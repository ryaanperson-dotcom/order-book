// Minimal self-contained test runner (no external dependencies).
#include "orderbook/OrderBook.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ob;

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static Order limit(OrderId id, Side s, Price p, Quantity q) { return {id, s, OrderType::Limit, p, q}; }

static void test_resting_orders_set_best_prices() {
    OrderBook b;
    b.submit(limit(1, Side::Buy, 9950, 100));
    b.submit(limit(2, Side::Buy, 9960, 100));
    b.submit(limit(3, Side::Sell, 10010, 100));
    b.submit(limit(4, Side::Sell, 10000, 100));
    CHECK(b.bestBid() == 9960);
    CHECK(b.bestAsk() == 10000);
    CHECK(b.spread() == 40);
    CHECK(b.orderCount() == 4);
}

static void test_crossing_order_trades_at_resting_price() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10000, 100));
    auto trades = b.submit(limit(2, Side::Buy, 10050, 100));  // willing to pay more
    CHECK(trades.size() == 1);
    CHECK(trades[0].price == 10000);  // maker's price, not taker's
    CHECK(trades[0].quantity == 100);
    CHECK(b.orderCount() == 0);
}

static void test_time_priority_within_level() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10000, 50));  // first in queue
    b.submit(limit(2, Side::Sell, 10000, 50));
    auto trades = b.submit(limit(3, Side::Buy, 10000, 50));
    CHECK(trades.size() == 1);
    CHECK(trades[0].sellId == 1);  // oldest order filled first
    CHECK(b.contains(2));
}

static void test_price_priority_across_levels() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10020, 100));
    b.submit(limit(2, Side::Sell, 10000, 100));  // better price, arrived later
    auto trades = b.submit(limit(3, Side::Buy, 10020, 100));
    CHECK(trades.size() == 1);
    CHECK(trades[0].sellId == 2);
}

static void test_sweep_multiple_levels_and_rest_remainder() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10000, 30));
    b.submit(limit(2, Side::Sell, 10010, 30));
    b.submit(limit(3, Side::Sell, 10020, 30));
    auto trades = b.submit(limit(4, Side::Buy, 10010, 100));
    CHECK(trades.size() == 2);
    CHECK(b.bestAsk() == 10020);
    CHECK(b.bestBid() == 10010);
    CHECK(b.volumeAt(Side::Buy, 10010) == 40);  // 100 - 30 - 30 rests
}

static void test_market_order_never_rests() {
    OrderBook b;
    b.submit(limit(1, Side::Buy, 9990, 50));
    auto trades = b.submit({2, Side::Sell, OrderType::Market, 0, 80});
    CHECK(trades.size() == 1);
    CHECK(trades[0].quantity == 50);
    CHECK(b.orderCount() == 0);  // leftover 30 discarded
}

static void test_ioc_cancels_remainder() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10000, 40));
    auto trades = b.submit({2, Side::Buy, OrderType::IOC, 10000, 100});
    CHECK(trades.size() == 1);
    CHECK(!b.contains(2));
    CHECK(!b.bestBid().has_value());
}

static void test_fok_all_or_nothing() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 10000, 40));
    auto none = b.submit({2, Side::Buy, OrderType::FOK, 10000, 100});
    CHECK(none.empty());
    CHECK(b.volumeAt(Side::Sell, 10000) == 40);  // book untouched

    auto all = b.submit({3, Side::Buy, OrderType::FOK, 10000, 40});
    CHECK(all.size() == 1);
    CHECK(b.orderCount() == 0);
}

static void test_cancel_and_reduce() {
    OrderBook b;
    b.submit(limit(1, Side::Buy, 9900, 100));
    b.submit(limit(2, Side::Buy, 9900, 100));
    CHECK(b.reduce(1, 60));
    CHECK(b.volumeAt(Side::Buy, 9900) == 160);
    CHECK(b.cancel(2));
    CHECK(!b.cancel(2));  // already gone
    CHECK(b.cancel(1));
    CHECK(!b.bestBid().has_value());  // empty level removed
}

static void test_rejects_bad_input() {
    OrderBook b;
    bool threw = false;
    try { b.submit(limit(1, Side::Buy, 100, 0)); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    b.submit(limit(2, Side::Buy, 100, 10));
    threw = false;
    try { b.submit(limit(2, Side::Buy, 100, 10)); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"resting orders set best prices", test_resting_orders_set_best_prices},
        {"crossing order trades at resting price", test_crossing_order_trades_at_resting_price},
        {"time priority within a level", test_time_priority_within_level},
        {"price priority across levels", test_price_priority_across_levels},
        {"sweep levels and rest remainder", test_sweep_multiple_levels_and_rest_remainder},
        {"market order never rests", test_market_order_never_rests},
        {"IOC cancels remainder", test_ioc_cancels_remainder},
        {"FOK all or nothing", test_fok_all_or_nothing},
        {"cancel and reduce", test_cancel_and_reduce},
        {"rejects bad input", test_rejects_bad_input},
    };
    for (auto& [name, fn] : tests) {
        int before = failures;
        fn();
        std::cout << (failures == before ? "[PASS] " : "[FAIL] ") << name << "\n";
    }
    std::cout << "\n" << (failures ? "Some tests failed." : "All tests passed.") << "\n";
    return failures ? 1 : 0;
}
