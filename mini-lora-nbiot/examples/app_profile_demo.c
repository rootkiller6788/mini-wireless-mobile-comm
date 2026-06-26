/**
 * @file app_profile_demo.c
 * @brief LPWAN application profiles demo -- smart meter, asset tracker, agriculture
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lora_nbiot_common.h"

int main(void) {
    printf("=== LPWAN Application Profiles Demo ===\n\n");

    printf("[L7] Smart Meter Report:\n");
    smart_meter_report_t meter;
    memset(&meter, 0, sizeof(meter));
    meter.meter_id = 424242;
    meter.reading_value = 12345.678;
    meter.reading_timestamp = 1719000000;
    meter.battery_level_pct = 92;
    meter.valve_status = 1;
    meter.alarm_flag = 0;

    uint8_t buf[32];
    int len = smart_meter_encode(&meter, buf, sizeof(buf));
    printf("  Encoded: %d bytes\n", len);

    smart_meter_report_t decoded;
    smart_meter_decode(buf, len, &decoded);
    printf("  Meter ID: %u\n", decoded.meter_id);
    printf("  Reading: %.3f\n", decoded.reading_value);
    printf("  Battery: %u%%\n", decoded.battery_level_pct);

    printf("\n[L7] Asset Tracker Report:\n");
    asset_tracker_report_t tracker;
    memset(&tracker, 0, sizeof(tracker));
    tracker.device_id = 999;
    tracker.latitude = 48.1351;
    tracker.longitude = 11.5820;
    tracker.has_gnss = 1;
    tracker.gateway_count = 3;
    tracker.rssi_dbm[0] = -85.0;
    tracker.rssi_dbm[1] = -92.0;
    tracker.rssi_dbm[2] = -98.0;
    tracker.battery_v = 3.7;
    tracker.motion_state = 1;

    len = asset_tracker_encode(&tracker, buf, sizeof(buf));
    printf("  Encoded: %d bytes\n", len);

    asset_tracker_report_t atd;
    asset_tracker_decode(buf, len, &atd);
    printf("  Location: %.4f, %.4f\n", atd.latitude, atd.longitude);

    printf("\n[L7] Agriculture Sensor Report:\n");
    agriculture_sensor_report_t agri;
    memset(&agri, 0, sizeof(agri));
    agri.sensor_id = 555;
    agri.soil_moisture_pct = 32.5;
    agri.soil_temperature_c = 18.2;
    agri.air_temperature_c = 22.5;
    agri.air_humidity_pct = 65.0;
    agri.leaf_wetness = 0.15;
    agri.solar_radiation_wm2 = 450;
    agri.battery_v = 3.85;
    agri.solar_panel_v = 5.1;

    len = agriculture_sensor_encode(&agri, buf, sizeof(buf));
    printf("  Encoded: %d bytes\n", len);

    agriculture_sensor_report_t agd;
    agriculture_sensor_decode(buf, len, &agd);
    printf("  Soil moisture: %.1f%%\n", agd.soil_moisture_pct);
    printf("  Air temp: %.1f C\n", agd.air_temperature_c);

    printf("\n[L2] LPWAN Technology Comparison:\n");
    for (int t = 0; t < 4; t++) {
        const lpwan_tech_profile_t *p = lpwan_get_profile((lpwan_technology_t)t);
        const char *names[] = {"LoRa", "NB-IoT", "LTE-M", "Sigfox"};
        printf("  %-8s: Range=%4.0fkm rural, Rate=%.0f-%.0f bps, LinkBudget=%d dB\n",
               names[t], p->max_range_km_rural,
               p->min_datarate_bps, p->max_datarate_bps,
               (int)p->link_budget_db);
    }

    printf("\nDemo complete.\n");
    return 0;
}
