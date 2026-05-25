#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <vector>

namespace agenui {

/**
 * @brief Composable batch-depth manager that coalesces a burst of flush
 *        requests into a single deferred callback at the end of the
 *        outermost batch window.
 *
 * Typical owners:
 *   - ComponentManager : flushes dirty components on batch close.
 *   - VirtualDOM       : emits a single layout-change notification on
 *                        batch close.
 *
 * The owner constructs a BatchGuard with a flush callback, then:
 *   - Uses BatchScope (RAII) or direct beginBatch/endBatch to open/close
 *     batch windows.
 *   - Calls requestFlush() whenever work is produced that will eventually
 *     need flushing. If no batch window is open, the flush fires
 *     immediately; otherwise it is deferred to the outermost endBatch().
 */
class BatchGuard {
public:
    using FlushCallback = std::function<void()>;

    explicit BatchGuard(FlushCallback flushCallback)
        : _flushCallback(std::move(flushCallback)) {}

    /**
     * @brief Set inner guards that are cascaded on beginBatch/endBatch.
     *
     * When this guard opens a batch window (outermost beginBatch), it also
     * opens batch windows on all inner guards in order. When it closes the
     * outermost window, it first closes inner guards in reverse order
     * (triggering their flushes), then fires its own flush callback.
     *
     * Must be called after the inner guards are fully constructed.
     * Typical usage: Surface constructs CM and VDOM first, then calls
     * setInnerGuards({vdom->batchGuard(), cm->batchGuard()}).
     */
    void setInnerGuards(std::initializer_list<BatchGuard*> guards) {
        _innerGuards.assign(guards.begin(), guards.end());
    }

    /**
     * @brief Open a batch window. Calls may nest; only the outermost
     *        beginBatch cascades to inner guards.
     */
    void beginBatch() {
        if (_batchDepth == 0) {
            for (BatchGuard* inner : _innerGuards) {
                if (inner) inner->beginBatch();
            }
        }
        ++_batchDepth;
    }

    /**
     * @brief Close a batch window. When the nesting depth reaches one:
     *        1. Close inner guards in reverse order (triggering their flushes)
     *           while _batchDepth is still 1, so any requestFlush() calls
     *           produced during inner flushes are deferred (not fired
     *           immediately).
     *        2. Decrement _batchDepth to zero.
     *        3. Fire own flush callback if a flush was requested.
     */
    void endBatch() {
        if (_batchDepth == 0) {
            return;
        }
        if (_batchDepth == 1) {
            // Close inner guards while our depth is still 1. This ensures
            // any requestFlush() triggered by inner flushes sees
            // _batchDepth > 0 and defers rather than firing immediately.
            for (std::size_t i = _innerGuards.size(); i-- > 0; ) {
                if (_innerGuards[i]) _innerGuards[i]->endBatch();
            }
            --_batchDepth;
            if (_pendingFlush) {
                flush();
            }
        } else {
            --_batchDepth;
        }
    }

    /**
     * @brief Request a flush.
     *   - If currently inside a batch window OR a flush is already in
     *     progress: marks the flush as pending (coalesced with any
     *     previously pending request).
     *   - If not inside a batch window and no flush is in progress:
     *     fires the flush callback immediately.
     *
     * The re-entrancy guard (_flushing) prevents recursive flush calls
     * when the flush callback itself triggers further requestFlush()
     * invocations (e.g. data-binding chains). Those nested requests are
     * coalesced and drained by the outermost flush invocation via the
     * pending-flush loop.
     */
    void requestFlush() {
        if (_batchDepth > 0 || _flushing) {
            _pendingFlush = true;
        } else {
            flush();
        }
    }

private:
    void flush() {
        if (!_flushCallback) {
            return;
        }
        _flushing = true;
        // Loop until no more pending flush requests accumulate during
        // the callback (handles re-entrant requestFlush from inside the
        // flush callback). Hard cap prevents infinite loops from
        // pathological feedback cycles (e.g. A dirties B, B dirties A).
        constexpr int kMaxFlushPasses = 8;
        int pass = 0;
        do {
            _pendingFlush = false;
            _flushCallback();
        } while (_pendingFlush && ++pass < kMaxFlushPasses);
        _flushing = false;
    }

    int _batchDepth = 0;
    bool _pendingFlush = false;
    bool _flushing = false;
    FlushCallback _flushCallback;
    std::vector<BatchGuard*> _innerGuards;
};

/**
 * @brief RAII helper that opens one or more batch windows on construction
 *        and closes them on destruction.
 *
 * Single-target usage:
 * @code
 *   BatchScope scope(componentManager->batchGuard());
 * @endcode
 *
 * Multi-target usage (composite scope):
 * @code
 *   // Opens VDOM guard first, then CM guard. Closes CM first (flushing
 *   // dirty components into the still-batching VDOM), then VDOM
 *   // (emitting one consolidated layout notification).
 *   BatchScope scope(virtualDom->batchGuard(), componentManager->batchGuard());
 * @endcode
 *
 * Ordering contract (important for correctness):
 *   - On construction, targets are begun in argument order: arg #0 first,
 *     arg #N-1 last. So the FIRST argument is the OUTERMOST scope.
 *   - On destruction, targets are ended in reverse argument order: arg
 *     #N-1 first, arg #0 last. So the LAST argument flushes FIRST.
 *   - When one subsystem's flush feeds another subsystem (e.g.
 *     ComponentManager flush produces updateNode calls that feed
 *     VirtualDOM), put the *consumer* first and the *producer* last:
 *     BatchScope(consumer_guard, producer_guard).
 *
 * Null targets are tolerated and silently skipped so callers can pass
 * partially-initialized pointers without extra null checks.
 */
template <std::size_t N>
class BatchScope {
    static_assert(N >= 1, "BatchScope needs at least one target");

public:
    template <typename... Args,
              typename = typename std::enable_if<
                  sizeof...(Args) == N &&
                  std::conjunction<std::is_convertible<Args, BatchGuard*>...>::value
              >::type>
    explicit BatchScope(Args... targets)
        : _targets{static_cast<BatchGuard*>(targets)...} {
        for (BatchGuard* target : _targets) {
            if (target) {
                target->beginBatch();
            }
        }
    }

    ~BatchScope() {
        for (std::size_t i = N; i-- > 0; ) {
            if (_targets[i]) {
                _targets[i]->endBatch();
            }
        }
    }

    BatchScope(const BatchScope&) = delete;
    BatchScope& operator=(const BatchScope&) = delete;
    BatchScope(BatchScope&&) = delete;
    BatchScope& operator=(BatchScope&&) = delete;

private:
    std::array<BatchGuard*, N> _targets;
};

// CTAD: BatchScope(a, b, c) deduces BatchScope<3>.
template <typename... Args>
BatchScope(Args...) -> BatchScope<sizeof...(Args)>;

}  // namespace agenui
