#include <gtest/gtest.h>
#include "agenui_batch_guard.h"

using namespace agenui;

// =============================================================================
// BatchGuard Tests
// =============================================================================

class BatchGuardTest : public ::testing::Test {
protected:
    int flushCount = 0;
    BatchGuard guard{[this]() { ++flushCount; }};
};

// --- requestFlush without batch window ---

TEST_F(BatchGuardTest, RequestFlush_NoBatch_FlushesImmediately) {
    guard.requestFlush();
    EXPECT_EQ(flushCount, 1);
}

TEST_F(BatchGuardTest, RequestFlush_NoBatch_MultipleCallsFlushEachTime) {
    guard.requestFlush();
    guard.requestFlush();
    guard.requestFlush();
    EXPECT_EQ(flushCount, 3);
}

// --- beginBatch / endBatch basic ---

TEST_F(BatchGuardTest, BeginEnd_NoFlushRequested_NoFlush) {
    guard.beginBatch();
    guard.endBatch();
    EXPECT_EQ(flushCount, 0);
}

TEST_F(BatchGuardTest, RequestFlush_InsideBatch_DefersUntilEnd) {
    guard.beginBatch();
    guard.requestFlush();
    EXPECT_EQ(flushCount, 0);
    guard.endBatch();
    EXPECT_EQ(flushCount, 1);
}

TEST_F(BatchGuardTest, MultipleFlushRequests_InsideBatch_CoalescedIntoOne) {
    guard.beginBatch();
    guard.requestFlush();
    guard.requestFlush();
    guard.requestFlush();
    guard.endBatch();
    EXPECT_EQ(flushCount, 1);
}

// --- Nested batches ---

TEST_F(BatchGuardTest, NestedBatch_FlushDefersToOutermost) {
    guard.beginBatch();
    guard.beginBatch();
    guard.requestFlush();
    guard.endBatch(); // inner close - depth goes from 2→1
    EXPECT_EQ(flushCount, 0);
    guard.endBatch(); // outer close - triggers flush
    EXPECT_EQ(flushCount, 1);
}

TEST_F(BatchGuardTest, TripleNestedBatch_FlushOnlyAtDepthZero) {
    guard.beginBatch();
    guard.beginBatch();
    guard.beginBatch();
    guard.requestFlush();
    guard.endBatch();
    EXPECT_EQ(flushCount, 0);
    guard.endBatch();
    EXPECT_EQ(flushCount, 0);
    guard.endBatch();
    EXPECT_EQ(flushCount, 1);
}

// --- endBatch with depth 0 (no-op) ---

TEST_F(BatchGuardTest, EndBatch_WithoutBegin_DoesNothing) {
    guard.endBatch(); // should be safe no-op
    EXPECT_EQ(flushCount, 0);
}

// --- InnerGuards cascading ---

class BatchGuardInnerTest : public ::testing::Test {
protected:
    int outerFlushCount = 0;
    int innerFlushCount = 0;
    BatchGuard inner{[this]() { ++innerFlushCount; }};
    BatchGuard outer{[this]() { ++outerFlushCount; }};

    void SetUp() override {
        outer.setInnerGuards({&inner});
    }
};

TEST_F(BatchGuardInnerTest, OuterBeginEnd_CascadesBeginEndToInner) {
    outer.beginBatch();
    inner.requestFlush();
    EXPECT_EQ(innerFlushCount, 0); // inner is batched too
    outer.endBatch();
    EXPECT_EQ(innerFlushCount, 1); // inner flushed before outer
    EXPECT_EQ(outerFlushCount, 0); // outer had no pending flush
}

TEST_F(BatchGuardInnerTest, OuterFlushAndInnerFlush_BothFire) {
    outer.beginBatch();
    inner.requestFlush();
    outer.requestFlush();
    outer.endBatch();
    EXPECT_EQ(innerFlushCount, 1);
    EXPECT_EQ(outerFlushCount, 1);
}

TEST_F(BatchGuardInnerTest, InnerFlushOrder_InnerBeforeOuter) {
    // Verify inner guards flush BEFORE the outer guard
    int sequence = 0;
    int innerSeq = 0;
    int outerSeq = 0;

    BatchGuard innerG([&]() { innerSeq = ++sequence; });
    BatchGuard outerG([&]() { outerSeq = ++sequence; });
    outerG.setInnerGuards({&innerG});

    outerG.beginBatch();
    innerG.requestFlush();
    outerG.requestFlush();
    outerG.endBatch();

    EXPECT_EQ(innerSeq, 1);
    EXPECT_EQ(outerSeq, 2);
}

TEST_F(BatchGuardInnerTest, NestedOuter_InnerOnlyCascadesOnOutermostBegin) {
    outer.beginBatch(); // cascades to inner (outermost)
    outer.beginBatch(); // nested, does NOT cascade again
    inner.requestFlush();
    outer.endBatch(); // depth 2→1, no flush yet
    EXPECT_EQ(innerFlushCount, 0);
    outer.endBatch(); // depth 1→0, cascades endBatch to inner
    EXPECT_EQ(innerFlushCount, 1);
}

