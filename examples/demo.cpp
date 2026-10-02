// Walk-through of the engine: build a book, then hit it with an aggressive order.
#include "orderbook/OrderBook.hpp"

#include <iomanip>
#include <iostream>

using namespace ob;

int main() {
    OrderBook book;

    // Prices in cents: 10000 = $100.00
    book.submit({1, Side::Buy,  OrderType::Limit,  9990, 200});
    book.submit({2, Side::Buy,  OrderType::Limit,  9980, 300});
    book.submit({3, Side::Buy,  OrderType::Limit,  9990, 100});
    book.submit({4, Side::Sell, OrderType::Limit, 10010, 150});
    book.submit({5, Side::Sell, OrderType::Limit, 10020, 250});
    book.submit({6, Side::Sell, OrderType::Limit, 10010, 50});

    std::cout << "Initial book:\n";
    book.print();

    std::cout << "\nAggressive BUY 300 @ $100.20 arrives...\n";
    for (const auto& t : book.submit({7, Side::Buy, OrderType::Limit, 10020, 300}))
        std::cout << "  TRADE " << t.quantity << " @ $" << std::fixed << std::setprecision(2) << t.price / 100.0
                  << "  (buy #" << t.buyId << " / sell #" << t.sellId << ")\n";

    std::cout << "\nBook after:\n";
    book.print();
}
