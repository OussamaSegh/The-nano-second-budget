// llds/ordered/order_book.hpp
// Price-level order books for Chapters 14 and 15.
//
// Both books aggregate quantity per price level (a "market-by-price" book).
// Prices are integer ticks. Both are single-writer: one thread applies
// updates, and readers get a consistent top of book through a SeqLock.
//
//  * MapBook: std::map per side. Node-based, O(log n), pointer chasing on
//    every operation. The obvious design and the baseline.
//  * LadderBook: one flat array of quantities per side indexed by
//    (price - base). Updates are O(1) array writes; best price is tracked
//    incrementally and rescanned only when the best level empties. The
//    array is contiguous, so a rescan streams through cache lines the
//    hardware prefetcher has already fetched.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "../sync/seqlock.hpp"

namespace llds {

enum class Side : std::uint8_t { Bid, Ask };

struct TopOfBook {
    std::int64_t bid_px = 0;
    std::int64_t bid_qty = 0;
    std::int64_t ask_px = 0;
    std::int64_t ask_qty = 0;
    std::uint64_t seq = 0;  // update counter
};

class MapBook {
public:
    // Add (qty > 0) or remove (qty < 0) quantity at a price level.
    void apply(Side side, std::int64_t px, std::int64_t qty) {
        if (side == Side::Bid) adjust(bids_, px, qty);
        else adjust(asks_, px, qty);
        publish();
    }

    TopOfBook top() const { return top_.load(); }
    const SeqLock<TopOfBook>& top_seqlock() const { return top_; }

    std::int64_t qty_at(Side side, std::int64_t px) const {
        if (side == Side::Bid) {
            auto it = bids_.find(px);
            return it == bids_.end() ? 0 : it->second;
        }
        auto it = asks_.find(px);
        return it == asks_.end() ? 0 : it->second;
    }

    std::size_t levels(Side side) const { return side == Side::Bid ? bids_.size() : asks_.size(); }

private:
    template <class M>
    static void adjust(M& m, std::int64_t px, std::int64_t qty) {
        auto& q = m[px];
        q += qty;
        if (q <= 0) m.erase(px);
    }

    void publish() {
        TopOfBook t;
        if (!bids_.empty()) {
            t.bid_px = bids_.begin()->first;
            t.bid_qty = bids_.begin()->second;
        }
        if (!asks_.empty()) {
            t.ask_px = asks_.begin()->first;
            t.ask_qty = asks_.begin()->second;
        }
        t.seq = ++seq_;
        top_.store(t);
    }

    std::map<std::int64_t, std::int64_t, std::greater<>> bids_;  // best (highest) first
    std::map<std::int64_t, std::int64_t> asks_;                  // best (lowest) first
    SeqLock<TopOfBook> top_;
    std::uint64_t seq_ = 0;
};

class LadderBook {
public:
    // Prices must lie in [base, base + levels).
    LadderBook(std::int64_t base, std::size_t levels)
        : base_(base), bids_(levels, 0), asks_(levels, 0) {}

    void apply(Side side, std::int64_t px, std::int64_t qty) {
        const auto i = static_cast<std::size_t>(px - base_);
        if (side == Side::Bid) {
            auto& q = bids_[i];
            q += qty;
            if (q < 0) q = 0;
            if (q > 0 && (best_bid_ < 0 || static_cast<std::int64_t>(i) > best_bid_)) best_bid_ = static_cast<std::int64_t>(i);
            else if (q == 0 && static_cast<std::int64_t>(i) == best_bid_) rescan_bid(i);
        } else {
            auto& q = asks_[i];
            q += qty;
            if (q < 0) q = 0;
            if (q > 0 && (best_ask_ < 0 || static_cast<std::int64_t>(i) < best_ask_)) best_ask_ = static_cast<std::int64_t>(i);
            else if (q == 0 && static_cast<std::int64_t>(i) == best_ask_) rescan_ask(i);
        }
        publish();
    }

    TopOfBook top() const { return top_.load(); }
    const SeqLock<TopOfBook>& top_seqlock() const { return top_; }

    std::int64_t qty_at(Side side, std::int64_t px) const {
        const auto i = static_cast<std::size_t>(px - base_);
        return side == Side::Bid ? bids_[i] : asks_[i];
    }

private:
    void rescan_bid(std::size_t from) {
        for (std::size_t j = from; j-- > 0;)
            if (bids_[j] > 0) { best_bid_ = static_cast<std::int64_t>(j); return; }
        best_bid_ = -1;
    }
    void rescan_ask(std::size_t from) {
        for (std::size_t j = from + 1; j < asks_.size(); ++j)
            if (asks_[j] > 0) { best_ask_ = static_cast<std::int64_t>(j); return; }
        best_ask_ = -1;
    }

    void publish() {
        TopOfBook t;
        if (best_bid_ >= 0) {
            t.bid_px = base_ + best_bid_;
            t.bid_qty = bids_[static_cast<std::size_t>(best_bid_)];
        }
        if (best_ask_ >= 0) {
            t.ask_px = base_ + best_ask_;
            t.ask_qty = asks_[static_cast<std::size_t>(best_ask_)];
        }
        t.seq = ++seq_;
        top_.store(t);
    }

    std::int64_t base_;
    std::vector<std::int64_t> bids_;
    std::vector<std::int64_t> asks_;
    std::int64_t best_bid_ = -1;  // index, -1 = empty
    std::int64_t best_ask_ = -1;
    SeqLock<TopOfBook> top_;
    std::uint64_t seq_ = 0;
};

}  // namespace llds
