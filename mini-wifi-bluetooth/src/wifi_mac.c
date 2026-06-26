/**
 * @file wifi_mac.c
 * @brief WiFi MAC Layer — CSMA/CA, Frame Construction, Aggregation, QoS (L2,L5,L6)
 *
 * Implements IEEE 802.11 MAC sublayer algorithms:
 *   - CSMA/CA channel access (DCF)
 *   - EDCA QoS scheduling
 *   - Frame construction (Data, RTS, CTS, ACK)
 *   - Frame parsing
 *   - A-MSDU aggregation/disassembly
 *   - Block ACK management
 *   - Throughput estimation (Bianchi model)
 *
 * Reference: IEEE Std 802.11-2020, Clause 10
 * Reference: Gast, M.S., "802.11 Wireless Networks", 2nd ed., O'Reilly 2005.
 */

#include "wifi_mac.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ==========================================================================
 * CSMA/CA — DCF Implementation (L2 Core Concept)
 * ========================================================================== */

int csma_params_init(wifi_csma_params_t *params, wifi_phy_mode_t phy)
{
    if (!params) return -1;

    switch (phy) {
        case WIFI_PHY_80211B:
            params->slot_time_us = 20.0;
            params->sifs_us      = 10.0;
            params->cw_min       = 31;
            break;
        case WIFI_PHY_80211A:
        case WIFI_PHY_80211G:
        case WIFI_PHY_80211N:
        case WIFI_PHY_80211AC:
        case WIFI_PHY_80211AX:
        case WIFI_PHY_80211BE:
        default:
            params->slot_time_us = 9.0;
            params->sifs_us      = 16.0;
            params->cw_min       = 15;
            break;
    }

    params->cw_max          = 1023;
    params->difs_us         = params->sifs_us + 2.0 * params->slot_time_us;
    params->eifs_us         = params->sifs_us + params->difs_us + 8.0 * params->slot_time_us; /* approx */
    params->retry_limit     = 7;
    params->long_retry_limit = 4;

    return 0;
}

double csma_backoff_duration(int cw, int *backoff_slots, double slot_time_us)
{
    if (cw < 0 || !backoff_slots || slot_time_us <= 0.0) return -1.0;

    /* Random backoff: uniform over [0, CW] */
    int slots = rand() % (cw + 1);
    *backoff_slots = slots;
    return (double)slots * slot_time_us;
}

int csma_channel_access(int *can_transmit, double *wait_us,
                        int channel_busy, int *backoff_remaining,
                        const wifi_csma_params_t *params)
{
    if (!can_transmit || !wait_us || !backoff_remaining || !params) return -1;

    if (channel_busy) {
        /* Channel busy: defer, freeze backoff counter */
        *can_transmit = 0;
        *wait_us = 0.0;  /* No wait reported while frozen */
        return 0;
    }

    /* Channel idle: handle DIFS and backoff */
    if (*backoff_remaining <= 0) {
        /* Initial DIFS wait, then start new random backoff */
        int cw = params->cw_min;  /* Use CWmin for initial attempt */
        int slots = 0;
        csma_backoff_duration(cw, &slots, params->slot_time_us);
        *backoff_remaining = slots;
        if (slots == 0) {
            *can_transmit = 1;
            *wait_us = params->difs_us;
        } else {
            *can_transmit = 0;
            *wait_us = params->difs_us + (double)slots * params->slot_time_us;
        }
    } else {
        /* Decrement backoff counter by 1 slot */
        (*backoff_remaining)--;
        if (*backoff_remaining <= 0) {
            *can_transmit = 1;
            *wait_us = params->sifs_us;  /* After DIFS + backoff, can TX immediately */
        } else {
            *can_transmit = 0;
            *wait_us = params->slot_time_us;
        }
    }

    return 0;
}

double nav_compute_duration(int frame_len_bytes, double data_rate_mbps,
                            const wifi_csma_params_t *params)
{
    if (frame_len_bytes <= 0 || data_rate_mbps <= 0.0 || !params) return -1.0;

    /* TX time = preamble + signal field + data */
    double preamble_us = 20.0;  /* Legacy long preamble */
    double signal_us   = 4.0;   /* One OFDM symbol for SIGNAL field */
    double data_us     = (double)(frame_len_bytes * 8) / data_rate_mbps;

    double tx_time = preamble_us + signal_us + data_us;

    /* NAV = TX time for the current frame + SIFS + ACK time + SIFS */
    double ack_time = preamble_us + signal_us + (14.0 * 8.0) / data_rate_mbps;  /* 14-byte ACK */
    return tx_time + params->sifs_us + ack_time;
}

