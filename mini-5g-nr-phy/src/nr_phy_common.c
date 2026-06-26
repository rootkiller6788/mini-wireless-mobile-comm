/**
 * nr_phy_common.c — 5G NR PHY Common Types Implementation
 *
 * Implements 3GPP TS 38.211 / 38.213 / 38.214 numerology,
 * frame structure, bandwidth part, MCS table lookups, TBS calculation.
 */

#include "nr_phy_common.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * L2: Numerology Initialization
 * ============================================================================ */

int nr_numerology_init(nr_numerology_t *num, int mu, int cp)
{
    if (!num) return -1;
    if (mu < 0 || mu > NR_MAX_MU) return -1;

    /* Extended CP only supported for mu = 2 in FR2 */
    if (cp == 1 && mu != 2) return -1;

    memset(num, 0, sizeof(*num));
    num->mu = mu;
    num->scs_khz = 15.0 * (1 << mu);

    /* Useful symbol duration T_u = 1 / Delta_f */
    num->symbol_duration_us = 1000.0 / num->scs_khz;

    /* Slots scale as 2^mu per subframe */
    num->slots_per_subframe = 1 << mu;
    num->slots_per_frame = 10 * num->slots_per_subframe;

    /* Symbol count per slot */
    if (cp == 0) {
        num->symbols_per_slot = NR_SYMBOLS_PER_SLOT_NCP;
        /* Nominal CP ≈ 144 * kappa * T_c / 2^mu, ~4.7 us for mu=0 */
        num->nominal_cp_us = 144.0 * 64.0 / (122.88 * (1 << mu));
    } else {
        num->symbols_per_slot = NR_SYMBOLS_PER_SLOT_ECP;
        num->nominal_cp_us = 512.0 * 64.0 / (122.88 * (1 << mu));
    }

    /* Max RBs at 100 MHz channel bandwidth */
    num->max_rb = nr_prb_count_for_bw(100.0, mu);

    /* Applicable frequency ranges */
    if (mu <= 2) {
        num->min_carrier_freq_ghz = 0.41;  /* FR1 starts at 410 MHz */
        num->max_carrier_freq_ghz = 7.125; /* FR1 upper limit */
    } else {
        num->min_carrier_freq_ghz = 24.25; /* FR2 starts at 24.25 GHz */
        num->max_carrier_freq_ghz = 52.6;  /* FR2 upper limit (Rel-17) */
    }

    return 0;
}

/* ============================================================================
 * L2: Carrier Configuration
 * ============================================================================ */

int nr_carrier_config_init(nr_carrier_config_t *cfg,
                            double freq_hz, double bw_hz,
                            int mu, int duplex_mode)
{
    if (!cfg) return -1;
    if (mu < 0 || mu > NR_MAX_MU) return -1;
    if (bw_hz <= 0 || freq_hz <= 0) return -1;

    memset(cfg, 0, sizeof(*cfg));
    cfg->center_freq_hz = freq_hz;
    cfg->bandwidth_hz = bw_hz;
    cfg->numerology_mu = mu;
    cfg->duplex_mode = duplex_mode;

    /* Determine CP type based on mu */
    /* Extended CP only for mu=2 in specific scenarios */
    cfg->cp_type = 0;  /* Normal CP is default */

    /* Calculate number of PRBs for this bandwidth */
    double bw_mhz = bw_hz / 1.0e6;
    cfg->num_prb = nr_prb_count_for_bw(bw_mhz, mu);
    if (cfg->num_prb <= 0) return -1;

    /* Default BWP covering the entire carrier */
    cfg->num_bwp = 1;
    cfg->bwps[0].bwp_id = 0;
    cfg->bwps[0].start_prb = 0;
    cfg->bwps[0].num_prb = cfg->num_prb;
    cfg->bwps[0].numerology_mu = mu;
    cfg->bwps[0].cp_type = cfg->cp_type;
    cfg->bwps[0].center_freq_hz = freq_hz;
    cfg->bwps[0].is_active = 1;
    cfg->active_bwp_id = 0;

    return 0;
}

/* ============================================================================
 * L2: Bandwidth Part Configuration
 * ============================================================================ */

