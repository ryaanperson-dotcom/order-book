#include "orderbook/OrderBook.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace ob {

bool OrderBook::crosses(const Order& incoming, Price restingPrice) {
    if (incoming.type == OrderType::Market) return true;
    return incoming.side == Side::Buy ? incoming.price >= restingPrice
                                      : incoming.price <= restingPrice;
}

template <typename Book>
Quantity OrderBook::availableLiquidity(const Order& incoming, const Book& opposite) const {
    Quantity total = 0;
    for (const auto& [price, level] : opposite) {
        if (!crosses(incoming, price)) break;
        for (const auto& o : level) {
            total += o.quantity;
            if (total >= incoming.quantity) return total;
        }
    }
    return total;
}

template <typename Book>
void OrderBook::match(Order& incoming, Book& opposite, std::vector<Trade>& trades) {
    while (incoming.quantity > 0 && !opposite.empty()) {
        auto levelIt = opposite.begin();  // best price on the other side
        if (!crosses(incoming, levelIt->first)) break;

        Level& level = levelIt->second;
        while (incoming.quantity > 0 && !level.empty()) {
            Order& resting = level.front();  // oldest order at this price
            Quantity fill = std::min(incoming.quantity, resting.quantity);

            Trade t{};
            t.buyId    = incoming.side == Side::Buy ? incoming.id : resting.id;
            t.sellId   = incoming.side == Side::Sell ? incoming.id : resting.id;
            t.price    = levelIt->first;
            t.quantity = fill;
            trades.push_back(t);

            incoming.quantity -= fill;
            resting.quantity -= fill;

            if (resting.quantity == 0) {
                index_.erase(resting.id);
                level.pop_front();
            }
        }
        if (level.empty()) opposite.erase(levelIt);
    }
}

std::vector<Trade> OrderBook::submit(const Order& order) {
    if (order.quantity == 0) throw std::invalid_argument("quantity must be positive");
    if (index_.count(order.id)) throw std::invalid_argument("duplicate order id");

    std::vector<Trade> trades;
    Order incoming = order;

    // Fill-or-kill: check there's enough liquidity before touching the book.
    if (incoming.type == OrderType::FOK) {
        Quantity avail = incoming.side == Side::Buy ? availableLiquidity(incoming, asks_)
                                                    : availableLiquidity(incoming, bids_);
        if (avail < incoming.quantity) return trades;
    }

    if (incoming.side == Side::Buy) match(incoming, asks_, trades);
    else                            match(incoming, bids_, trades);

    // Only plain limit orders rest. Market/IOC/FOK leftovers are discarded.
    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) rest(incoming);

    return trades;
}

void OrderBook::rest(const Order& order) {
    Level* level = order.side == Side::Buy ? &bids_[order.price] : &asks_[order.price];
    level->push_back(order);
    index_[order.id] = Locator{order.side, order.price, std::prev(level->end())};
}

bool OrderBook::cancel(OrderId id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;
    const Locator& loc = found->second;

    auto eraseFrom = [&](auto& book) {
        auto levelIt = book.find(loc.price);
        levelIt->second.erase(loc.it);
        if (levelIt->second.empty()) book.erase(levelIt);
    };
    if (loc.side == Side::Buy) eraseFrom(bids_);
    else                       eraseFrom(asks_);

    index_.erase(found);
    return true;
}

bool OrderBook::reduce(OrderId id, Quantity newQuantity) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;
    if (newQuantity == 0) return cancel(id);
    Order& o = *found->second.it;
    if (newQuantity >= o.quantity) return false;  // increases must lose priority: cancel + resubmit
    o.quantity = newQuantity;
    return true;
}

std::optional<Price> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    auto b = bestBid(), a = bestAsk();
    if (!b || !a) return std::nullopt;
    return *a - *b;
}

Quantity OrderBook::volumeAt(Side side, Price price) const {
    auto sum = [&](const auto& book) -> Quantity {
        auto it = book.find(price);
        if (it == book.end()) return 0;
        Quantity q = 0;
        for (const auto& o : it->second) q += o.quantity;
        return q;
    };
    return side == Side::Buy ? sum(bids_) : sum(asks_);
}

void OrderBook::print(std::size_t depth) const {
    auto fmt = [](Price p) {
        std::ostringstream s;
        s << std::fixed << std::setprecision(2) << static_cast<double>(p) / 100.0;
        return s.str();
    };

    std::vector<std::pair<Price, Quantity>> askLevels;
    for (auto it = asks_.begin(); it != asks_.end() && askLevels.size() < depth; ++it)
        askLevels.emplace_back(it->first, volumeAt(Side::Sell, it->first));

    std::cout << "        PRICE      QTY\n";
    for (auto it = askLevels.rbegin(); it != askLevels.rend(); ++it)
        std::cout << "  ASK " << std::setw(8) << fmt(it->first) << std::setw(9) << it->second << "\n";
    std::cout << "  ----------------------\n";
    std::size_t shown = 0;
    for (auto it = bids_.begin(); it != bids_.end() && shown < depth; ++it, ++shown)
        std::cout << "  BID " << std::setw(8) << fmt(it->first) << std::setw(9)
                  << volumeAt(Side::Buy, it->first) << "\n";
}

}  // namespace ob
