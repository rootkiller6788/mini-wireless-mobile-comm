/**
 * @file link_budget_demo.c
 * @brief Link budget and range estimation demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lora_phy.h"
#include "nbiot_phy.h"
#include "lora_nbiot_common.h"
#include "lora_channel.h"

int main(void) {
    printf("=== LPWAN Link Budget Demo ===\n\n");

    path_loss_model_t model;
    memset(&model, 0, sizeof(model));
    model.env = PROP_ENV_URBAN;
    model.path_loss_exponent = 3.0;
    model.shadow_fading_std_db = 8.0;
    model.reference_distance_m = 1.0;
    model.frequency_mhz = 868.0;
    model.tx_height_m = 30.0;
    model.rx_height_m = 1.5;

    printf("Environment: Urban (n=3.0, sigma=8dB, 868 MHz)\n\n");

    printf("[L4] Friis free-space path loss:\n");
    printf("  100m: %.1f dB\n", friis_path_loss_db(100.0, 868.0));
    printf("  1km:  %.1f dB\n", friis_path_loss_db(1000.0, 868.0));
    printf("  10km: %.1f dB\n\n", friis_path_loss_db(10000.0, 868.0));

    printf("[L3] Log-distance model:\n");
    printf("  100m: %.1f dB\n", log_distance_path_loss_db(100.0, &model));
    printf("  1km:  %.1f dB\n", log_distance_path_loss_db(1000.0, &model));
    printf("  10km: %.1f dB\n\n", log_distance_path_loss_db(10000.0, &model));

    printf("[L4] Okumura-Hata model (urban):\n");
    printf("  1km:  %.1f dB\n",
           okumura_hata_path_loss_db(1.0, 868.0, 30.0, 1.5, 0));
    printf("  10km: %.1f dB\n\n",
           okumura_hata_path_loss_db(10.0, 868.0, 30.0, 1.5, 0));

    printf("[L4] Link budget (LoRa SF12, 14dBm TX):\n");
    double sens = lora_receiver_sensitivity(LORA_SF12, LORA_BW_125_KHZ, 6.0);
    printf("  RX sensitivity: %.1f dBm\n", sens);

    double max_range = max_range_from_link_budget(&model, 14.0, 0.0, 0.0,
                                                    sens, 3.0);
    printf("  Max range (log-distance): %.1f km\n\n", max_range / 1000.0);

    printf("[L5] Battery life estimation:\n");
    double life = battery_life_estimate_years(2400.0, 3.6, 3600.0,
                                               0.5, 30.0, 0.01, 10.0, 1.5);
    printf("  Li-SOCl2 AA (2400mAh): %.1f years\n", life);
    printf("  (1 report/hour, 0.5s TX, 30mA TX, 1.5uA sleep)\n");

    printf("\nDemo complete.\n");
    return 0;
}
