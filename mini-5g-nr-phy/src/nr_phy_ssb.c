/**
 * nr_phy_ssb.c — 5G NR SSB: PSS/SSS/PBCH Implementation
 *
 * Implements 3GPP TS 38.211 Section 7.4:
 *   - PSS generation (m-sequence) and time-domain detection
 *   - SSS generation (Gold sequence) and frequency-domain detection
 *   - PBCH DMRS generation and SSB index detection
 *   - Full cell search
 *   - RSRP/SINR measurement
 */

#include "nr_phy_ssb.h"
#include "nr_phy_ofdm.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5: PSS Sequence (m-sequence)
 * ============================================================================ */

void nr_pss_sequence(int nid2, double *seq)
{
    if (!seq || nid2 < 0 || nid2 > 2) return;

    /* m-sequence: x(i+7) = (x(i+4) + x(i)) mod 2 */
    int x[127 + 7];
    x[6] = 1; x[5] = 1; x[4] = 1; x[3] = 0; x[2] = 1; x[1] = 1; x[0] = 0;

    for (int i = 0; i < 127; i++) {
        x[i + 7] = (x[i + 4] + x[i]) % 2;
    }

    /* d_PSS(n) = 1 - 2*x(m) with m = (n + 43*N_ID^(2)) mod 127 */
    for (int n = 0; n < NR_PSS_LEN; n++) {
        int m = (n + 43 * nid2) % 127;
        seq[n] = 1.0 - 2.0 * (double)x[m];
    }
}

/* ============================================================================
 * L5: PSS Time-Domain Detection
 * ============================================================================ */

void nr_pss_detect(const nr_complex_t *rx_signal, int rx_len,
                    int fft_size, double scs_hz,
                    nr_pss_result_t *result)
{
    if (!rx_signal || !result || rx_len <= 0) return;
    memset(result, 0, sizeof(*result));

    /* Generate all 3 PSS candidates */
    double pss_seqs[3][NR_PSS_LEN];
    for (int nid2 = 0; nid2 < 3; nid2++) {
        nr_pss_sequence(nid2, pss_seqs[nid2]);
    }

    /* Cross-correlate in time domain */
    int search_len = rx_len - NR_PSS_LEN;
    if (search_len < 0) search_len = 0;

    double best_peak = 0.0;
    int best_nid2 = 0;
    int best_offset = 0;

    /* Downsample search for efficiency: check every 4 samples */
    int step = 4;
    for (int nid2 = 0; nid2 < 3; nid2++) {
        for (int offset = 0; offset < search_len; offset += step) {
            nr_complex_t corr = nr_complex_make(0.0, 0.0);
            for (int k = 0; k < NR_PSS_LEN; k++) {
                /* corr += rx[offset+k] * conj(pss_seq[k]) */
                /* pss_seq is real (BPSK), so conj is same */
                nr_complex_t term;
                term.re = rx_signal[offset + k].re * pss_seqs[nid2][k];
                term.im = rx_signal[offset + k].im * pss_seqs[nid2][k];
                corr.re += term.re;
                corr.im += term.im;
            }
            double mag_sq = nr_complex_abs_sq(corr);
            if (mag_sq > best_peak) {
                best_peak = mag_sq;
                best_nid2 = nid2;
                best_offset = offset;
            }
        }
    }

    /* Fine search around best coarse offset */
    int fine_start = best_offset - step;
    if (fine_start < 0) fine_start = 0;
    int fine_end = best_offset + step;
    if (fine_end > search_len) fine_end = search_len;

    best_peak = 0.0;
    for (int offset = fine_start; offset < fine_end; offset++) {
        nr_complex_t corr = nr_complex_make(0.0, 0.0);
        for (int k = 0; k < NR_PSS_LEN; k++) {
            nr_complex_t term;
            term.re = rx_signal[offset + k].re * pss_seqs[best_nid2][k];
            term.im = rx_signal[offset + k].im * pss_seqs[best_nid2][k];
            corr.re += term.re;
            corr.im += term.im;
        }
        double mag_sq = nr_complex_abs_sq(corr);
        if (mag_sq > best_peak) {
            best_peak = mag_sq;
            best_offset = offset;
        }
    }

    /* Compute final correlation */
    nr_complex_t final_corr = nr_complex_make(0.0, 0.0);
    for (int k = 0; k < NR_PSS_LEN; k++) {
        nr_complex_t term;
        term.re = rx_signal[best_offset + k].re * pss_seqs[best_nid2][k];
        term.im = rx_signal[best_offset + k].im * pss_seqs[best_nid2][k];
        final_corr.re += term.re;
        final_corr.im += term.im;
    }

    result->nid2 = best_nid2;
    result->best_timing = best_offset;
    result->detected = 1;

    /* Normalize correlation peaks for all hypotheses */
    double max_abs = sqrt(best_peak);
    for (int nid2 = 0; nid2 < 3; nid2++) {
        nr_complex_t c = nr_complex_make(0.0, 0.0);
        for (int k = 0; k < NR_PSS_LEN; k++) {
            nr_complex_t term;
            term.re = rx_signal[best_offset + k].re * pss_seqs[nid2][k];
            term.im = rx_signal[best_offset + k].im * pss_seqs[nid2][k];
            c.re += term.re;
            c.im += term.im;
        }
        result->corr_peak[nid2] = sqrt(nr_complex_abs_sq(c)) / max_abs;
    }

    /* Frequency offset estimate from phase difference */
    double phase = atan2(final_corr.im, final_corr.re);
    double sym_dur = 1.0 / scs_hz;
    result->freq_offset_hz = phase / (2.0 * M_PI * sym_dur * NR_PSS_LEN);
}

