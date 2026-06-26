/**
 * @file pathloss.c
 * @brief Path Loss Model Implementations (L2, L4, L7)
 *
 * Implements fundamental and advanced path loss models for wireless
 * channel characterization. Each function implements a specific knowledge
 * point from wireless propagation theory.
 *
 * Theorem Coverage:
 *   L4: Friis Free-Space Equation
 *   L4: Two-Ray Ground Reflection
 *   L4: Okumura-Hata (urban/suburban/rural)
 *   L4: COST-231 Hata (1500-2000 MHz extension)
 *   L4: Walfisch-Ikegami (microcell)
 *   L4: ITU-R P.1238 Indoor Propagation
 *   L7: 3GPP TR 38.901 UMi/UMa (5G NR)
 *
 * Reference:
 *   Friis, "A Note on a Simple Transmission Formula", Proc. IRE, 1946
 *   Hata, "Empirical Formula for Propagation Loss", IEEE Trans VT, 1980
 *   COST 231 Final Report, "Digital Mobile Radio", 1999
 *   3GPP TR 38.901 v16.1.0, "Study on channel model for frequencies
 *     from 0.5 to 100 GHz", 2020
 */

#include "pathloss.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L4: Friis Free-Space Path Loss
 *============================================================================*/

double pathloss_friis_free_space(double distance_m, double freq_hz)
{
    /* Guard against invalid inputs */
    if (distance_m <= 0.0 || freq_hz <= 0.0) {
        return -1.0;  /* error indicator */
    }

    double lambda = CHANNEL_C0 / freq_hz;

    /* PL(dB) = 20*log10(4*pi*d/lambda) */
    double pl_db = 20.0 * log10((4.0 * M_PI * distance_m) / lambda);

    /* Path loss is always positive (loss, not gain).
     * For extremely short distances (< lambda/(4*pi)), theoretical
     * model gives negative loss (gain) — clamp to 0 dB. */
    if (pl_db < 0.0) {
        pl_db = 0.0;
    }

    return pl_db;
}

double pathloss_reference_loss(double ref_distance_m, double freq_hz)
{
    return pathloss_friis_free_space(ref_distance_m, freq_hz);
}

/*============================================================================
 * L4: Two-Ray Ground Reflection Model
 *============================================================================*/

double pathloss_two_ray_breakpoint(double tx_height_m, double rx_height_m,
                                    double freq_hz)
{
    if (tx_height_m <= 0.0 || rx_height_m <= 0.0 || freq_hz <= 0.0) {
        return -1.0;
    }

    double lambda = CHANNEL_C0 / freq_hz;
    /* Breakpoint: d_b = 4*h_t*h_r / lambda */
    return (4.0 * tx_height_m * rx_height_m) / lambda;
}

