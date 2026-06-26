/**
 * nr_phy_common.h — 5G NR Physical Layer: Common Types, Numerology & Frame
 *
 * Knowledge Coverage:
 *   L1 Definitions: Numerology (mu), subcarrier spacing, frame/slot/symbol,
 *                   resource block, resource element, bandwidth part, CORESET
 *   L2 Core Concepts: OFDM numerologies, TDD/FDD, slot formats, CP types
 *   L4 Fundamental Laws: Nyquist sampling in 5G bandwidth, time-freq lattice
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless Communications (5G NR)
 *   MIT 6.450      — Digital Communications (PHY layer)
 *   Berkeley EE123 — DSP in communications
 *
 * References:
 *   3GPP TS 38.211 v17.0.0: Physical channels and modulation
 *   3GPP TS 38.213 v17.0.0: Physical layer procedures for control
 *   3GPP TS 38.214 v17.0.0: Physical layer procedures for data
 *   Dahlman, Parkvall & Skold (2020): "5G NR" 2nd ed., Academic Press
 */

#ifndef NR_PHY_COMMON_H
#define NR_PHY_COMMON_H

#define _USE_MATH_DEFINES
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NR_MAX_MU               4
#define NR_NUM_SC_PER_RB        12
#define NR_MAX_RB               275
#define NR_SYMBOLS_PER_SLOT_NCP 14
#define NR_SYMBOLS_PER_SLOT_ECP 12
#define NR_SLOTS_PER_FRAME_MU0  10
#define NR_SUBFRAMES_PER_FRAME  10
#define NR_FRAME_DURATION_MS    10.0
#define NR_SUBFRAME_DURATION_MS 1.0
#define NR_MAX_LAYERS_DL        8
#define NR_MAX_LAYERS_UL        4
#define NR_MAX_PORTS_DL         32
#define NR_MAX_ANTENNA_PORTS    32
#define NR_MAX_BWP              4
#define NR_MIN_BWP_RB           24

/** Numerology descriptor — encodes mu and derived parameters */
typedef struct {
    int     mu;
    double  scs_khz;
    double  symbol_duration_us;
    double  nominal_cp_us;
    int     slots_per_subframe;
    int     slots_per_frame;
    int     symbols_per_slot;
    int     max_rb;
    double  min_carrier_freq_ghz;
    double  max_carrier_freq_ghz;
} nr_numerology_t;

/** Logical RE index */
typedef struct {
    int     frame;
    int     subframe;
    int     slot;
    int     symbol;
    int     subcarrier;
    int     prb;
    int     antenna_port;
} nr_re_index_t;

typedef struct {
    int     start_prb;
    int     num_prb;
    int     bwp_id;
} nr_prb_alloc_t;

/** Bandwidth Part configuration (3GPP TS 38.211 4.4.5) */
typedef struct {
    int     bwp_id;
    int     start_prb;
    int     num_prb;
    int     numerology_mu;
    int     cp_type;
    double  center_freq_hz;
    int     is_active;
} nr_bwp_config_t;

/** Carrier configuration */
typedef struct {
    double          center_freq_hz;
    double          bandwidth_hz;
    int             numerology_mu;
    int             num_prb;
    int             num_bwp;
    nr_bwp_config_t bwps[NR_MAX_BWP];
    int             active_bwp_id;
    int             cp_type;
    int             duplex_mode;
} nr_carrier_config_t;

typedef enum {
    NR_SLOT_DL       = 0,
    NR_SLOT_UL       = 1,
    NR_SLOT_FLEXIBLE = 2,
    NR_SLOT_DL_UL    = 3
} nr_slot_format_t;

typedef enum {
    NR_SYM_DL        = 0,
    NR_SYM_UL        = 1,
    NR_SYM_FLEXIBLE  = 2,
    NR_SYM_GUARD     = 3
} nr_symbol_type_t;

typedef struct {
    nr_slot_format_t format;
    nr_symbol_type_t symbols[NR_SYMBOLS_PER_SLOT_NCP];
    int              num_dl_symbols;
    int              num_ul_symbols;
    int              num_flexible_symbols;
} nr_slot_pattern_t;

/** CORESET configuration (3GPP TS 38.211 7.3.2) */
typedef struct {
    int     coreset_id;
    int     start_symbol;
    int     duration_symbols;
    int     num_prb;
    int     start_prb;
    int     reg_bundle_size;
    int     interleaver_size;
    int     shift_index;
    int     precoder_granularity;
} nr_coreset_config_t;

/** Search space (3GPP TS 38.213 10.1) */
typedef struct {
    int     search_space_id;
    int     coreset_id;
    int     monitoring_period_slots;
    int     monitoring_offset;
    int     duration_slots;
    int     num_candidates[5];
    int     dci_format_mask;
} nr_search_space_t;