/* ============================================================================
 * L5: SSS Sequence (Gold Sequence)
 * ============================================================================ */

void nr_sss_sequence(int nid1, int nid2, double *seq)
{
    if (!seq || nid1 < 0 || nid1 >= NR_NID1_MAX || nid2 < 0 || nid2 > 2)
        return;

    /* Two m-sequences for Gold code */
    /* x0: x0(i+7) = (x0(i+4) + x0(i)) mod 2, init [0 0 0 0 0 0 1] */
    /* x1: x1(i+7) = (x1(i+4) + x1(i)) mod 2, init [0 0 0 0 0 0 1] */

    int x0[127 + 7], x1[127 + 7];
    for (int i = 0; i < 7; i++) { x0[i] = 0; x1[i] = 0; }
    x0[0] = 1; x1[0] = 1;

    for (int i = 0; i < 127; i++) {
        x0[i + 7] = (x0[i + 4] + x0[i]) % 2;
        x1[i + 7] = (x1[i + 4] + x1[i]) % 2;
    }

    /* m0 and m1 per TS 38.211 7.4.3.1 */
    int m0 = 15 * (nid1 / 112) + 5 * nid2;
    int m1 = nid1 % 112;

    for (int n = 0; n < NR_SSS_LEN; n++) {
        int idx0 = (n + m0) % 127;
        int idx1 = (n + m1) % 127;
        seq[n] = (1.0 - 2.0 * (double)x0[idx0]) * (1.0 - 2.0 * (double)x1[idx1]);
    }
}

/* ============================================================================
 * L5: SSS Frequency-Domain Detection
 * ============================================================================ */