int nr_bwp_configure(nr_carrier_config_t *cfg, int bwp_id,
                      int start_rb, int num_rb, int mu)
{
    if (!cfg) return -1;
    if (bwp_id < 0 || bwp_id >= NR_MAX_BWP) return -1;
    if (num_rb < NR_MIN_BWP_RB) return -1;
    if (start_rb < 0 || (start_rb + num_rb) > cfg->num_prb) return -1;

    /* Check if BWP already exists */
    int idx = -1;
    for (int i = 0; i < cfg->num_bwp; i++) {
        if (cfg->bwps[i].bwp_id == bwp_id) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        /* New BWP */
        if (cfg->num_bwp >= NR_MAX_BWP) return -1;
        idx = cfg->num_bwp++;
    }

    cfg->bwps[idx].bwp_id = bwp_id;
    cfg->bwps[idx].start_prb = start_rb;
    cfg->bwps[idx].num_prb = num_rb;
    cfg->bwps[idx].numerology_mu = mu;
    cfg->bwps[idx].cp_type = cfg->cp_type;
    cfg->bwps[idx].is_active = (bwp_id == cfg->active_bwp_id);

    /* BWP center frequency offset from carrier center */
    double bw_per_prb = nr_scs_khz(mu) * NR_NUM_SC_PER_RB;
    int prb_offset = start_rb + num_rb / 2 - cfg->num_prb / 2;
    cfg->bwps[idx].center_freq_hz = cfg->center_freq_hz +
                                     prb_offset * bw_per_prb * 1000.0;

    return 0;
}

/* ============================================================================
 * L2: RE Index to Time-Frequency Position
 * ============================================================================ */

void nr_re_index_to_position(const nr_re_index_t *idx,
                               const nr_carrier_config_t *cfg,
                               double *t_sec, double *f_hz)
{
    if (!idx || !cfg || !t_sec || !f_hz) return;

    nr_numerology_t num;
    if (nr_numerology_init(&num, cfg->numerology_mu, cfg->cp_type) != 0) {
        *t_sec = 0.0; *f_hz = 0.0;
        return;
    }

    /* Time calculation */
    /* Frame number to seconds */
    double t = idx->frame * 0.010;

    /* Subframe: 0.001 seconds each */
    t += idx->subframe * 0.001;

    /* Slot within subframe: (1/N_slots_per_subframe) ms */
    double slot_dur = 0.001 / num.slots_per_subframe;
    t += idx->slot * slot_dur;

    /* Symbol within slot: each symbol is T_u + T_cp seconds */
    int fft_sz = nr_fft_size_min(cfg->numerology_mu, cfg->num_prb);
    for (int s = 0; s < idx->symbol; s++) {
        int cp_samples = nr_cp_length(s, fft_sz, cfg->numerology_mu);
        double sym_len = (fft_sz + cp_samples) / (fft_sz * num.scs_khz * 1000.0);
        t += sym_len;
    }

    *t_sec = t;

    /* Frequency calculation */
    double scs_hz = num.scs_khz * 1000.0;
    double f = cfg->center_freq_hz;
    f += (idx->prb - cfg->num_prb / 2) * NR_NUM_SC_PER_RB * scs_hz;
    f += (idx->subcarrier - NR_NUM_SC_PER_RB / 2) * scs_hz;
    *f_hz = f;
}

/* ============================================================================
 * L2: PRB Count for Bandwidth (3GPP TS 38.101 Table 5.3.2-1)
 * ============================================================================ */