int csma_cw_double(int cw, int cw_min, int cw_max)
{
    if (cw < cw_min || cw_max < cw_min) return cw_min;

    int new_cw = 2 * (cw + 1) - 1;
    if (new_cw > cw_max) new_cw = cw_max;
    if (new_cw < cw_min) new_cw = cw_min;
    return new_cw;
}

/* ==========================================================================
 * EDCA Implementation (L5 Algorithm)
 * ========================================================================== */

int edca_params_init(edca_params_t *edca, edca_access_category_t ac,
                     wifi_phy_mode_t phy)
{
    if (!edca) return -1;

    /* Default EDCA parameter sets (OFDM PHY) */
    switch (ac) {
        case EDCA_AC_BK: /* Background */
            edca->aifsn      = 7;
            edca->cw_min     = 15;
            edca->cw_max     = 1023;
            edca->txop_limit_ms = 0.0;
            break;
        case EDCA_AC_BE: /* Best Effort */
            edca->aifsn      = 3;
            edca->cw_min     = 15;
            edca->cw_max     = 1023;
            edca->txop_limit_ms = 0.0;
            break;
        case EDCA_AC_VI: /* Video */
            edca->aifsn      = 2;
            edca->cw_min     = 7;
            edca->cw_max     = 15;
            edca->txop_limit_ms = 3.008;
            break;
        case EDCA_AC_VO: /* Voice */
            edca->aifsn      = 2;
            edca->cw_min     = 3;
            edca->cw_max     = 7;
            edca->txop_limit_ms = 1.504;
            break;
        default:
            return -1;
    }

    return 0;
}

int edca_backoff(double *backoff_us, edca_access_category_t ac, int cw,
                 const wifi_csma_params_t *params, const edca_params_t *edca)
{
    if (!backoff_us || !params || !edca) return -1;

    /* AIFS[AC] = SIFS + AIFSN[AC] × SlotTime */
    double aifs = params->sifs_us + (double)edca->aifsn * params->slot_time_us;

    /* Random backoff */
    int slots = rand() % (cw + 1);
    *backoff_us = aifs + (double)slots * params->slot_time_us;

    return 0;
}

/* ==========================================================================
 * Frame Construction (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Internal: write 16-bit value in little-endian byte order
 */
static void put_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

/**
 * @brief Internal: get 16-bit little-endian value
 */
static uint16_t get_le16(const uint8_t *buf)
{
    return ((uint16_t)buf[0]) | ((uint16_t)buf[1] << 8);
}

int wifi_frame_build_data(uint8_t *frame, int max_len,
                          const uint8_t da[6], const uint8_t sa[6],
                          const uint8_t bssid[6],
                          const uint8_t *payload, int payload_len,
                          int tid)
{
    if (!frame || !da || !sa || !bssid || max_len < 36) return -1;
    if (payload_len < 0) return -1;
    if (!payload && payload_len > 0) return -1;

    int has_qos = (tid >= 0 && tid <= 7);
    int header_len = 24 + (has_qos ? 2 : 0);
    int total_len = header_len + payload_len + 4;  /* header + payload + FCS */

    if (total_len > max_len || total_len > 2346) return -1;

    /* Frame Control (2 bytes): Protocol=0, Type=Data(10), Subtype=QoS Data(1000) or Data(0000) */
    uint16_t fc = 0x0800;  /* Type=10 (Data) */
    if (has_qos) fc |= 0x0080;  /* Subtype=1000 (QoS Data) */
    fc |= 0x0001;  /* To-DS=1 (assuming STA→AP) */

    put_le16(frame + 0, fc);
    /* Duration/ID (2 bytes): filled later, placeholder */
    put_le16(frame + 2, 0);
    /* Address 1 = DA (RA) */
    memcpy(frame + 4, da, 6);
    /* Address 2 = SA (TA) */
    memcpy(frame + 10, sa, 6);
    /* Address 3 = BSSID */
    memcpy(frame + 16, bssid, 6);
    /* Sequence Control (2 bytes) */
    put_le16(frame + 22, 0x0000);  /* Fragment=0, Sequence=0 placeholder */
    int offset = 24;

    /* QoS Control (2 bytes): TID in bits 0-3 */
    if (has_qos) {
        put_le16(frame + offset, (uint16_t)(tid & 0x0F));
        offset += 2;
    }

    /* Payload */
    if (payload && payload_len > 0) {
        memcpy(frame + offset, payload, (size_t)payload_len);
    }
    offset += payload_len;

    /* FCS (4 bytes) — CRC-32 over MAC header + payload */
    uint32_t fcs = crc32_80211(frame, offset);
    /* FCS is stored in little-endian byte order */
    frame[offset + 0] = (uint8_t)(fcs & 0xFF);
    frame[offset + 1] = (uint8_t)((fcs >> 8) & 0xFF);
    frame[offset + 2] = (uint8_t)((fcs >> 16) & 0xFF);
    frame[offset + 3] = (uint8_t)((fcs >> 24) & 0xFF);
    offset += 4;

    return offset;
}

