/**
 * @file test_handover.c
 * @brief Comprehensive test suite for mini-handover-mobility module.
 *
 * Covers L1-L8 knowledge levels with assert-based tests.
 * Tests handover decision algorithms, mobility models, signal measurements,
 * optimization techniques, and protocol state machines.
 *
 * Run: make test
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "handover_types.h"
#include "handover_decision.h"
#include "mobility_model.h"
#include "signal_measurement.h"
#include "handover_optimize.h"
#include "handover_protocol.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EPSILON 1e-6

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

#define ASSERT_EQ_FLOAT(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        printf("  expected %.6f, got %.6f\n", (double)(b), (double)(a)); \
        FAIL(msg); return; \
    } \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  expected %d, got %d\n", (int)(b), (int)(a)); \
        FAIL(msg); return; \
    } \
} while(0)

/* ============================================================================
 * L1: Type definitions and utility functions
 * ============================================================================ */
static void test_l1_measurement_quantity(void) {
    TEST("MeasurementQuantity init and validation");
    MeasurementQuantity mq;
    meas_quantity_init(&mq);
    ASSERT_EQ_FLOAT(mq.rsrp_dbm, -200.0, EPSILON, "init rsrp");
    ASSERT_EQ_FLOAT(mq.rsrq_db, -30.0, EPSILON, "init rsrq");
    ASSERT_TRUE(!meas_quantity_is_valid(&mq), "invalid state detected");

    mq.rsrp_dbm = -90.0;
    mq.rsrq_db = -10.0;
    mq.rssi_dbm = -70.0;
    ASSERT_TRUE(meas_quantity_is_valid(&mq), "valid state accepted");
    PASS();
}

static void test_l1_cell_info(void) {
    TEST("CellInfo init and distance");
    CellInfo cell;
    cell_info_init(&cell);
    ASSERT_EQ_FLOAT(cell.tx_power_dbm, 43.0, EPSILON, "default tx power");
    ASSERT_EQ_FLOAT(cell.coverage_radius_m, 500.0, EPSILON, "default coverage");

    UEPosition ue;
    ue.position_x = 300.0;
    ue.position_y = 400.0;
    double dist = cell_distance_to_ue(&cell, &ue);
    ASSERT_EQ_FLOAT(dist, 500.0, 1.0, "distance 3-4-5 triangle");
    PASS();
}

static void test_l1_ue_context(void) {
    TEST("UEContext init and update");
    UEContext ue;
    ue_context_init(&ue, 1001);
    ASSERT_EQ_INT(ue.ue_id, 1001, "ue id");
    ASSERT_EQ_INT(ue.mobility_state, MOB_STATIONARY, "initial mobility");

    ue.position.velocity_x = 10.0;
    ue.position.velocity_y = 0.0;
    ue_update_position(&ue, 2.0);
    ASSERT_EQ_FLOAT(ue.position.position_x, 20.0, EPSILON, "position update x");
    ASSERT_EQ_FLOAT(ue.position.speed_mps, 10.0, EPSILON, "speed update");
    PASS();
}

static void test_l1_handover_stats(void) {
    TEST("HandoverStatistics update");
    HandoverStatistics stats;
    ho_stats_init(&stats);
    ASSERT_TRUE(stats.ho_success_rate > 0.99, "initial success rate");

    ho_stats_update(&stats, true, false, 80.0);
    ASSERT_EQ_INT(stats.total_handover_attempts, 1, "attempts after success");
    ASSERT_EQ_INT(stats.successful_handovers, 1, "success count");

    ho_stats_update(&stats, false, true, 120.0);
    ASSERT_EQ_INT(stats.failed_handovers, 1, "failure count");
    ASSERT_EQ_INT(stats.pingpong_handovers, 1, "pingpong count");
    ASSERT_TRUE(stats.ho_success_rate < 0.99, "rate updated");
    PASS();
}

/* ============================================================================
 * L2: Core Concepts — Hysteresis decision
 * ============================================================================ */