int nr_prb_count_for_bw(double bw_mhz, int mu)
{
    /* Table 5.3.2-1: Max transmission bandwidth N_RB for FR1 */
    /* SCS: 15, 30, 60 kHz for FR1 */
    if (mu < 0 || mu > 4) return 0;

    /* Number of RBs for given BW and SCS */
    /* Conservative values incorporating guard bands */

    if (mu == 0) { /* 15 kHz SCS */
        if (bw_mhz <= 5.0)   return 25;
        if (bw_mhz <= 10.0)  return 52;
        if (bw_mhz <= 15.0)  return 79;
        if (bw_mhz <= 20.0)  return 106;
        if (bw_mhz <= 25.0)  return 133;
        if (bw_mhz <= 30.0)  return 160;
        if (bw_mhz <= 40.0)  return 216;
        if (bw_mhz <= 50.0)  return 270;
        return 270; /* Max for 50MHz+ at 15kHz */
    } else if (mu == 1) { /* 30 kHz SCS */
        if (bw_mhz <= 5.0)   return 11;
        if (bw_mhz <= 10.0)  return 24;
        if (bw_mhz <= 15.0)  return 38;
        if (bw_mhz <= 20.0)  return 51;
        if (bw_mhz <= 25.0)  return 65;
        if (bw_mhz <= 30.0)  return 78;
        if (bw_mhz <= 40.0)  return 106;
        if (bw_mhz <= 50.0)  return 133;
        if (bw_mhz <= 60.0)  return 162;
        if (bw_mhz <= 80.0)  return 217;
        if (bw_mhz <= 100.0) return 273;
        return 273;
    } else if (mu == 2) { /* 60 kHz SCS */
        if (bw_mhz <= 10.0)  return 11;
        if (bw_mhz <= 15.0)  return 18;
        if (bw_mhz <= 20.0)  return 24;
        if (bw_mhz <= 25.0)  return 31;
        if (bw_mhz <= 30.0)  return 38;
        if (bw_mhz <= 40.0)  return 51;
        if (bw_mhz <= 50.0)  return 65;
        if (bw_mhz <= 60.0)  return 79;
        if (bw_mhz <= 80.0)  return 107;
        if (bw_mhz <= 100.0) return 135;
        if (bw_mhz <= 200.0) return 264;
        return 264;
    } else { /* mu >= 3: 120/240 kHz SCS, FR2 */
        if (bw_mhz <= 50.0)   return 32;
        if (bw_mhz <= 100.0)  return 66;
        if (bw_mhz <= 200.0)  return 132;
        if (bw_mhz <= 400.0)  return 264;
        return 264;
    }
}

/* ============================================================================
 * L2: TBS Calculation (3GPP TS 38.214 Section 5.1.3.2)
 * ============================================================================ */

int nr_tbs_calculate(int n_re, int code_rate, int mod_order, int n_layers)
{
    if (n_re <= 0 || code_rate <= 0 || mod_order <= 0 || n_layers <= 0)
        return 0;

    /* Step 1: Intermediate number of info bits */
    double r = (double)code_rate / 1024.0;
    double n_info = (double)n_re * r * (double)mod_order * (double)n_layers;

    /* Step 2: Quantization per Table 5.1.3.2-1/2 */
    int tbs;
    if (n_info <= 3824.0) {
        /* Step 3: n = floor(log2(n_info)) */
        double n_tmp = n_info;
        int n = 0;
        while (n_tmp >= 2.0) { n_tmp /= 2.0; n++; }
        if (n < 0) n = 0;

        /* n' = max(3, floor(n_info / 2^(n-3)) - 24) */
        int n_prime = (int)(n_info / (double)(1 << (n > 2 ? n - 3 : 0)));
        n_prime -= 24;
        if (n_prime < 1) n_prime = 1;

        /* Closest TBS in Table 5.1.3.2-1 not less than n'*2^(n-3) */
        /* Simplified: approximate quantization */
        int base = n_prime * (1 << (n > 2 ? n - 3 : 0));
        tbs = base;

        /* Round to nearest byte */
        tbs = (tbs + 7) / 8 * 8;
        if (tbs < 24) tbs = 24;
    } else {
        /* Step 4: Quantized TBS for larger blocks */
        /* n = floor(log2(n_info - 24)) - 5 */
        double n_val = (n_info - 24.0);
        int n = 0;
        while (n_val >= 2.0) { n_val /= 2.0; n++; }
        n -= 5;
        if (n < 0) n = 0;

        /* n'_info = 2^n * round((n_info - 24) / 2^n) */
        double n_info_prime = (double)(1 << n) *
            round((n_info - 24.0) / (double)(1 << n));

        if (code_rate <= 256.0) {
            /* Base graph 2 */
            tbs = (int)(8.0 * ceil(n_info_prime / 8.0));
        } else {
            /* Base graph 1 */
            if (n_info_prime > 8424.0) {
                tbs = (int)(8.0 * ceil((n_info_prime - 24.0) / 8.0));
            } else {
                tbs = (int)(8.0 * ceil(n_info_prime / 8.0));
            }
        }
    }

    if (tbs < 24) tbs = 24;
    return tbs;
}

/* ============================================================================
 * L2: Simple Accessor Functions
 * ============================================================================ */

int nr_modulation_order(nr_mod_scheme_t mod)
{
    switch (mod) {
        case NR_MOD_BPSK:    return 1;
        case NR_MOD_QPSK:    return 2;
        case NR_MOD_QAM16:   return 4;
        case NR_MOD_QAM64:   return 6;
        case NR_MOD_QAM256:  return 8;
        case NR_MOD_QAM1024: return 10;
        default:             return 0;
    }
}