int wifi_frame_parse(wifi_mac_header_t *header,
                     wifi_frame_type_t *frame_type,
                     int *subtype, int *to_ds, int *from_ds,
                     const uint8_t *frame, int frame_len)
{
    if (!header || !frame || frame_len < 10) return -1;

    uint16_t fc = get_le16(frame);

    /* Protocol version */
    int version = fc & 0x0003;
    if (version != 0) return -1;  /* Only 802.11-2020 supported */

    *frame_type = (wifi_frame_type_t)((fc >> 2) & 0x03);
    *subtype    = (fc >> 4) & 0x0F;
    *to_ds      = (fc >> 8) & 0x01;
    *from_ds    = (fc >> 9) & 0x01;

    /* Parse header fields based on frame type */
    header->frame_control = fc;
    header->duration_id   = get_le16(frame + 2);

    int offset = 4;
    switch (*frame_type) {
        case WIFI_FRAME_DATA:
            /* Address fields depend on To-DS/From-DS */
            if (*to_ds && *from_ds) {
                /* WDS: 4 addresses */
                if (frame_len < 30) return -1;
                memcpy(header->addr1, frame + offset, 6); offset += 6; /* RA */
                memcpy(header->addr2, frame + offset, 6); offset += 6; /* TA */
                memcpy(header->addr3, frame + offset, 6); offset += 6; /* DA */
                memcpy(header->addr4, frame + offset, 6); offset += 6; /* SA */
                header->seq_ctrl = get_le16(frame + offset); offset += 2;
            } else if (*to_ds) {
                /* To AP: 3 addresses */
                if (frame_len < 24) return -1;
                memcpy(header->addr1, frame + offset, 6); offset += 6; /* BSSID (RA) */
                memcpy(header->addr2, frame + offset, 6); offset += 6; /* SA (TA) */
                memcpy(header->addr3, frame + offset, 6); offset += 6; /* DA */
                memset(header->addr4, 0, 6);
                header->seq_ctrl = get_le16(frame + offset); offset += 2;
            } else if (*from_ds) {
                /* From AP */
                if (frame_len < 24) return -1;
                memcpy(header->addr1, frame + offset, 6); offset += 6; /* DA (RA) */
                memcpy(header->addr2, frame + offset, 6); offset += 6; /* BSSID (TA) */
                memcpy(header->addr3, frame + offset, 6); offset += 6; /* SA */
                memset(header->addr4, 0, 6);
                header->seq_ctrl = get_le16(frame + offset); offset += 2;
            } else {
                /* Ad-hoc: 3 addresses */
                if (frame_len < 24) return -1;
                memcpy(header->addr1, frame + offset, 6); offset += 6; /* DA (RA) */
                memcpy(header->addr2, frame + offset, 6); offset += 6; /* SA (TA) */
                memcpy(header->addr3, frame + offset, 6); offset += 6; /* BSSID */
                memset(header->addr4, 0, 6);
                header->seq_ctrl = get_le16(frame + offset); offset += 2;
            }

            /* QoS Control — check if QoS subtype */
            if ((*subtype & 0x08) && frame_len >= offset + 2) {
                header->qos_ctrl = get_le16(frame + offset); offset += 2;
            } else {
                header->qos_ctrl = 0;
            }

            /* HT Control */
            if ((fc & 0x8000) && frame_len >= offset + 2) {
                header->ht_ctrl = get_le16(frame + offset); offset += 2;
            } else {
                header->ht_ctrl = 0;
            }
            break;

        case WIFI_FRAME_MGMT:
            if (frame_len < 24) return -1;
            memcpy(header->addr1, frame + offset, 6); offset += 6; /* DA */
            memcpy(header->addr2, frame + offset, 6); offset += 6; /* SA */
            memcpy(header->addr3, frame + offset, 6); offset += 6; /* BSSID */
            memset(header->addr4, 0, 6);
            header->seq_ctrl = get_le16(frame + offset); offset += 2;
            header->qos_ctrl = 0;
            header->ht_ctrl  = 0;
            break;

        case WIFI_FRAME_CTRL:
            /* Control frames have minimal header: 2+2+6+(6 for RTS) */
            memset(header->addr2, 0, 6);
            memset(header->addr4, 0, 6);
            header->qos_ctrl = 0;
            header->ht_ctrl  = 0;
            header->seq_ctrl = 0;
            if (*subtype == 0x0B || *subtype == 0x0C) {
                /* RTS/CTS: RA only */
                if (frame_len >= 10) {
                    memcpy(header->addr1, frame + offset, 6);
                    offset += 6;
                }
                memset(header->addr3, 0, 6);
            } else if (*subtype == 0x0D) {
                /* ACK: RA only */
                if (frame_len >= 10) {
                    memcpy(header->addr1, frame + offset, 6);
                    offset += 6;
                }
                memset(header->addr3, 0, 6);
            } else {
                memset(header->addr1, 0, 6);
                memset(header->addr3, 0, 6);
            }
            break;

        default:
            break;
    }

    return 0;
}