static void test_l2_hysteresis(void) {
    TEST("Hysteresis handover decision");
    /* Target better by 5 dB, hysteresis 3 dB → should HO */
    ASSERT_TRUE(ho_decision_hysteresis(-100.0, -95.0, 3.0),
                "ho when target 5dB better");
    /* Target better by 2 dB, hysteresis 3 dB → should NOT HO */
    ASSERT_TRUE(!ho_decision_hysteresis(-100.0, -98.0, 3.0),
                "no ho within hysteresis");
    /* Target worse → no HO */
    ASSERT_TRUE(!ho_decision_hysteresis(-100.0, -105.0, 3.0),
                "no ho when target worse");
    /* Zero hysteresis with equal RSRP → no HO (must be strictly greater) */
    ASSERT_TRUE(!ho_decision_hysteresis(-100.0, -100.0, 0.0),
                "no ho when equal with zero hysteresis");
    PASS();
}

/* ============================================================================
 * L4: Fundamental Laws — Event A3 (3GPP)
 * ============================================================================ */
static void test_l4_event_a3(void) {
    TEST("3GPP Event A3 handover decision");

    /* Serving = -105 dBm, Neighbour = -98 dBm, offset = 2 dB, hys = 2 dB
     * Adjusted serving: -105 + 0 + 0 = -105
     * Adjusted neighbour: -98 + 0 + 0 = -98
     * Entry: -98 - 2 > -105 + 2 → -100 > -103 → TRUE */
    bool entry, leaving;
    ho_decision_event_a3(-105.0, -98.0, 2.0, 2.0, 0.0, 0.0, 0.0, &entry, &leaving);
    ASSERT_TRUE(entry, "A3 entry condition met");
    ASSERT_TRUE(!leaving, "A3 leaving condition not met");

    /* Neighbour much weaker → no entry */
    ho_decision_event_a3(-105.0, -120.0, 2.0, 2.0, 0.0, 0.0, 0.0, &entry, &leaving);
    ASSERT_TRUE(!entry, "A3 entry not met for weak neighbour");
    ASSERT_TRUE(leaving, "A3 leaving condition met for weak neighbour");
    PASS();
}

/* ============================================================================
 * L4: Event A5
 * ============================================================================ */
static void test_l4_event_a5(void) {
    TEST("3GPP Event A5 handover decision");

    /* Serving = -115 dBm (< T1=-110), Neighbour = -98 dBm (> T2=-105)
     * With hys=2:
     * Entry: -115+2 = -113 < -110 (TRUE) AND -98-2=-100 > -105 (TRUE) → HO */
    bool entry, leaving;
    ho_decision_event_a5(-115.0, -98.0, -110.0, -105.0, 2.0, 0.0, 0.0, &entry, &leaving);
    ASSERT_TRUE(entry, "A5 entry when serving weak and neighbour strong");
    ASSERT_TRUE(!leaving, "A5 leaving not met");

    /* Serving strong → no entry */
    ho_decision_event_a5(-95.0, -98.0, -110.0, -105.0, 2.0, 0.0, 0.0, &entry, &leaving);
    ASSERT_TRUE(!entry, "A5 no entry when serving strong");
    PASS();
}

/* ============================================================================
 * L4: SINR-based decision
 * ============================================================================ */
static void test_l4_sinr_decision(void) {
    TEST("SINR-based handover decision");
    ASSERT_TRUE(ho_decision_sinr(0.0, 8.0, 3.0, 2.0),
                "ho when serving bad and target good");
    ASSERT_TRUE(!ho_decision_sinr(10.0, 8.0, 3.0, 2.0),
                "no ho when serving good");
    ASSERT_TRUE(!ho_decision_sinr(0.0, 1.0, 3.0, 2.0),
                "no ho when target improvement insufficient");
    PASS();
}

/* ============================================================================
 * L5: TTT mechanism
 * ============================================================================ */
