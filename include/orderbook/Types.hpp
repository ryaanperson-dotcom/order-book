#pragma once

#include <cstdint>
#include <string>

namespace ob {

// Prices are stored as integer "ticks" (e.g. cents), never floating point.
// Floats can't represent 0.1 exactly, which breaks price equality checks.
using Price    = std::int64_t;
using Quantity = std::uint64_t;
using OrderId  = std::uint64_t;

enum class Side { Buy, Sell };

enum class OrderType {
    Limit,   // rest any unfilled quantity on the book (good-till-cancel)
    Market,  // take whatever liquidity exists at any price, never rests
    IOC,     // immediate-or-cancel: limit price, fill what you can, cancel the rest
    FOK      // fill-or-kill: limit price, fill completely right now or do nothing
};

struct Order {
    OrderId   id;
    Side      side;
    OrderType type;
    Price     price;     // ignored for Market orders
    Quantity  quantity;  // remaining (unfilled) quantity
};

struct Trade {
    OrderId  buyId;
    OrderId  sellId;
    Price    price;      // always the resting (maker) order's price
    Quantity quantity;
};

inline const char* toString(Side s) { return s == Side::Buy ? "BUY" : "SELL"; }

}  // namespace ob