int wifi_frame_build_rts(uint8_t *rts_frame, int max_len,
                         const uint8_t ra[6], const uint8_t ta[6],
                         uint16_t nav_duration_us)
{
    if (!rts_frame || !ra || !ta || max_len < 20) return -1;

    /* Frame Control: Type=01 (Control), Subtype=1011 (RTS) */
    put_le16(rts_frame + 0, 0x01B4);

    /* Duration */
    put_le16(rts_frame + 2, nav_duration_us);

    /* RA */
    memcpy(rts_frame + 4, ra, 6);

    /* TA */
    memcpy(rts_frame + 10, ta, 6);

    /* FCS */
    uint32_t fcs = crc32_80211(rts_frame, 16);
    rts_frame[16] = (uint8_t)(fcs & 0xFF);
    rts_frame[17] = (uint8_t)((fcs >> 8) & 0xFF);
    rts_frame[18] = (uint8_t)((fcs >> 16) & 0xFF);
    rts_frame[19] = (uint8_t)((fcs >> 24) & 0xFF);

    return 20;
}

int wifi_frame_build_cts(uint8_t *cts_frame, int max_len,
                         const uint8_t ra[6], uint16_t nav_duration_us)
{
    if (!cts_frame || !ra || max_len < 14) return -1;

    /* Frame Control: Type=01 (Control), Subtype=1100 (CTS) */
    put_le16(cts_frame + 0, 0x01C4);

    /* Duration */
    put_le16(cts_frame + 2, nav_duration_us);

    /* RA */
    memcpy(cts_frame + 4, ra, 6);

    /* FCS */
    uint32_t fcs = crc32_80211(cts_frame, 10);
    cts_frame[10] = (uint8_t)(fcs & 0xFF);
    cts_frame[11] = (uint8_t)((fcs >> 8) & 0xFF);
    cts_frame[12] = (uint8_t)((fcs >> 16) & 0xFF);
    cts_frame[13] = (uint8_t)((fcs >> 24) & 0xFF);

    return 14;
}