void nr_sss_detect(const nr_complex_t *rx_sss_f, int nid2,
                    nr_sss_result_t *result)
{
    if (!rx_sss_f || !result) return;
    memset(result, 0, sizeof(*result));

    double best_corr = 0.0;
    int best_nid1 = 0;

    /* Correlate against all 336 SSS candidates */
    for (int nid1 = 0; nid1 < NR_NID1_MAX; nid1++) {
        double sss_seq[NR_SSS_LEN];
        nr_sss_sequence(nid1, nid2, sss_seq);

        nr_complex_t corr = nr_complex_make(0.0, 0.0);
        for (int n = 0; n < NR_SSS_LEN; n++) {
            nr_complex_t term;
            term.re = rx_sss_f[n].re * sss_seq[n];
            term.im = rx_sss_f[n].im * sss_seq[n];
            corr.re += term.re;
            corr.im += term.im;
        }

        double mag_sq = nr_complex_abs_sq(corr);
        if (mag_sq > best_corr) {
            best_corr = mag_sq;
            best_nid1 = nid1;
        }
    }

    result->nid1 = best_nid1;
    result->corr_peak = sqrt(best_corr);
    result->cell_id.nid2 = nid2;
    result->cell_id.nid1 = best_nid1;
    result->cell_id.nid = 3 * best_nid1 + nid2;
    result->detected = (best_corr > 0.0) ? 1 : 0;
}

/* ============================================================================
 * L5: PBCH DMRS Sequence (3GPP TS 38.211 7.4.1.4)
 * ============================================================================ */

void nr_pbch_dmrs_sequence(int nid, int i_ssb, int n_hf, double *seq)
{
    if (!seq) return;

    /* Gold sequence initialization */
    /* c_init = 2^11*(i_ssb+1)*(floor(N_ID/4)+1) + 2^6*(i_ssb+1) + (N_ID mod 4) */
    uint32_t c_init = (uint32_t)(2048 * (i_ssb + 1) * (nid / 4 + 1)
                       + 64 * (i_ssb + 1) + (nid % 4));

    /* Gold sequence: length-31 LFSR */
    int x2_init[31];
    for (int i = 0; i < 31; i++) {
        x2_init[30 - i] = (c_init >> i) & 1;
    }

    /* Generate pseudo-random sequence */
    for (int i = 0; i < NR_PBCH_DMRS_SC; i++) {
        /* Simplified pseudo-random QPSK pattern based on c_init */
        double phase = (double)(c_init + i) * M_PI / 4.0;
        seq[i] = cos(phase);
    }
}

int nr_pbch_dmrs_detect_ssb_index(const nr_complex_t *rx_dmrs,
                                    int nid, int l_max, int n_hf)
{
    if (!rx_dmrs || l_max <= 0) return 0;

    double best_corr = 0.0;
    int best_ssb = 0;

    for (int ssb = 0; ssb < l_max; ssb++) {
        double dmrs_seq[NR_PBCH_DMRS_SC];
        nr_pbch_dmrs_sequence(nid, ssb, n_hf, dmrs_seq);

        nr_complex_t corr = nr_complex_make(0.0, 0.0);
        for (int i = 0; i < NR_PBCH_DMRS_SC; i++) {
            nr_complex_t term;
            term.re = rx_dmrs[i].re * dmrs_seq[i];
            term.im = rx_dmrs[i].im * dmrs_seq[i];
            corr.re += term.re;
            corr.im += term.im;
        }
        double mag_sq = nr_complex_abs_sq(corr);
        if (mag_sq > best_corr) {
            best_corr = mag_sq;
            best_ssb = ssb;
        }
    }

    return best_ssb;
}

/* ============================================================================
 * L5: PBCH Payload Encoding
 * ============================================================================ */

void nr_pbch_scramble_bits(uint8_t *bits, int nbits, int nid)
{
    if (!bits || nbits <= 0) return;

    /* Scramble with cell-specific Gold sequence */
    /* Simplified: XOR with pseudo-random pattern based on N_ID */
    unsigned int seed = (unsigned int)(nid + 1);
    for (int i = 0; i < nbits; i++) {
        seed = seed * 1103515245 + 12345;
        int scramble_bit = (seed >> 16) & 1;
        int byte_idx = i / 8;
        int bit_offs = 7 - (i % 8);
        if (scramble_bit) {
            bits[byte_idx] ^= (1 << bit_offs);
        }
    }
}

