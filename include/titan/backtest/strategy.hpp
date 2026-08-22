#pragma once

#include "titan/core/events.hpp"
#include "titan/market_data/book_snapshot.hpp"
#include "titan/market_data/execution_report.hpp"

namespace titan {

// Hooks fire once after full-file replay completes, not live per-message.
// Default bodies are no-ops, so IStrategy itself is a valid do-nothing strategy.
class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual void onEvent(const Event&) {}
    virtual void onExecutionReport(const ExecutionReport&) {}
    virtual void onBookUpdate(const BookSnapshot&) {}
};

}  // namespace titan