int wifi_frame_build_ack(uint8_t *ack_frame, int max_len, const uint8_t ra[6])
{
    if (!ack_frame || !ra || max_len < 14) return -1;

    /* Frame Control: Type=01 (Control), Subtype=1101 (ACK) */
    put_le16(ack_frame + 0, 0x01D4);

    /* Duration: 0 for ACK (no following transmission) */
    put_le16(ack_frame + 2, 0x0000);

    /* RA */
    memcpy(ack_frame + 4, ra, 6);

    /* FCS */
    uint32_t fcs = crc32_80211(ack_frame, 10);
    ack_frame[10] = (uint8_t)(fcs & 0xFF);
    ack_frame[11] = (uint8_t)((fcs >> 8) & 0xFF);
    ack_frame[12] = (uint8_t)((fcs >> 16) & 0xFF);
    ack_frame[13] = (uint8_t)((fcs >> 24) & 0xFF);

    return 14;
}

/* ==========================================================================
 * Block ACK (L5 Algorithm)
 * ========================================================================== */

int block_ack_init(wifi_block_ack_t *ba, int start_seq, int n_frames)
{
    if (!ba || n_frames <= 0 || n_frames > 64) return -1;

    ba->bitmap       = 0;
    ba->starting_seq = start_seq;
    ba->n_frames     = n_frames;
    return 0;
}

int block_ack_record(wifi_block_ack_t *ba, int seq_num)
{
    if (!ba) return -1;

    int offset = seq_num - ba->starting_seq;
    if (offset < 0 || offset >= ba->n_frames) return -1;

    ba->bitmap |= ((uint64_t)1 << (uint64_t)offset);
    return 0;
}

int block_ack_is_received(const wifi_block_ack_t *ba, int seq_num)
{
    if (!ba) return -1;

    int offset = seq_num - ba->starting_seq;
    if (offset < 0 || offset >= ba->n_frames) return -1;

    return ((ba->bitmap >> (uint64_t)offset) & 1) ? 1 : 0;
}

int block_ack_get_missing(int *retx_seqs, int max_retx,
                          const wifi_block_ack_t *ba)
{
    if (!retx_seqs || !ba) return -1;

    int count = 0;
    for (int i = 0; i < ba->n_frames && count < max_retx; i++) {
        if (!(ba->bitmap & ((uint64_t)1 << (uint64_t)i))) {
            retx_seqs[count++] = ba->starting_seq + i;
        }
    }
    return count;
}

/* ==========================================================================
 * A-MSDU Aggregation (L5 Algorithm)
 * ========================================================================== */

int amsdu_aggregate(uint8_t *amsdu, int max_len,
                    const uint8_t da[6], const uint8_t sa[6],
                    const uint8_t **msdus, const int *msdu_lens, int n_msdus)
{
    if (!amsdu || !da || !sa || !msdus || !msdu_lens || n_msdus <= 0) return -1;

    int offset = 0;
    int subframe_hdr = 14;  /* DA(6) + SA(6) + Length(2) */

    for (int i = 0; i < n_msdus; i++) {
        if (!msdus[i] || msdu_lens[i] <= 0) continue;

        int padded_len = (msdu_lens[i] + 3) & ~3;  /* pad to 4 bytes */
        int total_subframe = subframe_hdr + msdu_lens[i];

        if (offset + total_subframe + 4 > max_len) break;  /* +4 for total FCS later */

        /* Subframe header */
        memcpy(amsdu + offset, da, 6);          offset += 6;
        memcpy(amsdu + offset, sa, 6);          offset += 6;
        put_le16(amsdu + offset, (uint16_t)msdu_lens[i]); offset += 2;

        /* MSDU payload */
        memcpy(amsdu + offset, msdus[i], (size_t)msdu_lens[i]);
        offset += msdu_lens[i];

        /* Padding to 4-byte boundary */
        int pad = padded_len - msdu_lens[i];
        if (pad > 0 && pad < 4) {
            memset(amsdu + offset, 0, (size_t)pad);
            offset += pad;
        }
    }

    return offset;
}