static void test_l5_ttt(void) {
    TEST("Time-To-Trigger evaluation");

    /* 5 consecutive TRUE → TTT of 3 should pass */
    bool hist1[] = {true, true, true, true, true};
    ASSERT_TRUE(ho_decision_ttt_evaluate(hist1, 5, 3), "TTT satisfied");

    /* Sequence with FALSE in middle → should fail */
    bool hist2[] = {true, true, false, true, true};
    ASSERT_TRUE(!ho_decision_ttt_evaluate(hist2, 5, 3), "TTT broken by false");

    /* Sequence where first 3 are true, rest false → should pass */
    bool hist3[] = {true, true, true, false, false};
    ASSERT_TRUE(ho_decision_ttt_evaluate(hist3, 5, 3), "TTT met in first 3");

    /* Not enough samples */
    bool hist4[] = {true, true};
    ASSERT_TRUE(!ho_decision_ttt_evaluate(hist4, 2, 3), "TTT insufficient samples");

    PASS();
}

/* ============================================================================
 * L5: Ping-pong detection
 * ============================================================================ */
static void test_l5_pingpong(void) {
    TEST("Ping-pong handover detection");

    /* Newest first: A→B→A within 3.5s, so newest (index 0) = A at t=3.5s */
    uint32_t pci[] = {101, 102, 101};  /* Newest A, then B, then old A */
    double   time[] = {3500.0, 2000.0, 0.0}; /* Newest first */

    ASSERT_TRUE(ho_detect_pingpong(pci, time, 3, 5000.0),
                "pingpong detected A→B→A within 5s");

    /* Same pattern but outside time window — newest first order */
    uint32_t pci2[] = {101, 102, 101};
    double   time2[] = {8000.0, 2000.0, 0.0}; /* 8s gap, outside 5s window */
    ASSERT_TRUE(!ho_detect_pingpong(pci2, time2, 3, 5000.0),
                "no pingpong outside time window");

    /* Only one handover → no pingpong possible */
    ASSERT_TRUE(!ho_detect_pingpong(pci2, time2, 1, 5000.0),
                "no pingpong with single event");
    PASS();
}

/* ============================================================================
 * L5: Multi-criteria decision (TOPSIS)
 * ============================================================================ */
static void test_l5_topsis(void) {
    TEST("TOPSIS multi-criteria handover decision");

    /* 3 candidates (LTE, WiFi, 5G), 3 criteria (RSSI, BW, Cost)
     * RSSI: benefit (higher better), BW: benefit, Cost: cost (lower better) */
    double matrix[] = {
        -80.0, 50.0, 10.0,   /* LTE: RSSI=-80, BW=50M, Cost=$10 */
        -65.0, 20.0, 5.0,    /* WiFi: RSSI=-65, BW=20M, Cost=$5 */
        -75.0, 100.0, 15.0   /* 5G: RSSI=-75, BW=100M, Cost=$15 */
    };
    double weights[] = {0.4, 0.35, 0.25};
    bool   benefit[] = {true, true, false};

    double closeness[3];
    int best;
    ho_decision_topsis(matrix, weights, benefit, 3, 3, closeness, &best);

    /* All closeness values should be in [0, 1] */
    ASSERT_TRUE(closeness[0] >= 0.0 && closeness[0] <= 1.0, "closeness in range 0");
    ASSERT_TRUE(closeness[1] >= 0.0 && closeness[1] <= 1.0, "closeness in range 1");
    ASSERT_TRUE(closeness[2] >= 0.0 && closeness[2] <= 1.0, "closeness in range 2");
    ASSERT_TRUE(best >= 0 && best < 3, "valid best index");
    PASS();
}

/* ============================================================================
 * L5: Weighted sum model
 * ============================================================================ */
static void test_l5_weighted_sum(void) {
    TEST("Weighted sum multi-criteria decision");

    double matrix[] = { -80.0, 50.0, -65.0, 20.0, -75.0, 100.0 };
    double weights[] = {0.5, 0.5};
    bool   benefit[] = {true, true};
    double scores[3];
    int best;

    ho_decision_weighted_sum(matrix, weights, benefit, 3, 2, scores, &best);
    ASSERT_TRUE(scores[best] >= 0.0, "best score non-negative");
    PASS();
}