double nr_scs_khz(int mu)
{
    if (mu < 0 || mu > NR_MAX_MU) return 0.0;
    return 15.0 * (double)(1 << mu);
}

int nr_symbols_per_slot(int cp_type)
{
    return (cp_type == 0) ? NR_SYMBOLS_PER_SLOT_NCP : NR_SYMBOLS_PER_SLOT_ECP;
}

int nr_slots_per_frame(int mu)
{
    if (mu < 0 || mu > NR_MAX_MU) return 0;
    return NR_SLOTS_PER_FRAME_MU0 * (1 << mu);
}

int nr_slots_per_subframe(int mu)
{
    if (mu < 0 || mu > NR_MAX_MU) return 0;
    return 1 << mu;
}

/* ============================================================================
 * L2: FFT Size and CP Length
 * ============================================================================ */

int nr_fft_size_min(int mu, int num_prb)
{
    if (mu < 0 || mu > NR_MAX_MU || num_prb <= 0) return 0;

    int min_size = num_prb * NR_NUM_SC_PER_RB;
    /* FFT size must be a power of 2 */
    int fft_size = 1;
    while (fft_size < min_size) fft_size <<= 1;

    /* Minimum by numerology: 128 for mu=0 */
    int min_by_mu = 128 << mu;
    if (fft_size < min_by_mu) fft_size = min_by_mu;

    /* Upper limit: 4096 (3GPP TS 38.211 5.3.1) */
    if (fft_size > 4096) fft_size = 4096;

    return fft_size;
}

int nr_cp_length(int symbol_idx, int fft_size, int mu)
{
    if (fft_size <= 0 || mu < 0 || mu > NR_MAX_MU) return 0;

    /* 3GPP TS 38.211 Table 5.3.1-1: CP lengths in kappa*T_c units */
    /* kappa = 64, T_c = 1/(480000*4096) */
    /* Normal CP for symbols l=0 and l=7*2^mu within 0.5ms: 144*kappa*2^{-mu} + 16*kappa */

    int base_cp;   /* shorter CP in kappa*T_c units per 2^mu */
    int extra_cp;  /* extra for longer CP symbols */

    if (mu == 0) {
        base_cp = 144;
        extra_cp = 16;
    } else if (mu == 1) {
        base_cp = 144;
        extra_cp = 16;
    } else if (mu == 2) {
        base_cp = 144;
        extra_cp = 16;
    } else {
        /* mu = 3, 4: scaled */
        base_cp = 144;
        extra_cp = 16;
    }

    /* CP length in samples: scaled by fft_size relative to reference */
    /* Reference: T_c = 1/(480000*4096) seconds, kappa = 64 */

    /* For normal CP: symbol 0 has longer CP in every 0.5ms boundary */
    /* Symbols 0 and 7*2^mu have extended CP within each 0.5ms half-subframe */
    int symbols_per_half = 7 * (1 << mu);
    int cp = base_cp;
    if (symbol_idx % symbols_per_half == 0) {
        cp += extra_cp;
    }

    /* Scale to FFT size: CP samples = ceil(cp * fft_size / (128 * 2^mu)) */
    /* The reference CP length is 144*kappa for fft_size=128*2^mu */
    int ref_fft = 128 * (1 << mu);
    int cp_samples = (cp * fft_size + ref_fft - 1) / ref_fft;

    return cp_samples;
}

/* ============================================================================
 * L2: Frequency Range Determination
 * ============================================================================ */

int nr_is_fr1(double freq_hz)
{
    /* FR1: 410 MHz - 7125 MHz (3GPP TS 38.101-1) */
    return (freq_hz >= 410.0e6 && freq_hz <= 7125.0e6) ? 1 : 0;
}

int nr_is_fr2(double freq_hz)
{
    /* FR2: 24250 MHz - 52600 MHz (3GPP TS 38.101-2, Rel-17) */
    return (freq_hz >= 24250.0e6 && freq_hz <= 52600.0e6) ? 1 : 0;
}

/* ============================================================================
 * L2: MCS Table Lookup (3GPP TS 38.214 Table 5.1.3.1-1)
 * ============================================================================ */