int amsdu_disassemble(const uint8_t **msdus, int *msdu_lens, int max_msdus,
                      const uint8_t *amsdu, int amsdu_len)
{
    if (!msdus || !msdu_lens || !amsdu || max_msdus <= 0 || amsdu_len <= 0) return -1;

    int count = 0;
    int offset = 0;

    while (offset + 14 <= amsdu_len && count < max_msdus) {
        /* Subframe header: DA(6) + SA(6) + Length(2) */
        uint16_t len = get_le16(amsdu + offset + 12);

        if (len == 0) break;  /* End marker */
        if (offset + 14 + (int)len > amsdu_len) break;  /* Malformed */

        msdus[count]     = amsdu + offset + 14;
        msdu_lens[count] = (int)len;
        count++;

        /* Advance to next subframe (4-byte aligned) */
        int padded_len = ((int)len + 3) & ~3;
        offset += 14 + padded_len;
    }

    return count;
}

/* ==========================================================================
 * Throughput Estimation (L2 Core Concept)
 * ========================================================================== */

double wifi_throughput_estimate(int payload_bytes, double phy_rate_mbps,
                                double per, const wifi_csma_params_t *params,
                                wifi_phy_mode_t phy)
{
    if (payload_bytes <= 0 || phy_rate_mbps <= 0.0 || !params) return 0.0;

    /* PHY preamble + header duration */
    double preamble_us;
    if (phy == WIFI_PHY_80211B) {
        preamble_us = 192.0;  /* Long preamble for DSSS */
    } else {
        preamble_us = 20.0;   /* OFDM legacy preamble */
    }

    /* Data transmission time */
    double data_us = (double)(payload_bytes * 8) / phy_rate_mbps;

    /* ACK time: 14-byte ACK + preamble */
    double ack_us = preamble_us + 4.0 + (14.0 * 8.0) / phy_rate_mbps; /* +4 for SIGNAL symbol */

    /* Average backoff: CWmin/2 * slot_time */
    double avg_backoff = (double)(params->cw_min) / 2.0 * params->slot_time_us;

    /* Total time for one successful transmission */
    double t_success = preamble_us + 4.0 + data_us + params->sifs_us + ack_us + params->difs_us + avg_backoff;

    /* Probability of successful transmission */
    double p_success = 1.0 - per;

    /* Time wasted on failed transmission (no ACK received) */
    double t_fail = preamble_us + 4.0 + data_us + params->sifs_us + avg_backoff;

    /* Average time per transmission attempt */
    int max_retries = params->retry_limit;
    double expected_time = t_success;
    double p_accum = p_success;
    for (int r = 1; r <= max_retries; r++) {
        expected_time += (1.0 - p_accum) * t_fail;
        p_accum += (1.0 - p_accum) * p_success;
    }

    /* Effective throughput */
    double payload_bits = (double)(payload_bytes * 8) * p_success;
    return payload_bits / expected_time;  /* Mbps */
}

double bianchi_throughput(int n_stations, int cw_min,
                          int payload_bytes, double phy_rate_mbps,
                          const wifi_csma_params_t *params)
{
    if (n_stations <= 0 || cw_min <= 0 || !params) return 0.0;

    /* First-order approximation of transmission probability τ */
    double tau = 2.0 / (double)(cw_min + 1);

    /* Probability at least one station transmits */
    double P_tr = 1.0 - pow(1.0 - tau, (double)n_stations);

    /* Probability of successful transmission given at least one transmits */
    double P_s = (double)n_stations * tau * pow(1.0 - tau, (double)(n_stations - 1)) / P_tr;

    /* Slot time */
    double sigma = params->slot_time_us;

    /* Successful transmission duration */
    double T_s = 20.0 + 4.0 + (double)(payload_bytes * 8) / phy_rate_mbps
               + params->sifs_us + 20.0 + 4.0 + 14.0 * 8.0 / phy_rate_mbps + params->difs_us;

    /* Collision duration (longest colliding frame) */
    double T_c = 20.0 + 4.0 + (double)(payload_bytes * 8) / phy_rate_mbps + params->sifs_us;

    /* Expected payload transmitted in a slot */
    double E_P = (double)(payload_bytes * 8) / phy_rate_mbps;

    /* Bianchi saturated throughput formula */
    double numerator   = P_s * P_tr * E_P;
    double denominator = (1.0 - P_tr) * sigma + P_tr * P_s * T_s + P_tr * (1.0 - P_s) * T_c;

    if (denominator <= 0.0) return 0.0;

    return numerator / denominator;  /* Fraction of channel capacity (0-1) */
}