/* ============================================================================
 * L5: GRA decision
 * ============================================================================ */
static void test_l5_gra(void) {
    TEST("Grey Relational Analysis handover decision");

    double matrix[] = { -80.0, 50.0, -65.0, 20.0, -75.0, 100.0 };
    double ref[] = { -60.0, 100.0 }; /* Ideal: best RSSI, highest BW */
    double grades[3];
    int best;

    ho_decision_gra(matrix, ref, 0.5, 3, 2, grades, &best);
    ASSERT_TRUE(grades[best] >= 0.0 && grades[best] <= 1.0, "GRA grade in range");
    PASS();
}

/* ============================================================================
 * L3: Mobility models
 * ============================================================================ */
static void test_l3_random_walk(void) {
    TEST("Random Walk mobility model");
    UEPosition pos;
    memset(&pos, 0, sizeof(pos));
    pos.position_x = 100.0;
    pos.position_y = 100.0;

    mob_random_walk_step(&pos, 0.1, 10.0, 2.0, 0.5, 0.0, 500.0, 0.0, 500.0);
    /* Position should have changed */
    ASSERT_TRUE(pos.position_x != 100.0 || pos.position_y != 100.0,
                "position changed after step");
    /* Should be within bounds */
    ASSERT_TRUE(pos.position_x >= 0.0 && pos.position_x <= 500.0,
                "x within bounds");
    ASSERT_TRUE(pos.position_y >= 0.0 && pos.position_y <= 500.0,
                "y within bounds");
    PASS();
}

static void test_l3_mobility_classify(void) {
    TEST("Mobility state classification");
    ASSERT_EQ_INT(mob_classify_state(0.0, 30.0, 5.0), MOB_STATIONARY,
                  "stationary at 0 m/s");
    ASSERT_EQ_INT(mob_classify_state(3.0, 30.0, 5.0), MOB_NORMAL,
                  "normal at 3 m/s");
    ASSERT_EQ_INT(mob_classify_state(15.0, 30.0, 5.0), MOB_MEDIUM,
                  "medium at 15 m/s");
    ASSERT_EQ_INT(mob_classify_state(35.0, 30.0, 5.0), MOB_HIGH_SPEED,
                  "high speed at 35 m/s");
    PASS();
}

static void test_l3_doppler(void) {
    TEST("Doppler shift computation");
    /* UE at 120 km/h = 33.33 m/s, carrier 2.6 GHz, angle 0 (toward BS) */
    double fd = mob_compute_doppler_shift(33.33, 2.6e9, 0.0);
    /* Expected: (33.33 / 3e8) * 2.6e9 ≈ 289 Hz */
    ASSERT_TRUE(fd > 200.0 && fd < 400.0, "doppler shift reasonable");
    PASS();
}

/* ============================================================================
 * L4: Path loss models
 * ============================================================================ */
static void test_l4_friis_path_loss(void) {
    TEST("Friis free-space path loss");
    /* At 2.6 GHz, 100m: PL ≈ 20*log10(4π·100·2.6e9/3e8) ≈ 80.7 dB */
    double pl = meas_friis_free_space_path_loss(100.0, 2.6e9);
    ASSERT_TRUE(pl > 75.0 && pl < 85.0, "PL at 100m reasonable");

    /* Path loss increases with distance */
    double pl_200 = meas_friis_free_space_path_loss(200.0, 2.6e9);
    ASSERT_TRUE(pl_200 > pl, "PL increases with distance");
    PASS();
}

static void test_l4_log_distance(void) {
    TEST("Log-distance path loss");
    double pl = meas_log_distance_path_loss(200.0, 100.0, 80.0, 3.5);
    /* PL(200) = 80 + 35*log10(2) ≈ 80 + 10.5 = 90.5 dB */
    ASSERT_TRUE(pl > 85.0 && pl < 95.0, "log-distance PL reasonable");
    PASS();
}