double pathloss_two_ray(double distance_m, double tx_height_m,
                         double rx_height_m, double freq_hz)
{
    if (distance_m <= 0.0 || tx_height_m <= 0.0 ||
        rx_height_m <= 0.0 || freq_hz <= 0.0) {
        return -1.0;
    }

    double d_b = pathloss_two_ray_breakpoint(tx_height_m, rx_height_m, freq_hz);

    /* Below breakpoint: oscillatory (use free-space approximation) */
    if (distance_m < d_b) {
        return pathloss_friis_free_space(distance_m, freq_hz);
    }

    /* Above breakpoint:
     * PL(dB) = 40*log10(d) - 10*log10(h_t^2 * h_r^2)
     *         = 40*log10(d) - 20*log10(h_t) - 20*log10(h_r) */
    double pl_db = 40.0 * log10(distance_m) -
                   20.0 * log10(tx_height_m) -
                   20.0 * log10(rx_height_m);

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L4: Log-Distance Path Loss Model
 *============================================================================*/

double pathloss_log_distance(const pathloss_params_t *params, double distance_m)
{
    if (!params || distance_m <= 0.0) return -1.0;
    if (params->ref_distance_m <= 0.0) return -1.0;
    if (distance_m < params->ref_distance_m) {
        /* Inside reference distance: use free-space */
        return pathloss_friis_free_space(distance_m, params->carrier_freq_hz);
    }

    /* PL(d) = PL(d_0) + 10*n*log10(d/d_0) */
    double pl_db = params->ref_loss_db +
                   10.0 * params->path_loss_exponent *
                   log10(distance_m / params->ref_distance_m);

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

double pathloss_shadow_fading_sample(double sigma_db)
{
    /* Box-Muller transform for N(0, sigma_db^2) */
    double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
    double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);

    /* Avoid log(0) */
    if (u1 <= 0.0) u1 = 1e-10;
    if (u2 <= 0.0) u2 = 1e-10;

    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return sigma_db * z;
}

/*============================================================================
 * L4: Okumura-Hata Model
 *============================================================================*/

double pathloss_okumura_hata_correction(double rx_height_m, double freq_mhz,
                                         int is_large_city)
{
    if (is_large_city) {
        /* Large city correction (f >= 300 MHz) */
        if (freq_mhz <= 200.0) {
            /* f <= 200 MHz */
            return 8.29 * pow(log10(1.54 * rx_height_m), 2.0) - 1.1;
        } else {
            /* f >= 400 MHz */
            return 3.2 * pow(log10(11.75 * rx_height_m), 2.0) - 4.97;
        }
    } else {
        /* Small/medium city */
        return (1.1 * log10(freq_mhz) - 0.7) * rx_height_m -
               (1.56 * log10(freq_mhz) - 0.8);
    }
}

double pathloss_okumura_hata_urban(double distance_km, double freq_mhz,
                                    double tx_height_m, double rx_height_m,
                                    int is_large_city)
{
    if (distance_km <= 0.0 || freq_mhz <= 0.0 ||
        tx_height_m <= 0.0 || rx_height_m <= 0.0) {
        return -1.0;
    }

    double a_hm = pathloss_okumura_hata_correction(rx_height_m, freq_mhz,
                                                    is_large_city);

    /* PL_urban = 69.55 + 26.16*log10(f) - 13.82*log10(h_b)
     *            + (44.9 - 6.55*log10(h_b))*log10(d) - a(h_m) */
    double pl_db = 69.55 +
                   26.16 * log10(freq_mhz) -
                   13.82 * log10(tx_height_m) +
                   (44.9 - 6.55 * log10(tx_height_m)) * log10(distance_km) -
                   a_hm;

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

double pathloss_okumura_hata_suburban(double distance_km, double freq_mhz,
                                       double tx_height_m, double rx_height_m)
{
    double pl_urban = pathloss_okumura_hata_urban(distance_km, freq_mhz,
                                                   tx_height_m, rx_height_m, 0);
    if (pl_urban < 0.0) return -1.0;

    /* Suburban correction:
     * PL_sub = PL_urban - 2*[log10(f/28)]^2 - 5.4 */
    double correction = 2.0 * pow(log10(freq_mhz / 28.0), 2.0) + 5.4;
    double pl_db = pl_urban - correction;

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

double pathloss_okumura_hata_rural(double distance_km, double freq_mhz,
                                    double tx_height_m, double rx_height_m)
{
    double pl_urban = pathloss_okumura_hata_urban(distance_km, freq_mhz,
                                                   tx_height_m, rx_height_m, 0);
    if (pl_urban < 0.0) return -1.0;

    /* Rural correction:
     * PL_rural = PL_urban - 4.78*[log10(f)]^2 + 18.33*log10(f) - 40.94 */
    double logf = log10(freq_mhz);
    double correction = 4.78 * logf * logf - 18.33 * logf + 40.94;
    double pl_db = pl_urban - correction;

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L4: COST-231 Hata Model
 *============================================================================*/

double pathloss_cost231_hata(double distance_km, double freq_mhz,
                              double tx_height_m, double rx_height_m,
                              int is_metropolitan)
{
    if (distance_km <= 0.0 || freq_mhz <= 0.0 ||
        tx_height_m <= 0.0 || rx_height_m <= 0.0) {
        return -1.0;
    }

    /* a(h_m) uses small/medium city correction for COST-231 */
    double a_hm = (1.1 * log10(freq_mhz) - 0.7) * rx_height_m -
                  (1.56 * log10(freq_mhz) - 0.8);

    double c_m = is_metropolitan ? 3.0 : 0.0;

    /* PL = 46.3 + 33.9*log10(f) - 13.82*log10(h_b) - a(h_m)
     *      + (44.9 - 6.55*log10(h_b))*log10(d) + C_M */
    double pl_db = 46.3 +
                   33.9 * log10(freq_mhz) -
                   13.82 * log10(tx_height_m) - a_hm +
                   (44.9 - 6.55 * log10(tx_height_m)) * log10(distance_km) +
                   c_m;

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L4: Walfisch-Ikegami Model (COST-231 Microcell)
 *============================================================================*/

double pathloss_walfisch_ikegami_los(double distance_km, double freq_mhz)
{
    if (distance_km <= 0.0 || freq_mhz <= 0.0) return -1.0;

    /* LOS in street canyon:
     * PL = 42.6 + 26*log10(d_km) + 20*log10(f_MHz) */
    double pl_db = 42.6 + 26.0 * log10(distance_km) + 20.0 * log10(freq_mhz);

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

double pathloss_walfisch_ikegami_nlos(double distance_km, double freq_mhz,
                                       double tx_height_m, double rx_height_m,
                                       double building_height_m,
                                       double street_width_m,
                                       double building_spacing_m,
                                       double street_angle_deg)
{
    if (distance_km <= 0.0 || freq_mhz <= 0.0) return -1.0;

    /* NLOS = L_0 + L_rts + L_msd (if L_rts + L_msd > 0)
     *   L_0 = free-space loss
     *   L_rts = rooftop-to-street diffraction
     *   L_msd = multi-screen diffraction loss */

    double distance_m = distance_km * 1000.0;
    double freq_hz = freq_mhz * 1e6;

    /* Free-space loss L_0 */
    double l_0 = pathloss_friis_free_space(distance_m, freq_hz);

    /* Rooftop-to-street diffraction L_rts:
     * L_rts = -16.9 - 10*log10(w) + 10*log10(f) + 20*log10(delta_h_m) + L_ori */
    double delta_h_m = building_height_m - rx_height_m;
    if (delta_h_m < 0.0) delta_h_m = 0.0;

    double l_rts = -16.9 - 10.0 * log10(street_width_m) +
                   10.0 * log10(freq_mhz) + 20.0 * log10(delta_h_m);

    /* Orientation loss L_ori:
     *  -10 + 0.354*phi     for 0 <= phi < 35 deg
     *   2.5 + 0.075*(phi-35) for 35 <= phi < 55 deg
     *   4.0 - 0.114*(phi-55) for 55 <= phi <= 90 deg */
    double phi = street_angle_deg;
    double l_ori;
    if (phi < 35.0) {
        l_ori = -10.0 + 0.354 * phi;
    } else if (phi < 55.0) {
        l_ori = 2.5 + 0.075 * (phi - 35.0);
    } else {
        l_ori = 4.0 - 0.114 * (phi - 55.0);
    }
    l_rts += l_ori;

    /* Multi-screen diffraction L_msd:
     * L_msd = L_bsh + k_a + k_d*log10(d) + k_f*log10(f) - 9*log10(b) */
    double b = building_spacing_m;
    double delta_h_b = tx_height_m - building_height_m;

    double l_bsh;
    if (delta_h_b > 0.0) {
        l_bsh = -18.0 * log10(1.0 + delta_h_b);
    } else {
        l_bsh = 0.0;
    }

    double k_a;
    if (delta_h_b > 0.0) {
        k_a = 54.0;
    } else if (distance_km >= 0.5) {
        k_a = 54.0 - 0.8 * delta_h_b;
    } else {
        k_a = 54.0 - 0.8 * delta_h_b * (distance_km / 0.5);
    }

    double k_d;
    if (delta_h_b > 0.0) {
        k_d = 18.0;
    } else {
        k_d = 18.0 - 15.0 * delta_h_b / building_height_m;
    }

    double k_f = -4.0;
    /* k_f adjustment for medium city / metropolitan */
    if (freq_mhz < 1800.0) {
        k_f += 0.7 * (freq_mhz / 925.0 - 1.0);
    } else {
        k_f += 1.5 * (freq_mhz / 925.0 - 1.0);
    }

    double l_msd = l_bsh + k_a + k_d * log10(distance_km) +
                   k_f * log10(freq_mhz) - 9.0 * log10(b);

    /* Total NLOS loss */
    double l_rts_msd = l_rts + l_msd;
    if (l_rts_msd < 0.0) {
        l_rts_msd = 0.0;
    }

    double pl_db = l_0 + l_rts_msd;
    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L4: ITU-R P.1238 Indoor Propagation
 *============================================================================*/

double pathloss_itu_indoor(double distance_m, double freq_mhz,
                            double path_loss_coeff, int num_floors,
                            double floor_loss_db)
{
    if (distance_m <= 0.0 || freq_mhz <= 0.0) return -1.0;

    /* PL(dB) = 20*log10(f) + N*log10(d) + L_f(n) - 28
     * For residential (same floor): L_f = 0
     * For office: L_f = 15 + 4*(n-1) for n >= 1 */
    double l_f = 0.0;
    if (num_floors > 0) {
        l_f = 15.0 + 4.0 * (double)(num_floors - 1);
        l_f += floor_loss_db * (double)num_floors;
    }

    double pl_db = 20.0 * log10(freq_mhz) +
                   path_loss_coeff * log10(distance_m) +
                   l_f - 28.0;

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L7: 3GPP TR 38.901 Path Loss Models (5G NR Applications)
 *============================================================================*/

double pathloss_3gpp_umi(double distance_m, double freq_ghz,
                          double tx_height_m, double rx_height_m, int is_los)
{
    if (distance_m <= 0.0 || freq_ghz <= 0.0) return -1.0;

    /* Breakpoint distance for UMi:
     * d_BP = 4 * (h_BS - 1) * (h_UT - 1) * f_c / c */
    double h_bs_eff = tx_height_m - 1.0;
    double h_ut_eff = rx_height_m - 1.0;
    double d_bp = 4.0 * h_bs_eff * h_ut_eff * freq_ghz * 1e9 / CHANNEL_C0;

    double pl_db;
    if (is_los) {
        if (distance_m < d_bp) {
            /* PL1 = 32.4 + 21*log10(d_3D) + 20*log10(f_c) */
            pl_db = 32.4 + 21.0 * log10(distance_m) + 20.0 * log10(freq_ghz);
        } else {
            /* PL2 = 32.4 + 40*log10(d_3D) + 20*log10(f_c)
             *        - 9.5*log10(d_BP^2 + (h_BS-h_UT)^2) */
            double h_diff = tx_height_m - rx_height_m;
            pl_db = 32.4 + 40.0 * log10(distance_m) + 20.0 * log10(freq_ghz) -
                    9.5 * log10(d_bp * d_bp + h_diff * h_diff);
        }
    } else {
        /* NLOS: PL = max(PL_UMi-LOS, PL_UMi-NLOS)
         * PL_UMi-NLOS = 35.3*log10(d_3D) + 22.4 + 21.3*log10(f_c)
         *                - 0.3*(h_UT - 1.5) */
        double pl_los = (distance_m < d_bp) ?
            32.4 + 21.0 * log10(distance_m) + 20.0 * log10(freq_ghz) :
            32.4 + 40.0 * log10(distance_m) + 20.0 * log10(freq_ghz) -
            9.5 * log10(d_bp * d_bp + pow(tx_height_m - rx_height_m, 2.0));

        double pl_nlos = 35.3 * log10(distance_m) + 22.4 +
                         21.3 * log10(freq_ghz) -
                         0.3 * (rx_height_m - 1.5);

        pl_db = (pl_los > pl_nlos) ? pl_los : pl_nlos;
    }

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

double pathloss_3gpp_uma(double distance_m, double freq_ghz,
                          double tx_height_m, double rx_height_m, int is_los)
{
    if (distance_m <= 0.0 || freq_ghz <= 0.0) return -1.0;

    /* Effective antenna heights for UMa */
    double h_bs_eff = tx_height_m; /* typ 25 m */
    double h_ut_eff = rx_height_m; /* typ 1.5 m */
    double h_e = 1.0; /* effective environment height */

    double d_bp = 4.0 * (h_bs_eff - h_e) * (h_ut_eff - h_e) *
                  freq_ghz * 1e9 / CHANNEL_C0;

    double pl_db;
    if (is_los) {
        if (distance_m < d_bp) {
            pl_db = 28.0 + 22.0 * log10(distance_m) + 20.0 * log10(freq_ghz);
        } else {
            pl_db = 28.0 + 40.0 * log10(distance_m) + 20.0 * log10(freq_ghz) -
                    9.0 * log10(d_bp * d_bp +
                                pow(h_bs_eff - h_ut_eff, 2.0));
        }
    } else {
        /* UMa NLOS:
         * PL = 13.54 + 39.08*log10(d_3D) + 20*log10(f_c)
         *      - 0.6*(h_UT - 1.5) */
        double pl_los = (distance_m < d_bp) ?
            28.0 + 22.0 * log10(distance_m) + 20.0 * log10(freq_ghz) :
            28.0 + 40.0 * log10(distance_m) + 20.0 * log10(freq_ghz) -
            9.0 * log10(d_bp * d_bp + pow(h_bs_eff - h_ut_eff, 2.0));

        double pl_nlos = 13.54 + 39.08 * log10(distance_m) +
                         20.0 * log10(freq_ghz) -
                         0.6 * (rx_height_m - 1.5);

        pl_db = (pl_los > pl_nlos) ? pl_los : pl_nlos;
    }

    if (pl_db < 0.0) pl_db = 0.0;
    return pl_db;
}

/*============================================================================
 * L2: Generic Path Loss Computation
 *============================================================================*/

double pathloss_compute(const pathloss_params_t *params, double distance_m)
{
    if (!params || distance_m <= 0.0) return -1.0;

    double freq_mhz, freq_ghz;

    switch (params->model) {
    case PATHLOSS_FREE_SPACE:
        return pathloss_friis_free_space(distance_m, params->carrier_freq_hz);

    case PATHLOSS_TWO_RAY:
        return pathloss_two_ray(distance_m, params->tx_height_m,
                                params->rx_height_m, params->carrier_freq_hz);

    case PATHLOSS_LOG_DISTANCE:
        return pathloss_log_distance(params, distance_m);

    case PATHLOSS_OKUMURA_HATA:
        freq_mhz = params->carrier_freq_hz / 1e6;
        return pathloss_okumura_hata_urban(distance_m / 1000.0, freq_mhz,
                                            params->tx_height_m,
                                            params->rx_height_m, 0);

    case PATHLOSS_COST231_HATA:
        freq_mhz = params->carrier_freq_hz / 1e6;
        return pathloss_cost231_hata(distance_m / 1000.0, freq_mhz,
                                      params->tx_height_m,
                                      params->rx_height_m, 0);

    case PATHLOSS_WALFISCH_IKEGAMI:
        freq_mhz = params->carrier_freq_hz / 1e6;
        return pathloss_walfisch_ikegami_nlos(
            distance_m / 1000.0, freq_mhz,
            params->tx_height_m, params->rx_height_m,
            params->building_height_m,
            params->street_width_m,
            params->building_spacing_m,
            params->street_angle_deg);

    case PATHLOSS_ITU_INDOOR:
        freq_mhz = params->carrier_freq_hz / 1e6;
        return pathloss_itu_indoor(distance_m, freq_mhz,
                                    params->path_loss_exponent, 0, 0.0);

    case PATHLOSS_3GPP_UMI:
        freq_ghz = params->carrier_freq_hz / 1e9;
        return pathloss_3gpp_umi(distance_m, freq_ghz,
                                  params->tx_height_m,
                                  params->rx_height_m, 1);

    case PATHLOSS_3GPP_UMA:
        freq_ghz = params->carrier_freq_hz / 1e9;
        return pathloss_3gpp_uma(distance_m, freq_ghz,
                                  params->tx_height_m,
                                  params->rx_height_m, 1);

    default:
        return -1.0;
    }
}

int pathloss_validate_params(const pathloss_params_t *params)
{
    if (!params) return -1;

    if (params->carrier_freq_hz <= 0.0) return -2;
    if (params->tx_height_m <= 0.0) return -3;
    if (params->rx_height_m <= 0.0) return -4;

    switch (params->model) {
    case PATHLOSS_OKUMURA_HATA:
        if (params->carrier_freq_hz < 150e6 || params->carrier_freq_hz > 1500e6)
            return -5;
        break;
    case PATHLOSS_COST231_HATA:
        if (params->carrier_freq_hz < 1500e6 || params->carrier_freq_hz > 2000e6)
            return -5;
        break;
    default:
        break;
    }

    return 0;
}

const char *pathloss_model_name(pathloss_model_t model)
{
    static const char *names[] = {
        "Friis Free-Space",
        "Two-Ray Ground Reflection",
        "Log-Distance Empirical",
        "Okumura-Hata",
        "COST-231 Hata",
        "Walfisch-Ikegami",
        "ITU-R P.1238 Indoor",
        "3GPP TR 38.901 UMi",
        "3GPP TR 38.901 UMa",
        "3GPP TR 38.901 RMa",
        "3GPP TR 38.901 InH",
        "WINNER II",
        "Custom"
    };
    if (model < 0 || model > PATHLOSS_CUSTOM) return "Unknown";
    return names[model];
}