typedef enum {
    NR_CHAN_PBCH   = 0,
    NR_CHAN_PDCCH  = 1,
    NR_CHAN_PDSCH  = 2,
    NR_CHAN_PUSCH  = 3,
    NR_CHAN_PUCCH  = 4,
    NR_CHAN_PRACH  = 5,
    NR_CHAN_SSB    = 6,
    NR_CHAN_CSI_RS = 7,
    NR_CHAN_DMRS   = 8,
    NR_CHAN_PTRS   = 9,
    NR_CHAN_SRS    = 10
} nr_chan_type_t;

typedef enum {
    NR_MOD_BPSK    = 0,
    NR_MOD_QPSK    = 1,
    NR_MOD_QAM16   = 2,
    NR_MOD_QAM64   = 3,
    NR_MOD_QAM256  = 4,
    NR_MOD_QAM1024 = 5
} nr_mod_scheme_t;

typedef struct {
    int             mcs_index;
    nr_mod_scheme_t modulation;
    double          code_rate;
    double          spectral_efficiency;
} nr_mcs_entry_t;

typedef struct {
    int             tbsize_bytes;
    int             num_layers;
    int             num_prb;
    int             coding_rate_x1024;
    nr_mod_scheme_t modulation;
    int             redundancy_version;
    int             new_data_indicator;
} nr_tb_config_t;

typedef struct {
    int     dmrs_type;
    int     max_len;
    int     num_additional_pos;
    int     scrambling_id;
    int     num_dmrs_ports;
    int     dmrs_ports[NR_MAX_ANTENNA_PORTS];
} nr_dmrs_config_t;

typedef struct {
    int     prach_format;
    int     root_sequence_index;
    int     zero_corr_zone_config;
    int     prach_freq_offset;
    int     prach_time_offset;
    int     preamble_index;
    double  target_power_dbm;
} nr_prach_config_t;

typedef struct {
    double re;
    double im;
} nr_complex_t;

/* L2 API */
int nr_numerology_init(nr_numerology_t *num, int mu, int cp);
int nr_carrier_config_init(nr_carrier_config_t *cfg,
                            double freq_hz, double bw_hz,
                            int mu, int duplex_mode);
int nr_bwp_configure(nr_carrier_config_t *cfg, int bwp_id,
                      int start_rb, int num_rb, int mu);
void nr_re_index_to_position(const nr_re_index_t *idx,
                               const nr_carrier_config_t *cfg,
                               double *t_sec, double *f_hz);
int nr_prb_count_for_bw(double bw_mhz, int mu);
int nr_tbs_calculate(int n_re, int code_rate, int mod_order, int n_layers);
int nr_modulation_order(nr_mod_scheme_t mod);
double nr_scs_khz(int mu);
int nr_symbols_per_slot(int cp_type);
int nr_slots_per_frame(int mu);
int nr_slots_per_subframe(int mu);
int nr_fft_size_min(int mu, int num_prb);
int nr_cp_length(int symbol_idx, int fft_size, int mu);
int nr_is_fr1(double freq_hz);
int nr_is_fr2(double freq_hz);
int nr_mcs_lookup(int mcs_index, int table_id, nr_mcs_entry_t *entry);
int nr_mcs_from_efficiency(double target_se, int table_id);

/* Inline complex arithmetic */
static inline nr_complex_t nr_complex_make(double re, double im) {
    nr_complex_t c; c.re = re; c.im = im; return c;
}
static inline nr_complex_t nr_complex_add(nr_complex_t a, nr_complex_t b) {
    nr_complex_t c; c.re = a.re + b.re; c.im = a.im + b.im; return c;
}
static inline nr_complex_t nr_complex_sub(nr_complex_t a, nr_complex_t b) {
    nr_complex_t c; c.re = a.re - b.re; c.im = a.im - b.im; return c;
}
static inline nr_complex_t nr_complex_mul(nr_complex_t a, nr_complex_t b) {
    nr_complex_t c;
    c.re = a.re * b.re - a.im * b.im;
    c.im = a.re * b.im + a.im * b.re;
    return c;
}
static inline nr_complex_t nr_complex_conj(nr_complex_t a) {
    nr_complex_t c; c.re = a.re; c.im = -a.im; return c;
}
static inline double nr_complex_abs_sq(nr_complex_t a) {
    return a.re * a.re + a.im * a.im;
}
static inline double nr_complex_abs(nr_complex_t a) {
    return sqrt(a.re * a.re + a.im * a.im);
}
static inline nr_complex_t nr_complex_scale(nr_complex_t a, double s) {
    nr_complex_t c; c.re = a.re * s; c.im = a.im * s; return c;
}
static inline nr_complex_t nr_complex_expj(double theta) {
    nr_complex_t c; c.re = cos(theta); c.im = sin(theta); return c;
}

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_COMMON_H */