/* ============================================================================
 * L4: Signal measurements
 * ============================================================================ */
static void test_l4_rssi_rsrp(void) {
    TEST("RSSI and RSRP computation");
    double rssi = meas_compute_rssi(43.0, 15.0, 0.0, 80.0, 0.0, 3.0, 0.0);
    /* P_tx + G_tx + G_rx - PL - body_loss = 43+15+0-80-3 = -25 dBm */
    ASSERT_EQ_FLOAT(rssi, -25.0, 1.0, "RSSI computation");

    double rsrp = meas_compute_rsrp(rssi, 10e6, 0.1667, 7.0);
    /* Should be lower than RSSI because only RS power counted */
    ASSERT_TRUE(rsrp < rssi, "RSRP < RSSI");
    ASSERT_TRUE(rsrp > -200.0, "RSRP valid");
    PASS();
}

static void test_l4_sinr_compute(void) {
    TEST("SINR computation");
    /* Signal = -80 dBm, Interference = -90 dBm, BW = 10 MHz, NF = 7 dB
     * Noise = -174 + 70 + 7 = -97 dBm (10 MHz = 70 dB-Hz)
     * I+N ≈ -89.8 dBm (total interference+noise)
     * SINR ≈ -80 - (-89.8) = 9.8 dB */
    double sinr = meas_compute_sinr(-80.0, -90.0, 10e6, 7.0);
    ASSERT_TRUE(sinr > 5.0 && sinr < 15.0, "SINR reasonable");
    PASS();
}

/* ============================================================================
 * L5: Kalman filter
 * ============================================================================ */
static void test_l5_kalman(void) {
    TEST("Kalman filter for RSRP tracking");

    double state_rsrp = -90.0;
    double state_rate = 0.0;
    double p11 = 100.0, p12 = 0.0, p22 = 10.0;

    /* First measurement */
    double filtered = meas_kalman_filter_rsrp(-85.0, 0.04, 1.0, 4.0,
                                              &state_rsrp, &state_rate,
                                              &p11, &p12, &p22);
    ASSERT_TRUE(filtered > -90.0 && filtered < -80.0, "first filtered estimate");

    /* Second measurement (should be more stable) */
    double filtered2 = meas_kalman_filter_rsrp(-85.5, 0.04, 1.0, 4.0,
                                               &state_rsrp, &state_rate,
                                               &p11, &p12, &p22);
    /* Covariance should decrease after measurements */
    ASSERT_TRUE(p11 < 100.0, "covariance decreased");
    ASSERT_TRUE(filtered2 > -90.0 && filtered2 < -80.0, "second filtered estimate");
    PASS();
}

/* ============================================================================
 * L3: L3 filtering (3GPP)
 * ============================================================================ */
static void test_l5_l3_filter(void) {
    TEST("3GPP Layer-3 filtering");

    /* k=0: a = 1/2^0 = 1 → no filtering, output = input */
    double out1 = meas_l3_filter(-90.0, -85.0, 0);
    ASSERT_EQ_FLOAT(out1, -85.0, EPSILON, "k=0 no filtering");

    /* k=4: a = 1/2^1 = 0.5 → equal weight */
    double out2 = meas_l3_filter(-90.0, -80.0, 4);
    ASSERT_EQ_FLOAT(out2, -85.0, EPSILON, "k=4 equal weight average");

    /* k=8: a = 1/4 = 0.25 → more weight to old value */
    double out3 = meas_l3_filter(-90.0, -80.0, 8);
    ASSERT_EQ_FLOAT(out3, -87.5, EPSILON, "k=8 weighted average");
    PASS();
}

/* ============================================================================
 * L7: Hysteresis optimization
 * ============================================================================ */
static void test_l7_hysteresis_optimize(void) {
    TEST("Hysteresis optimization");
    double h_opt = ho_optimize_hysteresis(8.0, 0.5, 0.5, 0.0, 10.0, 0.5);
    /* Should return a value in the search range */
    ASSERT_TRUE(h_opt >= 0.0 && h_opt <= 10.0, "hysteresis in range");
    PASS();
}

