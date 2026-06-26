/**
 * nr_phy_mimo.h — 5G NR MIMO Processing
 *
 * Knowledge Coverage:
 *   L1 Definitions: Layers, antenna ports, PMI, RI, CQI
 *   L2 Core Concepts: Spatial multiplexing, transmit diversity, beamforming
 *   L3 Math Structures: MIMO channel matrix H, SVD decomposition
 *   L4 Fundamental Laws: MIMO capacity (Telatar 1999)
 *   L5 Algorithms: SVD precoding, MMSE MIMO detection, codebook search
 *   L6 Canonical Problems: 5G Type I codebook, CSI reporting
 *   L8 Advanced: Massive MIMO, MU-MIMO
 *
 * Course: Stanford EE359, MIT 6.450
 * Ref: 3GPP TS 38.211 6.3.1, TS 38.214 5.2.2
 */

#ifndef NR_PHY_MIMO_H
#define NR_PHY_MIMO_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NR_MIMO_MAX_LAYERS      8
#define NR_MIMO_MAX_TX_PORTS    32
#define NR_MIMO_MAX_RX_PORTS    8

typedef struct {
    int     num_tx_ports;
    int     num_rx_ports;
    int     num_layers;
    int     codebook_mode;
    int     panel_dims[2];
    int     oversampling[2];
    int     i1_1, i1_2, i1_3, i2;
    int     polarization;
} nr_mimo_config_t;

typedef struct {
    int     rank;
    int     pmi_i1_1, pmi_i1_2, pmi_i1_3, pmi_i2;
    int     cqi;
    double  sinr_per_layer[NR_MIMO_MAX_LAYERS];
    double  capacity_bps_per_hz;
} nr_csi_report_t;

typedef struct {
    int     num_tx;
    int     num_rx;
    double *H_real;
    double *W;
    double *Q;
    double *R;
    double  noise_var;
} nr_mimo_det_ctx_t;

void nr_mimo_config_init(nr_mimo_config_t *cfg,
                          int n_tx, int n_rx, int n_layers,
                          int n1, int n2, int o1, int o2, int dual_pol);

int nr_mimo_codebook_type1(const nr_mimo_config_t *cfg,
                            nr_complex_t *W);

void nr_mimo_precode(const nr_complex_t *W, int n_tx, int n_layers,
                      const nr_complex_t *sym_in,
                      nr_complex_t *sym_out);

void nr_mimo_channel_matrix(int n_rx, int n_tx,
                             const nr_complex_t *h_flat,
                             nr_complex_t *H);

int nr_mimo_det_init(nr_mimo_det_ctx_t *ctx,
                      int n_tx, int n_rx,
                      const nr_complex_t *H,
                      double noise_var);

void nr_mimo_det_zf(const nr_mimo_det_ctx_t *ctx,
                     const nr_complex_t *rx_symbols,
                     nr_complex_t *tx_estimates);

void nr_mimo_det_mmse(const nr_mimo_det_ctx_t *ctx,
                       const nr_complex_t *rx_symbols,
                       nr_complex_t *tx_estimates);

void nr_mimo_det_mmse_sic(const nr_mimo_det_ctx_t *ctx,
                           const nr_complex_t *rx_symbols,
                           const int *order,
                           nr_complex_t *tx_estimates);

void nr_mimo_det_free(nr_mimo_det_ctx_t *ctx);

void nr_mimo_det_layers_sinr(const nr_mimo_det_ctx_t *ctx,
                              double *sinr_per_layer);

double nr_mimo_capacity(int n_rx, int n_tx,
                         const nr_complex_t *H,
                         double snr_lin);

double nr_mimo_waterfilling(const double *eigenvals, int n_modes,
                             double p_total, double noise_var,
                             double *p_alloc);

void nr_mimo_svd(int n_rx, int n_tx,
                 const nr_complex_t *H,
                 nr_complex_t *U, double *S, nr_complex_t *V);

double nr_mimo_condition_number(int n_rx, int n_tx,
                                 const nr_complex_t *H);

int nr_mimo_rank_estimate(int n_rx, int n_tx,
                           const nr_complex_t *H,
                           double snr_lin,
                           double min_sinr_per_layer_lin);

void nr_mimo_mf_precoder(int n_rx, int n_tx,
                          const nr_complex_t *H,
                          nr_complex_t *W);

int nr_mimo_zf_precoder(int n_rx, int n_tx,
                         const nr_complex_t *H,
                         nr_complex_t *W);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_MIMO_H */