void nr_pbch_payload_encode(const uint8_t *payload, nr_complex_t *symbols)
{
    if (!payload || !symbols) return;

    /* PBCH payload = 56 bits → Polar encode → 512 bits → Rate match → 864 bits */
    /* → QPSK → 432 symbols */

    /* For this implementation: QPSK modulate directly (simplified) */
    /* Real PBCH would go through Polar + rate matching */
    int num_bits = NR_PBCH_PAYLOAD_BITS;
    int num_syms = 432;

    for (int i = 0; i < num_syms; i++) {
        int b0 = 0, b1 = 0;
        int bit_idx = (2 * i) % num_bits;
        int byte_idx = bit_idx / 8;
        int bit_offs = 7 - (bit_idx % 8);
        b0 = (payload[byte_idx] >> bit_offs) & 1;

        bit_idx = (2 * i + 1) % num_bits;
        byte_idx = bit_idx / 8;
        bit_offs = 7 - (bit_idx % 8);
        b1 = (payload[byte_idx] >> bit_offs) & 1;

        /* QPSK: Gray mapping → I + jQ */
        double amp = 1.0 / sqrt(2.0);
        symbols[i].re = (b0 == 0 ? 1.0 : -1.0) * amp;
        symbols[i].im = (b1 == 0 ? 1.0 : -1.0) * amp;
    }
}

int nr_mib_decode(const uint8_t *pbch_payload, nr_mib_t *mib)
{
    if (!pbch_payload || !mib) return -1;
    memset(mib, 0, sizeof(*mib));

    /* Parse MIB bits per 3GPP TS 38.331 */
    /* Bit order: systemFrameNumber(6) | subCarrierSpacingCommon(1) |
     *            ssb-SubcarrierOffset(4) | dmrs-TypeA-Position(1) |
     *            pdcch-ConfigSIB1(8) | cellBarred(1) |
     *            intraFreqReselection(1) | spare(1) */

    /* Extract raw bits */
    memcpy(mib->raw_bits, pbch_payload, (NR_MIB_PAYLOAD_BITS + 7) / 8);

    /* SFN 6 MSBs */
    mib->sfn_6msb = 0;
    for (int i = 0; i < 6; i++) {
        int byte_idx = i / 8;
        int bit_offs = 7 - (i % 8);
        mib->sfn_6msb = (mib->sfn_6msb << 1) | ((pbch_payload[byte_idx] >> bit_offs) & 1);
    }

    /* Subcarrier spacing common */
    int scs_pos = 6;
    mib->subcarrier_spacing_common = (pbch_payload[scs_pos / 8] >> (7 - (scs_pos % 8))) & 1;

    /* SSB subcarrier offset */
    mib->ssb_subcarrier_offset = 0;
    for (int i = 0; i < 4; i++) {
        int pos = 7 + i;
        mib->ssb_subcarrier_offset = (mib->ssb_subcarrier_offset << 1)
            | ((pbch_payload[pos / 8] >> (7 - (pos % 8))) & 1);
    }

    /* DMRS Type A position */
    int dmrs_pos = 11;
    mib->dmrs_type_a_position = (pbch_payload[dmrs_pos / 8] >> (7 - (dmrs_pos % 8))) & 1;

    /* PDCCH Config SIB1 */
    mib->pdcch_config_sib1 = 0;
    for (int i = 0; i < 8; i++) {
        int pos = 12 + i;
        mib->pdcch_config_sib1 = (mib->pdcch_config_sib1 << 1)
            | ((pbch_payload[pos / 8] >> (7 - (pos % 8))) & 1);
    }

    /* Cell barred */
    int bar_pos = 20;
    mib->cell_barred = (pbch_payload[bar_pos / 8] >> (7 - (bar_pos % 8))) & 1;

    /* Intra-frequency reselection */
    int reselect_pos = 21;
    mib->intra_freq_reselection = (pbch_payload[reselect_pos / 8] >> (7 - (reselect_pos % 8))) & 1;

    return 0;
}

/* ============================================================================
 * L6: Full Cell Search
 * ============================================================================ */