/* ============================================================================
 * L7: TTT optimization
 * ============================================================================ */
static void test_l7_ttt_optimize(void) {
    TEST("TTT optimization");
    double ttt = ho_optimize_ttt(30.0, 2.6e9, 8.0, 50.0, 2.0);
    /* TTT should be in valid LTE range */
    ASSERT_TRUE(ttt >= 40.0 && ttt <= 5120.0, "TTT in LTE range");
    PASS();
}

/* ============================================================================
 * L8: Conditional handover (CHO)
 * ============================================================================ */
static void test_l8_conditional_ho(void) {
    TEST("Conditional Handover evaluation");

    /* Serving weak, target strong, TTT met → execute CHO */
    ASSERT_TRUE(ho_conditional_evaluate(-115.0, -95.0, -110.0, -100.0, true),
                "CHO executed when conditions met");

    /* TTT not met → do not execute */
    ASSERT_TRUE(!ho_conditional_evaluate(-115.0, -95.0, -110.0, -100.0, false),
                "CHO not executed without TTT");

    /* Serving strong → do not execute */
    ASSERT_TRUE(!ho_conditional_evaluate(-95.0, -95.0, -110.0, -100.0, true),
                "CHO not executed when serving strong");
    PASS();
}

/* ============================================================================
 * L8: DAPS handover
 * ============================================================================ */
static void test_l8_daps(void) {
    TEST("DAPS handover evaluation");
    ASSERT_TRUE(ho_daps_evaluate(true, true, 50, 10),
                "DAPS feasible with all conditions met");
    ASSERT_TRUE(!ho_daps_evaluate(false, true, 50, 10),
                "DAPS not feasible without UE support");
    ASSERT_TRUE(!ho_daps_evaluate(true, true, 5, 10),
                "DAPS not feasible with insufficient PRBs");
    PASS();
}

/* ============================================================================
 * L7: MRO diagnosis
 * ============================================================================ */
static void test_l7_mro_diagnose(void) {
    TEST("MRO handover problem diagnosis");

    /* RLF before HO → too-late */
    ASSERT_EQ_INT(ho_mro_diagnose(true, false, false, false), 1,
                  "too-late handover diagnosis");

    /* RLF after HO, reconnected to source → too-early */
    ASSERT_EQ_INT(ho_mro_diagnose(false, true, true, false), 2,
                  "too-early handover diagnosis");

    /* RLF after HO, reconnected to other → wrong cell */
    ASSERT_EQ_INT(ho_mro_diagnose(false, true, false, true), 3,
                  "wrong cell handover diagnosis");

    /* No RLF → no problem */
    ASSERT_EQ_INT(ho_mro_diagnose(false, false, false, false), 0,
                  "no problem diagnosis");
    PASS();
}

/* ============================================================================
 * L7: MRO correction
 * ============================================================================ */
static void test_l7_mro_correct(void) {
    TEST("MRO handover parameter correction");

    double new_ttt, new_a3, new_cio;

    /* Too-late: TTT should decrease */
    ho_mro_correct(1, 160.0, 3.0, 0.0, &new_ttt, &new_a3, &new_cio);
    ASSERT_TRUE(new_ttt < 160.0, "TTT decreased for too-late HO");

    /* Too-early: TTT should increase */
    ho_mro_correct(2, 160.0, 3.0, 0.0, &new_ttt, &new_a3, &new_cio);
    ASSERT_TRUE(new_ttt > 160.0, "TTT increased for too-early HO");
    ASSERT_TRUE(new_cio < 0.0, "CIO decreased for too-early HO");
    PASS();
}

/* ============================================================================
 * L6: LTE X2 handover procedure
 * ============================================================================ */
