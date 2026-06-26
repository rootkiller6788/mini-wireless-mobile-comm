/**
 * @file example_ble_health_monitor.c
 * @brief Example: BLE Health Monitor — GATT server, advertising, security
 *
 * Demonstrates a BLE health monitoring application:
 *   - Configuring advertising data (device name, services)
 *   - Setting up GATT service with Heart Rate characteristic
 *   - Connection parameter negotiation
 *   - Link budget estimation for a BLE wearable to phone link
 *   - LE Secure Connections key establishment
 *
 * Knowledge coverage: L1 (BLE types), L2 (BLE state machine, channel hopping),
 * L4 (Friis range estimation), L5 (GATT operations, ECDH key exchange),
 * L6 (BLE health monitor application), L7 (IoT health application).
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../include/wifi_bt_types.h"
#include "../include/bluetooth_core.h"
#include "../include/bluetooth_ble.h"
#include "../include/wireless_security.h"

int main(void)
{
    printf("=== BLE Health Monitor Application ===\n\n");

    /* ================================================================
     * Step 1: Configure BLE advertising
     * ================================================================ */
    printf("--- Advertising Configuration ---\n");

    uint8_t adv_data[31];
    uint8_t mfg_id_data[] = {0x01, 0x02};  /* Simple mfg data */
    int adv_len = ble_adv_data_format(adv_data, 31,
                                      0x02,              /* General Discoverable */
                                      "HeartRateMonitor", /* Device name */
                                      -4,                /* TX power +4 dBm → actually signed */
                                      0x004C,            /* Apple mfg ID (example) */
                                      mfg_id_data, 2);
    printf("  Advertising data length: %d bytes (max 31)\n", adv_len);

    /* Simulate advertising timing */
    printf("  Advertising interval: 100 ms\n");
    for (int ev = 0; ev < 3; ev++) {
        double next_ms;
        ble_adv_timing(&next_ms, 100.0, ev);
        printf("    Event %d: next at +%.1f ms (ch 37→38→39)\n", ev, next_ms);
    }

    /* ================================================================
     * Step 2: Link Layer — state transitions
     * ================================================================ */
    printf("\n--- Connection Setup ---\n");
    ble_ll_state_t state = BLE_LL_STANDBY;

    /* Start advertising */
    ble_ll_state_transition(&state, state, 1);  /* → ADVERTISING */
    printf("  State: STANDBY → ADVERTISING\n");

    /* Connection request received */
    ble_ll_state_transition(&state, state, 3);  /* → CONNECTION */
    printf("  State: ADVERTISING → CONNECTION (connected!)\n");

    /* Connection parameters */
    ble_conn_params_t conn;
    ble_conn_params_init(&conn, 30.0, 0, 2000.0);
    printf("  Connection interval: %.0f ms\n", conn.conn_interval_ms);
    printf("  Supervision timeout: %.0f ms\n", conn.supervision_timeout_ms);

    /* Channel hopping simulation */
    printf("\n--- Channel Hopping (Data Channels) ---\n");
    int ch_map[37];
    for (int i = 0; i < 37; i++) ch_map[i] = 1;  /* All channels good */
    int ch = 0;
    for (int ev = 0; ev < 5; ev++) {
        ble_channel_hop(&ch, ch, 8, ch_map, 37);
        printf("  ConnEvent %d: data channel %d (%.1f MHz)\n",
               ev, ch, ble_channel_to_freq(ch));
    }

    /* ================================================================
     * Step 3: GATT — Heart Rate Service
     * ================================================================ */
    printf("\n--- GATT Heart Rate Service ---\n");

    ble_gatt_service_t hr_service;
    ble_uuid_t svc_uuid;
    svc_uuid.is_16bit = 1;
    svc_uuid.uuid16 = 0x180D;  /* Heart Rate Service */

    if (ble_gatt_service_init(&hr_service, svc_uuid, 3) == 0) {
        /* Characteristic 1: Heart Rate Measurement (0x2A37) — Notify */
        ble_uuid_t hr_meas_uuid;
        hr_meas_uuid.is_16bit = 1;
        hr_meas_uuid.uuid16 = 0x2A37;
        ble_gatt_add_characteristic(&hr_service, 0, hr_meas_uuid,
                                     BLE_GATT_PROP_NOTIFY, 0x01 | 0x02);

        /* Characteristic 2: Body Sensor Location (0x2A38) — Read */
        ble_uuid_t bsl_uuid;
        bsl_uuid.is_16bit = 1;
        bsl_uuid.uuid16 = 0x2A38;
        ble_gatt_add_characteristic(&hr_service, 1, bsl_uuid,
                                     BLE_GATT_PROP_READ, 0x01);

        /* Simulate heart rate measurement */
        uint8_t hr_data[] = {0x06, 0x48};  /* Flags: 8-bit HR, HR=72 bpm */
        ble_gatt_write(&hr_service, 0, hr_data, 2);

        uint8_t read_back[10];
        int len = ble_gatt_read(read_back, 10, &hr_service, 0);
        printf("  Heart Rate Measurement: %d bytes\n", len);
        if (len >= 2) {
            printf("    Flags: 0x%02X (8-bit HR format)\n", read_back[0]);
            printf("    Heart Rate: %d bpm\n", read_back[1]);
        }
        printf("  GATT handles: start=%d, end=%d\n",
               hr_service.start_handle, hr_service.end_handle);

        /* Cleanup */
        for (int i = 0; i < hr_service.n_attrs; i++) free(hr_service.attrs[i].value);
        free(hr_service.attrs);
    }

    /* ================================================================
     * Step 4: Security — LE Secure Connections
     * ================================================================ */
    printf("\n--- LE Secure Connections ---\n");

    /* Simulate key exchange */
    uint8_t priv_key[32], pub_x[32], pub_y[32], dhkey[32];
    memset(priv_key, 0x11, 32);
    memset(pub_x, 0x22, 32);
    memset(pub_y, 0x33, 32);

    ble_le_sc_dhkey(dhkey, priv_key, pub_x, pub_y);
    printf("  DHKey generated (256-bit): ");
    for (int i = 0; i < 4; i++) printf("%02X", dhkey[i]);
    printf("...\n");

    /* Derive LTK */
    uint8_t nonce_m[16], nonce_s[16], ltk[16];
    memset(nonce_m, 0xAA, 16);
    memset(nonce_s, 0xBB, 16);
    bt_address_t addr_m, addr_s;
    memset(addr_m.addr, 0xCC, 6);
    memset(addr_s.addr, 0xDD, 6);

    ble_f5_ltk_derive(ltk, dhkey, nonce_m, nonce_s, &addr_m, &addr_s);
    printf("  LTK derived (128-bit): ");
    for (int i = 0; i < 4; i++) printf("%02X", ltk[i]);
    printf("...\n");

    /* ================================================================
     * Step 5: Link Budget — Range Estimation
     * ================================================================ */
    printf("\n--- Link Budget & Range ---\n");
    double range = ble_range_estimate(0.0, -93.0, 0.0, 0.0);
    printf("  TX: 0 dBm, RX sensitivity: -93 dBm\n");
    printf("  Estimated range (free space): %.0f meters\n", range);

    /* Realistic indoor with path loss exponent */
    double rx_power_10m = received_power_dbm(0.0, 0.0, 0.0, 10.0, 2.45e9, 2.8);
    double margin = ble_link_margin(rx_power_10m, -93.0);
    printf("  Received power at 10m (n=2.8): %.1f dBm\n", rx_power_10m);
    printf("  Link margin at 10m: %.1f dB\n", margin);

    /* ================================================================
     * Step 6: Coexistence — WiFi + BLE sharing 2.4 GHz band
     * ================================================================ */
    printf("\n--- Coexistence (WiFi + BLE) ---\n");
    coex_config_t coex;
    coex.mechanism = COEX_AFH;
    coex.wifi_duty_cycle = 0.35;
    coex.bt_duty_cycle = 0.05;
    (void)coex;  /* Coexistence config used to illustrate AFH selection */
    bt_fhss_params_t fhss;
    bt_fhss_init(&fhss, 0);

    /* Simulate AFH: avoid WiFi channels 1, 6, 11 */
    double per_chan[79] = {0};
    /* WiFi Channel 1: 2402-2422 MHz → BT channels 0-20 */
    /* WiFi Channel 6: 2427-2447 MHz → BT channels 25-45 */
    /* WiFi Channel 11: 2452-2472 MHz → BT channels 50-70 */
    /* Simulate high PER where WiFi overlaps */
    for (int i = 0; i < 79; i++) {
        double f = bt_channel_to_freq(i);
        if ((f >= 2402 && f <= 2423) ||
            (f >= 2426 && f <= 2448) ||
            (f >= 2451 && f <= 2473)) {
            per_chan[i] = 0.15;  /* 15% PER near WiFi channels */
        } else {
            per_chan[i] = 0.01;  /* 1% PER clean channels */
        }
    }

    int good = bt_afh_classify(&fhss, per_chan, 79, 0.10);
    printf("  AFH: %d good channels remaining (min 20 required)\n", good);
    printf("  Mechanism: AFH (BT avoids WiFi-occupied channels)\n");

    printf("\n=== BLE Health Monitor Demo Complete ===\n");
    return 0;
}