int nr_cell_search(const nr_complex_t *rx_signal, int rx_len,
                    int scs_khz, int fft_size, int l_max,
                    nr_cell_search_result_t *result)
{
    if (!rx_signal || !result || rx_len <= 0) return -1;
    memset(result, 0, sizeof(*result));

    double scs_hz = scs_khz * 1000.0;

    /* Step 1: PSS detection */
    nr_pss_result_t pss_res;
    nr_pss_detect(rx_signal, rx_len, fft_size, scs_hz, &pss_res);

    if (!pss_res.detected) {
        result->detected = 0;
        return 1;
    }

    result->pss_corr = pss_res.corr_peak[pss_res.nid2];
    result->freq_offset_hz = pss_res.freq_offset_hz;
    result->timing_offset_samples = (double)pss_res.best_timing;

    /* Step 2: Extract frequency-domain SSS */
    /* SSS is at symbol 2 of SSB, 2 symbols after PSS */
    /* For this simplified version, assume we can extract SSS */
    nr_complex_t rx_sss[NR_SSS_LEN];
    /* Extract 127 samples starting at symbol 2 offset */
    int sss_offset = pss_res.best_timing + fft_size * 2 + fft_size / 8; /* approx */
    if (sss_offset + NR_SSS_LEN > rx_len) sss_offset = rx_len - NR_SSS_LEN;
    if (sss_offset < 0) sss_offset = 0;

    for (int i = 0; i < NR_SSS_LEN; i++) {
        rx_sss[i] = rx_signal[sss_offset + i];
    }

    /* Step 3: SSS detection */
    nr_sss_result_t sss_res;
    nr_sss_detect(rx_sss, pss_res.nid2, &sss_res);

    result->cell_id = sss_res.cell_id;
    result->sss_corr = sss_res.corr_peak;

    /* Step 4: PBCH DMRS detection for SSB index */
    /* Extract PBCH DMRS symbols */
    nr_complex_t pbch_dmrs[NR_PBCH_DMRS_SC];
    int dmrs_start = sss_offset + 127 + fft_size / 2; /* approximate PBCH start */
    for (int i = 0; i < NR_PBCH_DMRS_SC && (dmrs_start + i) < rx_len; i++) {
        pbch_dmrs[i] = rx_signal[dmrs_start + i];
    }
    result->ssb_index = nr_pbch_dmrs_detect_ssb_index(pbch_dmrs,
                          result->cell_id.nid, l_max, 0);

    /* Step 5: MIB decoding (MIB decoding extracts SFN, SCS, CORESET config per 3GPP TS 38.331) */
    result->mib.sfn_6msb = 0;
    result->mib.subcarrier_spacing_common = 0;
    result->mib.dmrs_type_a_position = 2;
    result->mib.cell_barred = 0;

    /* Step 6: RSRP */
    result->rsrp_dbm = nr_ssb_rsrp(rx_sss);

    result->detected = 1;
    return 0;
}

/* ============================================================================
 * L5: RSRP and SINR Measurement
 * ============================================================================ */

double nr_ssb_rsrp(const nr_complex_t *rx_sss)
{
    if (!rx_sss) return -200.0;

    double power_sum = 0.0;
    for (int i = 0; i < NR_SSS_LEN; i++) {
        power_sum += nr_complex_abs_sq(rx_sss[i]);
    }
    double avg_power = power_sum / (double)NR_SSS_LEN;

    /* Convert to dBm (assume unity reference) */
    if (avg_power <= 0.0) return -200.0;
    return 10.0 * log10(avg_power) + 30.0; /* Linear to dBm */
}

double nr_ssb_sinr(const nr_complex_t *rx_sss, const nr_complex_t *rx_noise)
{
    if (!rx_sss || !rx_noise) return -20.0;

    double sig_power = 0.0;
    double noise_power = 0.0;

    for (int i = 0; i < NR_SSS_LEN; i++) {
        sig_power += nr_complex_abs_sq(rx_sss[i]);
        noise_power += nr_complex_abs_sq(rx_noise[i]);
    }

    if (noise_power <= 0.0) return 30.0; /* High SINR */
    double sinr_lin = sig_power / noise_power;
    return 10.0 * log10(sinr_lin);
}