static void test_l6_lte_x2_handover(void) {
    TEST("LTE X2 handover procedure state machine");

    UEContext ue;
    ue_context_init(&ue, 2001);
    CellInfo serving, target;
    cell_info_init(&serving);
    cell_info_init(&target);
    serving.identity.pci = 100;
    target.identity.pci = 200;

    HandoverDecision decision;
    ho_decision_init(&decision);
    decision.handover_triggered = true;
    decision.recommended_target_id = 200;

    /* Start from INIT */
    HoX2State state = X2HO_INIT;
    HoX2State next;

    /* INIT → MEASUREMENT */
    ho_lte_x2_procedure_step(state, &ue, &serving, &target,
                             &decision, true, true, true, &next);
    ASSERT_EQ_INT(next, X2HO_MEASUREMENT, "INIT→MEASUREMENT");

    /* MEASUREMENT → HO_REQUEST */
    state = next;
    ho_lte_x2_procedure_step(state, &ue, &serving, &target,
                             &decision, true, true, true, &next);
    ASSERT_EQ_INT(next, X2HO_HO_REQUEST, "MEASUREMENT→HO_REQUEST");
    PASS();
}

/* ============================================================================
 * L6: RRC state transitions
 * ============================================================================ */
static void test_l6_rrc_states(void) {
    TEST("RRC state transitions");

    UEContext ue;
    ue_context_init(&ue, 3001);

    /* IDLE → CONNECTED */
    ASSERT_TRUE(ho_rrc_state_transition(RRC_IDLE, RRC_CONNECTED, &ue),
                "IDLE→CONNECTED valid");
    ASSERT_TRUE(ue.is_connected, "UE connected");

    /* CONNECTED → INACTIVE */
    ASSERT_TRUE(ho_rrc_state_transition(RRC_CONNECTED, RRC_INACTIVE, &ue),
                "CONNECTED→INACTIVE valid");
    ASSERT_TRUE(!ue.is_connected, "UE not connected in INACTIVE");

    /* IDLE → INACTIVE (invalid) */
    ASSERT_TRUE(!ho_rrc_state_transition(RRC_IDLE, RRC_INACTIVE, &ue),
                "IDLE→INACTIVE invalid");
    PASS();
}

/* ============================================================================
 * L6: Handover latency model
 * ============================================================================ */
static void test_l6_ho_latency(void) {
    TEST("Handover latency estimation");

    double total = ho_latency_estimate(40.0, 160.0, 10.0, 20.0, 10.0, false);
    /* Total ≈ 40+160 + 20 + 35 + 20 = 275 ms */
    ASSERT_TRUE(total > 200.0 && total < 500.0, "HO latency in expected range");

    /* DAPS should reduce the execution phase */
    double total_daps = ho_latency_estimate(40.0, 160.0, 10.0, 20.0, 10.0, true);
    ASSERT_TRUE(total_daps < total, "DAPS reduces total latency");
    PASS();
}

/* ============================================================================
 * L6: Handover failure recovery
 * ============================================================================ */
static void test_l6_ho_failure_recovery(void) {
    TEST("Handover failure recovery");

    UEContext ue;
    ue_context_init(&ue, 4001);
    ue.position.position_x = 200.0;
    ue.position.position_y = 0.0;

    CellInfo cells[2];
    cell_info_init(&cells[0]);
    cells[0].identity.pci = 500;
    cells[0].position_x = 200.0;
    cells[0].position_y = 50.0;
    cells[0].tx_power_dbm = 43.0;

    /* Recovery with valid cell and context */
    ASSERT_TRUE(ho_failure_recovery(&ue, cells, 1, true, true, -120.0),
                "failure recovery success");
    ASSERT_EQ_INT(ue.serving_cell_id, 500, "new serving cell after recovery");

    /* Recovery fails without UE context */
    ASSERT_TRUE(!ho_failure_recovery(&ue, cells, 1, true, false, -120.0),
                "failure recovery fails without context");
    PASS();
}

/* ============================================================================
 * L7: Admission control
 * ============================================================================ */