/* MCS Table 1: up to 64QAM, code rate x 1024, spectral efficiency */
static const struct { int mod_order; double rate; double se; } mcs_table1[32] = {
    {2, 120.0, 0.2344},  {2, 157.0, 0.3066},  {2, 193.0, 0.3770},
    {2, 251.0, 0.4902},  {2, 308.0, 0.6016},  {2, 379.0, 0.7402},
    {2, 449.0, 0.8770},  {2, 526.0, 1.0273},  {2, 602.0, 1.1758},
    {2, 679.0, 1.3262},  {4, 340.0, 1.3281},  {4, 378.0, 1.4766},
    {4, 434.0, 1.6953},  {4, 490.0, 1.9141},  {4, 553.0, 2.1602},
    {4, 616.0, 2.4063},  {4, 658.0, 2.5703},  {6, 438.0, 2.5664},
    {6, 466.0, 2.7305},  {6, 517.0, 3.0293},  {6, 567.0, 3.3223},
    {6, 616.0, 3.6094},  {6, 666.0, 3.9023},  {6, 719.0, 4.2129},
    {6, 772.0, 4.5234},  {6, 822.0, 4.8164},  {6, 873.0, 5.1152},
    {6, 910.0, 5.3320},  {6, 948.0, 5.5547},  {2, 0.0, 0.0},
    {4, 0.0, 0.0},        {6, 0.0, 0.0}
};

/* MCS Table 2: up to 256QAM */
static const struct { int mod_order; double rate; double se; } mcs_table2[32] = {
    {2, 120.0, 0.2344},  {2, 193.0, 0.3770},  {2, 308.0, 0.6016},
    {2, 449.0, 0.8770},  {2, 602.0, 1.1758},  {4, 378.0, 1.4766},
    {4, 434.0, 1.6953},  {4, 490.0, 1.9141},  {4, 553.0, 2.1602},
    {4, 616.0, 2.4063},  {4, 658.0, 2.5703},  {6, 466.0, 2.7305},
    {6, 517.0, 3.0293},  {6, 567.0, 3.3223},  {6, 616.0, 3.6094},
    {6, 666.0, 3.9023},  {6, 719.0, 4.2129},  {6, 772.0, 4.5234},
    {6, 822.0, 4.8164},  {6, 873.0, 5.1152},  {8, 682.5, 5.3320},
    {8, 711.0, 5.5547},  {8, 754.0, 5.8906},  {8, 797.0, 6.2266},
    {8, 841.0, 6.5703},  {8, 885.0, 6.9141},  {8, 916.5, 7.1602},
    {8, 948.0, 7.4063},  {2, 0.0, 0.0},       {4, 0.0, 0.0},
    {6, 0.0, 0.0},        {8, 0.0, 0.0}
};

int nr_mcs_lookup(int mcs_index, int table_id, nr_mcs_entry_t *entry)
{
    if (!entry) return -1;
    if (mcs_index < 0 || mcs_index > 31) return -1;
    if (table_id < 1 || table_id > 3) return -1;

    memset(entry, 0, sizeof(*entry));
    entry->mcs_index = mcs_index;

    int mod_order;
    double rate, se;

    if (table_id == 1) {
        mod_order = mcs_table1[mcs_index].mod_order;
        rate = mcs_table1[mcs_index].rate;
        se = mcs_table1[mcs_index].se;
    } else if (table_id == 2) {
        mod_order = mcs_table2[mcs_index].mod_order;
        rate = mcs_table2[mcs_index].rate;
        se = mcs_table2[mcs_index].se;
    } else {
        /* Table 3: 64QAM low SE for URLLC (same as Table 1 but shifted) */
        int idx = (mcs_index < 28) ? mcs_index : 28;
        mod_order = mcs_table1[idx].mod_order;
        rate = mcs_table1[idx].rate;
        se = mcs_table1[idx].se;
    }

    switch (mod_order) {
        case 2:  entry->modulation = NR_MOD_QPSK;   break;
        case 4:  entry->modulation = NR_MOD_QAM16;  break;
        case 6:  entry->modulation = NR_MOD_QAM64;  break;
        case 8:  entry->modulation = NR_MOD_QAM256; break;
        default: entry->modulation = NR_MOD_QPSK;   break;
    }
    entry->code_rate = rate;
    entry->spectral_efficiency = se;

    return 0;
}

int nr_mcs_from_efficiency(double target_se, int table_id)
{
    int best_mcs = -1;
    for (int i = 0; i <= 28; i++) {
        nr_mcs_entry_t entry;
        if (nr_mcs_lookup(i, table_id, &entry) == 0) {
            if (entry.spectral_efficiency >= target_se) {
                best_mcs = i;
                break;
            }
        }
    }
    return best_mcs;
}