// --- Multiple inner guards, reverse-close order ---

TEST(BatchGuardMultiInner, ReverseCloseOrder) {
    int sequence = 0;
    int seq1 = 0, seq2 = 0, seq3 = 0;

    BatchGuard inner1([&]() { seq1 = ++sequence; });
    BatchGuard inner2([&]() { seq2 = ++sequence; });
    BatchGuard outer([&]() { seq3 = ++sequence; });
    outer.setInnerGuards({&inner1, &inner2});

    outer.beginBatch();
    inner1.requestFlush();
    inner2.requestFlush();
    outer.requestFlush();
    outer.endBatch();

    // inner2 closes first (reverse order), then inner1, then outer
    EXPECT_EQ(seq2, 1);
    EXPECT_EQ(seq1, 2);
    EXPECT_EQ(seq3, 3);
}

// --- Re-entrancy guard ---

TEST(BatchGuardReentrancy, FlushCallback_RequestFlush_CoalescedInNextPass) {
    int flushCount = 0;
    BatchGuard guard([&]() {
        ++flushCount;
        if (flushCount == 1) {
            // Re-entrant: request another flush from within flush callback
            guard.requestFlush();
        }
    });

    guard.requestFlush();
    // The re-entrant requestFlush triggers a second pass in the flush loop
    EXPECT_EQ(flushCount, 2);
}

TEST(BatchGuardReentrancy, MaxFlushPasses_Capped) {
    int flushCount = 0;
    BatchGuard guard([&]() {
        ++flushCount;
        // Always request another flush → infinite loop without cap
        guard.requestFlush();
    });

    guard.requestFlush();
    // Capped at 8 passes (kMaxFlushPasses)
    EXPECT_EQ(flushCount, 8);
}

TEST(BatchGuardReentrancy, InsideBatch_ReentrantRequestDefers) {
    int flushCount = 0;
    BatchGuard guard([&]() {
        ++flushCount;
        if (flushCount == 1) {
            guard.requestFlush(); // re-entrant during flush
        }
    });

    guard.beginBatch();
    guard.requestFlush();
    guard.endBatch();
    // First flush fires, re-entrant request triggers second pass
    EXPECT_EQ(flushCount, 2);
}

// --- Null flush callback ---

TEST(BatchGuardNullCallback, RequestFlush_NullCallback_DoesNotCrash) {
    BatchGuard guard(nullptr);
    guard.requestFlush(); // should not crash
    guard.beginBatch();
    guard.requestFlush();
    guard.endBatch(); // should not crash
}

// --- Null inner guards tolerance ---

TEST(BatchGuardNullInner, NullInnerGuard_Tolerated) {
    int flushCount = 0;
    BatchGuard outer([&]() { ++flushCount; });
    outer.setInnerGuards({nullptr, nullptr});

    outer.beginBatch();
    outer.requestFlush();
    outer.endBatch();
    EXPECT_EQ(flushCount, 1); // no crash, flush still works
}

// =============================================================================
// BatchScope Tests
// =============================================================================

TEST(BatchScopeTest, SingleTarget_BeginsAndEndsOnScope) {
    int flushCount = 0;
    BatchGuard guard([&]() { ++flushCount; });
    {
        BatchScope scope(&guard);
        guard.requestFlush();
        EXPECT_EQ(flushCount, 0);
    }
    EXPECT_EQ(flushCount, 1);
}

TEST(BatchScopeTest, MultiTarget_ClosesInReverseOrder) {
    int sequence = 0;
    int seq1 = 0, seq2 = 0;

    BatchGuard g1([&]() { seq1 = ++sequence; });
    BatchGuard g2([&]() { seq2 = ++sequence; });
    {
        BatchScope scope(&g1, &g2);
        g1.requestFlush();
        g2.requestFlush();
    }
    // g2 ends first (reverse), g1 ends second
    EXPECT_EQ(seq2, 1);
    EXPECT_EQ(seq1, 2);
}

TEST(BatchScopeTest, NullTarget_Tolerated) {
    int flushCount = 0;
    BatchGuard guard([&]() { ++flushCount; });
    {
        BatchScope scope(static_cast<BatchGuard*>(nullptr), &guard);
        guard.requestFlush();
    }
    EXPECT_EQ(flushCount, 1); // no crash
}

TEST(BatchScopeTest, TripleTarget_CorrectOrder) {
    int sequence = 0;
    int s1 = 0, s2 = 0, s3 = 0;

    BatchGuard g1([&]() { s1 = ++sequence; });
    BatchGuard g2([&]() { s2 = ++sequence; });
    BatchGuard g3([&]() { s3 = ++sequence; });
    {
        BatchScope scope(&g1, &g2, &g3);
        g1.requestFlush();
        g2.requestFlush();
        g3.requestFlush();
    }
    // Reverse: g3 first, g2 second, g1 third
    EXPECT_EQ(s3, 1);
    EXPECT_EQ(s2, 2);
    EXPECT_EQ(s1, 3);
}