static void test_l7_admission_control(void) {
    TEST("Handover admission control");

    CellLoadInfo load;
    memset(&load, 0, sizeof(load));
    load.total_prbs = 100;
    load.used_prbs = 30;
    load.num_active_ues = 20;
    load.prb_utilization = 0.3;

    /* Plenty of resources → admit */
    ASSERT_TRUE(ho_admission_control(&load, 10, 0.9, 10.0, 3.0),
                "admission when resources available");

    /* Too many PRBs needed → reject */
    ASSERT_TRUE(!ho_admission_control(&load, 80, 0.9, 10.0, 3.0),
                "rejection when resources exceeded");

    /* Post-admission utilization too high → reject */
    ASSERT_TRUE(!ho_admission_control(&load, 70, 0.95, 10.0, 3.0),
                "rejection when post-util too high");
    PASS();
}

/* ============================================================================
 * L8: Next cell prediction
 * ============================================================================ */
static void test_l8_cell_prediction(void) {
    TEST("ML-based next cell prediction");

    MeasurementReport history[3];
    meas_report_init(&history[0], 5001, 100);
    meas_report_add_neighbour(&history[0], 200, -95.0, -12.0, 10.0);
    history[0].timestamp_ms = 0.0;
    history[0].serving_meas.rsrp_dbm = -105.0;

    meas_report_init(&history[1], 5001, 100);
    meas_report_add_neighbour(&history[1], 200, -92.0, -10.0, 12.0);
    history[1].timestamp_ms = 100.0;
    history[1].serving_meas.rsrp_dbm = -106.0;

    meas_report_init(&history[2], 5001, 100);
    meas_report_add_neighbour(&history[2], 200, -89.0, -8.0, 14.0);
    history[2].timestamp_ms = 200.0;
    history[2].serving_meas.rsrp_dbm = -107.0;

    /* Neighbour 200 has improving RSRP → should be predicted */
    uint32_t predicted = ho_predict_next_cell(history, 3, 100.0);
    ASSERT_EQ_INT(predicted, 200, "neighbour with improving RSRP predicted");
    PASS();
}

/* ============================================================================
 * Main test runner
 * ============================================================================ */
int main(void) {
    printf("\n=== mini-handover-mobility Test Suite ===\n\n");

    printf("--- L1: Definitions ---\n");
    test_l1_measurement_quantity();
    test_l1_cell_info();
    test_l1_ue_context();
    test_l1_handover_stats();

    printf("\n--- L2/L4: Core Concepts & Laws ---\n");
    test_l2_hysteresis();
    test_l4_event_a3();
    test_l4_event_a5();
    test_l4_sinr_decision();

    printf("\n--- L3: Mathematical Structures (Mobility) ---\n");
    test_l3_random_walk();
    test_l3_mobility_classify();
    test_l3_doppler();

    printf("\n--- L4: Path Loss & Signal Measurements ---\n");
    test_l4_friis_path_loss();
    test_l4_log_distance();
    test_l4_rssi_rsrp();
    test_l4_sinr_compute();

    printf("\n--- L5: Algorithms ---\n");
    test_l5_ttt();
    test_l5_pingpong();
    test_l5_topsis();
    test_l5_weighted_sum();
    test_l5_gra();
    test_l5_kalman();
    test_l5_l3_filter();

    printf("\n--- L6: Canonical Problems ---\n");
    test_l6_lte_x2_handover();
    test_l6_rrc_states();
    test_l6_ho_latency();
    test_l6_ho_failure_recovery();

    printf("\n--- L7: Applications ---\n");
    test_l7_hysteresis_optimize();
    test_l7_ttt_optimize();
    test_l7_mro_diagnose();
    test_l7_mro_correct();
    test_l7_admission_control();

    printf("\n--- L8: Advanced Topics ---\n");
    test_l8_conditional_ho();
    test_l8_daps();
    test_l8_cell_prediction();

    printf("\n========================================\n");
    printf("  Total: %d passed, %d failed\n",
           tests_passed, tests_failed);
    printf("========================================\n\n");

    return (tests_failed > 0) ? 1 : 0;
}
