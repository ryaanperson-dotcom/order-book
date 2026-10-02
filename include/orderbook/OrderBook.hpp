#pragma once

#include "orderbook/Types.hpp"

#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ob {

// A limit order book with price-time priority matching.
//
// Data structure choices:
//   - Each price level is a FIFO queue (std::list) so the oldest order at a
//     price fills first (time priority).
//   - Levels live in a sorted std::map so the best price is always begin()
//     (price priority). Bids sort high->low, asks sort low->high.
//   - An unordered_map from OrderId to the order's exact position gives O(1)
//     cancels instead of scanning the book.
class OrderBook {
public:
    // Submit an order. Returns every trade it caused (possibly none).
    std::vector<Trade> submit(const Order& order);

    // Cancel a resting order. Returns false if the id isn't on the book.
    bool cancel(OrderId id);

    // Reduce a resting order's quantity (keeps its queue position).
    bool reduce(OrderId id, Quantity newQuantity);

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;
    std::optional<Price> spread() const;

    Quantity volumeAt(Side side, Price price) const;
    std::size_t orderCount() const { return index_.size(); }
    bool contains(OrderId id) const { return index_.count(id) > 0; }

    // Print the top `depth` levels of each side.
    void print(std::size_t depth = 5) const;

private:
    using Level = std::list<Order>;
    using Bids  = std::map<Price, Level, std::greater<Price>>;
    using Asks  = std::map<Price, Level, std::less<Price>>;

    struct Locator {
        Side side;
        Price price;
        Level::iterator it;
    };

    template <typename Book>
    void match(Order& incoming, Book& opposite, std::vector<Trade>& trades);

    template <typename Book>
    Quantity availableLiquidity(const Order& incoming, const Book& opposite) const;

    void rest(const Order& order);

    static bool crosses(const Order& incoming, Price restingPrice);

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Locator> index_;
};

}  // namespace ob
