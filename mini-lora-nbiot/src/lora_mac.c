/**
 * @file lora_mac.c
 * @brief LoRaWAN MAC Layer -- Frame construction, MIC, duty cycle, ADR, join procedure
 *
 * Knowledge: L2 Class A/B/C, L3 AES-128 CMAC, L4 duty cycle limits,
 *            L5 frame construction/parsing, ADR algorithm, L6 join procedure
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lora_phy.h"
#include "lora_mac.h"

/* ======================================================================
   L5: Frame Construction
   ====================================================================== */

/*
 * Build a LoRaWAN Join Request frame.
 *
 * Join Request format (LoRaWAN 1.0.4):
 *   [MHDR=0x00 1B] [AppEUI 8B] [DevEUI 8B] [DevNonce 2B] [MIC 4B]
 *   Total: 23 bytes
 *
 * The MIC is computed using the AppKey (AES-128 CMAC).
 * This is sent unencrypted because the AppKey is pre-shared.
 *
 * @return Frame length in bytes, or -1 on error
 */
int lora_build_join_request(const uint8_t *join_eui, const uint8_t *dev_eui,
                             uint16_t dev_nonce, uint8_t *buffer, size_t buf_len)
{
    if (!join_eui || !dev_eui || !buffer || buf_len < 23) return -1;

    size_t idx = 0;

    /* MHDR: Join Request = 0x00 */
    buffer[idx++] = 0x00;

    /* AppEUI (8 bytes, little-endian per LoRaWAN) */
    for (int i = 0; i < 8; i++)
        buffer[idx++] = join_eui[7 - i];

    /* DevEUI (8 bytes, little-endian) */
    for (int i = 0; i < 8; i++)
        buffer[idx++] = dev_eui[7 - i];

    /* DevNonce (2 bytes, little-endian) */
    buffer[idx++] = dev_nonce & 0xFF;
    buffer[idx++] = (dev_nonce >> 8) & 0xFF;

    /* MIC field (4 bytes) -- filled separately by caller */
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;

    return (int)idx;
}

/*
 * Build a LoRaWAN data frame (uplink).
 *
 * Data frame format:
 *   [MHDR 1B] [FHDR: DevAddr 4B + FCtrl 1B + FCnt 2B + FOpts 0-15B]
 *   [FPort 1B] [Payload N B] [MIC 4B]
 *
 * MHDR for confirmed data up: 0x40, unconfirmed: 0x80
 *
 * @return Frame length in bytes, or -1 on error
 */
int lora_build_data_frame(const lora_session_t *session,
                           uint8_t f_port, const uint8_t *payload,
                           uint16_t payload_len, int confirmed,
                           uint8_t *buffer, size_t buf_len)
{
    if (!session || !buffer) return -1;
    if (payload_len > 0 && !payload) return -1;

    size_t min_len = 1 + 4 + 1 + 2 + 1 + payload_len + 4;
    if (buf_len < min_len) return -1;

    size_t idx = 0;

    /* MHDR */
    uint8_t mtype = confirmed ? LORA_MTYPE_CONFIRMED_DATA_UP
                              : LORA_MTYPE_UNCONFIRMED_DATA_UP;
    buffer[idx++] = (uint8_t)(mtype << 5);

    /* FHDR: DevAddr (4 bytes, little-endian) */
    buffer[idx++] = session->dev_addr & 0xFF;
    buffer[idx++] = (session->dev_addr >> 8)  & 0xFF;
    buffer[idx++] = (session->dev_addr >> 16) & 0xFF;
    buffer[idx++] = (session->dev_addr >> 24) & 0xFF;

    /* FCtrl: ADR enabled + no ACK + no FPending + FOptsLen=0 */
    uint8_t fctrl = session->adr_enabled ? 0x80 : 0x00;
    buffer[idx++] = fctrl;

    /* FCnt (2 bytes, little-endian) */
    buffer[idx++] = session->f_cnt_up & 0xFF;
    buffer[idx++] = (session->f_cnt_up >> 8) & 0xFF;

    /* FPort */
    buffer[idx++] = f_port;

    /* Payload */
    if (payload && payload_len > 0) {
        memcpy(&buffer[idx], payload, payload_len);
        idx += payload_len;
    }

    /* MIC field -- pre-filled with zeros, caller computes actual MIC */
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;
    buffer[idx++] = 0x00;

    return (int)idx;
}

/*
 * Parse a LoRaWAN frame from raw bytes.
 *
 * @return 0 on success, -1 on parse error
 */
int lora_parse_frame(lora_mac_frame_t *frame, const uint8_t *data, size_t len)
{
    if (!frame || !data || len < 12) return -1;

    memset(frame, 0, sizeof(*frame));

    /* MHDR */
    frame->mhdr = data[0];
    frame->mtype = (lora_mtype_t)((data[0] >> 5) & 0x07);

    /* FHDR: DevAddr */
    frame->fhdr.dev_addr = (uint32_t)data[1]
                         | ((uint32_t)data[2] << 8)
                         | ((uint32_t)data[3] << 16)
                         | ((uint32_t)data[4] << 24);

    /* FCtrl */
    frame->fhdr.f_ctrl = data[5];

    /* FCnt */
    frame->fhdr.f_cnt = (uint16_t)data[6] | ((uint16_t)data[7] << 8);

    /* FOpts (if present) */
    uint8_t fopts_len = frame->fhdr.f_ctrl & 0x0F;
    if (fopts_len > 15) fopts_len = 15;
    if (len >= (size_t)(8 + fopts_len)) {
        memcpy(frame->fhdr.f_opts, &data[8], fopts_len);
        frame->fhdr.f_opts_len = fopts_len;
    }

    return 0;
}

/* ======================================================================
   L3: AES-128 CMAC for MIC
   ====================================================================== */

/*
 * Simplified AES-128 CMAC implementation for MIC calculation.
 *
 * In a full implementation, this would use a proper AES library.
 * For this educational module, we implement a lightweight version
 * that demonstrates the MIC structure without the full cipher.
 *
 * The CMAC algorithm (RFC 4493):
 *   1. Generate subkeys K1, K2 from the AES key
 *   2. Process message in 16-byte blocks using AES-CBC
 *   3. Apply subkey to final block based on padding
 *   4. Output = first 4 bytes of final ciphertext
 *
 * Note: This simplified version demonstrates the CMAC structure.
 * Production systems MUST use a proper AES implementation.
 *
 * @param key     16-byte AES key
 * @param msg     Message to authenticate
 * @param msg_len Message length
 * @param mic     Output: 4-byte MIC
 */
void lora_aes128_cmac(const uint8_t *key, const uint8_t *msg,
                       size_t msg_len, uint8_t *mic)
{
    if (!key || !msg || !mic) return;

    /*
     * For demonstration, compute a simplified MIC using XOR-based
     * CBC-MAC structure with the key as initialization vector.
     *
     * Block cipher (educational simplified AES stand-in):
     *   Each 16-byte block is XORed with accumulated state and
     *   a key-dependent transformation is applied.
     *
     * Warning: This is NOT cryptographically secure!
     * It demonstrates the CMAC algorithm structure for
     * educational purposes only.
     */

    uint8_t state[16];
    memcpy(state, key, 16);  /* Use key as initial state */

    /* Process message in 16-byte blocks */
    size_t num_blocks = (msg_len + 15) / 16;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t offset = b * 16;
        size_t remaining = msg_len - offset;
        size_t block_len = (remaining < 16) ? remaining : 16;

        /* XOR message block into state */
        for (size_t i = 0; i < block_len; i++)
            state[i] ^= msg[offset + i];

        /* Padding for last block (simplified) */
        if (block_len < 16)
            state[block_len] ^= 0x80;  /* ISO/IEC 9797-1 padding */

        /* Key-dependent non-linear transformation (simplified AES stand-in)
         * Using: state = state XOR rotate_right(state, 4) XOR key */
        uint8_t rotated[16];
        for (int i = 0; i < 16; i++)
            rotated[(i + 4) % 16] = state[i];

        for (int i = 0; i < 16; i++)
            state[i] = state[i] ^ rotated[i] ^ key[i] ^ (uint8_t)(b * 37);
    }

    /* Output first 4 bytes as MIC */
    memcpy(mic, state, 4);
}

/*
 * Calculate LoRaWAN frame MIC.
 *
 * B0 block structure (16 bytes):
 *   [0x49] [4B reserved=0] [Dir] [DevAddr 4B] [FCnt 4B] [0x00] [msg_len 1B]
 *
 * MIC = AES-CMAC(NwkSKey, B0 | msg)[0:4]
 *
 * Direction: 0 = uplink, 1 = downlink
 */
void lora_calculate_mic(const lora_session_t *session,
                         const lora_mac_frame_t *frame, int direction)
{
    if (!session || !frame) return;

    /* Build B0 block */
    uint8_t b0[16];
    memset(b0, 0, 16);
    b0[0] = 0x49;
    b0[5] = (uint8_t)(direction & 1);
    b0[6] = session->dev_addr & 0xFF;
    b0[7] = (session->dev_addr >> 8) & 0xFF;
    b0[8] = (session->dev_addr >> 16) & 0xFF;
    b0[9] = (session->dev_addr >> 24) & 0xFF;

    uint32_t fcnt = (direction == 0) ? session->f_cnt_up : session->f_cnt_down;
    b0[10] = fcnt & 0xFF;
    b0[11] = (fcnt >> 8) & 0xFF;
    b0[12] = (fcnt >> 16) & 0xFF;
    b0[13] = (fcnt >> 24) & 0xFF;
    /* b0[15] = msg_len -- computed below */

    /* Build message: MHDR | FHDR | FPort | FRMPayload */
    uint8_t msg[256];
    size_t msg_len = 0;

    msg[msg_len++] = frame->mhdr;
    msg[msg_len++] = frame->fhdr.dev_addr & 0xFF;
    msg[msg_len++] = (frame->fhdr.dev_addr >> 8) & 0xFF;
    msg[msg_len++] = (frame->fhdr.dev_addr >> 16) & 0xFF;
    msg[msg_len++] = (frame->fhdr.dev_addr >> 24) & 0xFF;
    msg[msg_len++] = frame->fhdr.f_ctrl;
    msg[msg_len++] = frame->fhdr.f_cnt & 0xFF;
    msg[msg_len++] = (frame->fhdr.f_cnt >> 8) & 0xFF;
    /* FOpts */
    if (frame->fhdr.f_opts_len > 0) {
        memcpy(&msg[msg_len], frame->fhdr.f_opts, frame->fhdr.f_opts_len);
        msg_len += frame->fhdr.f_opts_len;
    }
    msg[msg_len++] = frame->f_port;
    if (frame->frm_payload && frame->frm_payload_len > 0) {
        memcpy(&msg[msg_len], frame->frm_payload, frame->frm_payload_len);
        msg_len += frame->frm_payload_len;
    }

    b0[15] = (uint8_t)(msg_len & 0xFF);

    /* Concatenate B0 + msg for CMAC input */
    uint8_t cmac_input[272];
    memcpy(cmac_input, b0, 16);
    memcpy(cmac_input + 16, msg, msg_len);

    lora_aes128_cmac(session->nwk_s_key, cmac_input, 16 + msg_len,
                      (uint8_t *)frame->mic);
}

/*
 * Verify LoRaWAN frame MIC.
 *
 * Computes expected MIC and compares with received MIC.
 * Returns 0 on match, -1 on mismatch.
 */
int lora_verify_mic(const lora_session_t *session,
                     const lora_mac_frame_t *frame, int direction)
{
    if (!session || !frame) return -1;

    uint8_t expected_mic[LORA_MIC_SIZE];
    memcpy(expected_mic, frame->mic, LORA_MIC_SIZE);

    /* Recalculate MIC */
    lora_calculate_mic(session, frame, direction);

    /* Compare */
    if (memcmp(expected_mic, frame->mic, LORA_MIC_SIZE) == 0)
        return 0;

    return -1;
}

/* ======================================================================
   L3: Session Key Derivation
   ====================================================================== */

/*
 * Derive LoRaWAN session keys from AppKey (OTAA).
 *
 * NwkSKey = aes128_encrypt(AppKey, 0x01 | AppNonce | NetID | DevNonce | pad16)
 * AppSKey = aes128_encrypt(AppKey, 0x02 | AppNonce | NetID | DevNonce | pad16)
 *
 * For this educational module, we use a simplified key derivation.
 */
void lora_derive_session_keys(const uint8_t *app_key,
                               const uint8_t *app_nonce,
                               const uint8_t *net_id,
                               uint16_t dev_nonce,
                               uint8_t *nwk_s_key,
                               uint8_t *app_s_key)
{
    if (!app_key || !app_nonce || !net_id || !nwk_s_key || !app_s_key)
        return;

    /*
     * Build derivation input:
     *   [prefix 1B] [AppNonce 3B] [NetID 3B] [DevNonce 2B] [pad 7B]
     */
    uint8_t input[16];
    memset(input, 0, 16);

    /* NwkSKey: prefix = 0x01 */
    input[0] = 0x01;
    memcpy(&input[1], app_nonce, 3);
    memcpy(&input[4], net_id, 3);
    input[7] = dev_nonce & 0xFF;
    input[8] = (dev_nonce >> 8) & 0xFF;

    /* Apply key using simplified CMAC-like derivation */
    lora_aes128_cmac(app_key, input, 16, nwk_s_key);

    /* AppSKey: prefix = 0x02 */
    input[0] = 0x02;
    lora_aes128_cmac(app_key, input, 16, app_s_key);
}

/* ======================================================================
   L4: Duty Cycle Tracking
   ====================================================================== */

void lora_duty_cycle_init(lora_duty_cycle_tracker_t *tracker,
                           double duty_cycle, double window_sec)
{
    if (!tracker) return;
    memset(tracker, 0, sizeof(*tracker));
    tracker->duty_cycle_limit = duty_cycle;
    tracker->observation_period = window_sec;
    tracker->toa_accumulated = 0.0;
    tracker->window_start = 0.0;
    tracker->last_tx_time = 0.0;
}

int lora_duty_cycle_check(lora_duty_cycle_tracker_t *tracker,
                           double toa_sec, double now_sec)
{
    if (!tracker) return 0;

    /* Reset window if period exceeded */
    if (now_sec - tracker->window_start > tracker->observation_period) {
        tracker->window_start = now_sec;
        tracker->toa_accumulated = 0.0;
    }

    /* Check if adding this TX would exceed duty cycle */
    double max_toa = tracker->duty_cycle_limit * tracker->observation_period;
    double new_toa = tracker->toa_accumulated + toa_sec;

    return (new_toa <= max_toa) ? 1 : 0;
}

void lora_duty_cycle_record(lora_duty_cycle_tracker_t *tracker,
                             double toa_sec, double now_sec)
{
    if (!tracker) return;

    /* Reset window if period exceeded */
    if (now_sec - tracker->window_start > tracker->observation_period) {
        tracker->window_start = now_sec;
        tracker->toa_accumulated = 0.0;
    }

    tracker->toa_accumulated += toa_sec;
    tracker->last_tx_time = now_sec;
}

double lora_duty_cycle_wait_sec(lora_duty_cycle_tracker_t *tracker,
                                 double toa_sec, double now_sec)
{
    if (!tracker) return 0.0;

    if (lora_duty_cycle_check(tracker, toa_sec, now_sec))
        return 0.0;

    /* Calculate time until enough budget accumulates */
    double max_toa = tracker->duty_cycle_limit * tracker->observation_period;
    double needed = tracker->toa_accumulated + toa_sec - max_toa;

    if (needed <= 0.0) return 0.0;

    /* Time needed = needed / duty_cycle_limit */
    return needed / tracker->duty_cycle_limit;
}

/* ======================================================================
   L5: ADR Algorithm
   ====================================================================== */

/*
 * Adaptive Data Rate (ADR) algorithm.
 *
 * Strategy:
 *   1. Measure SNR margin = SNR_measured - SNR_required(SF_current)
 *   2. If margin > 10 dB: try higher data rate (lower SF by 1)
 *   3. If margin < 5 dB: try 1 step lower data rate
 *   4. If margin < 0 dB: go to highest SF for maximum robustness
 *   5. Optimize TX power: reduce power if margin is excessive
 *
 * @return New SF setting
 */
uint8_t lora_adr_update(lora_adr_state_t *adr, double snr_measured,
                         uint32_t frame_count __attribute__((unused)))
{
    if (!adr || !adr->enabled) return adr ? adr->current_sf : 7;

    /* SNR required for current SF */
    double snr_req;
    switch (adr->current_sf) {
        case 7:  snr_req = -7.5;  break;
        case 8:  snr_req = -10.0; break;
        case 9:  snr_req = -12.5; break;
        case 10: snr_req = -15.0; break;
        case 11: snr_req = -17.5; break;
        case 12: snr_req = -20.0; break;
        default: snr_req = -7.5;
    }

    adr->snr_margin_db = snr_measured - snr_req;
    adr->adr_ack_cnt++;

    /* Update requires enough frames for stable measurement */
    if (adr->adr_ack_cnt < adr->adr_ack_delay)
        return adr->current_sf;

    /* ADR decision */
    if (adr->snr_margin_db > 10.0 && adr->current_sf > 7) {
        /* Good link: increase data rate */
        adr->current_sf--;
        adr->adr_ack_cnt = 0;
    } else if (adr->snr_margin_db < 5.0 && adr->current_sf < 12) {
        /* Marginal link: decrease data rate */
        adr->current_sf++;
        adr->adr_ack_cnt = 0;
    } else if (adr->snr_margin_db < 0.0) {
        /* Bad link: go to highest SF immediately */
        adr->current_sf = 12;
        adr->adr_ack_cnt = 0;
    }

    return adr->current_sf;
}

/* ======================================================================
   L6: Join Procedure
   ====================================================================== */

void lora_join_init(lora_join_procedure_t *proc,
                     const uint8_t *join_eui, const uint8_t *dev_eui,
                     const uint8_t *app_key)
{
    if (!proc) return;
    memset(proc, 0, sizeof(*proc));
    proc->state = JOIN_STATE_IDLE;
    memcpy(proc->join_eui, join_eui, LORA_APPEUI_SIZE);
    memcpy(proc->dev_eui, dev_eui, LORA_DEVEUI_SIZE);
    memcpy(proc->app_key, app_key, LORA_APPKEY_SIZE);
    proc->max_retries = 8;
    proc->retry_count = 0;
}

int lora_join_handle_accept(lora_join_procedure_t *proc,
                             const uint8_t *join_accept, size_t len)
{
    if (!proc || !join_accept || len < 17) return -1;

    /* Parse Join Accept (simplified) */
    proc->state = JOIN_STATE_ACCEPTED;

    /* Extract session parameters */
    proc->session.dev_addr = (uint32_t)join_accept[4]
                           | ((uint32_t)join_accept[5] << 8)
                           | ((uint32_t)join_accept[6] << 16)
                           | ((uint32_t)join_accept[7] << 24);

    lora_derive_session_keys(proc->app_key, &join_accept[1],
                              &join_accept[1], proc->dev_nonce,
                              proc->session.nwk_s_key,
                              proc->session.app_s_key);

    proc->session.f_cnt_up = 0;
    proc->session.f_cnt_down = 0;
    proc->session.device_class = LORA_CLASS_A;
    proc->session.rx1_dr_offset = 0;
    proc->session.rx2_dr = 0;
    proc->session.rx2_freq = 869525000.0;
    proc->session.rx1_delay = 1;
    proc->session.adr_enabled = 1;

    proc->state = JOIN_STATE_ACTIVE;
    return 0;
}

/*
 * Calculate Class A receive window start times.
 *
 * RX1: opens RECEIVE_DELAY1 seconds after uplink end
 * RX2: opens RECEIVE_DELAY1 + 1 second after uplink end
 *
 * Default RECEIVE_DELAY1 = 1 second (configurable).
 */
void lora_class_a_rx_windows(double uplink_end_sec, uint8_t rx_delay,
                              double *rx1_start, double *rx2_start)
{
    if (!rx1_start || !rx2_start) return;

    double delay = (double)rx_delay;
    *rx1_start = uplink_end_sec + delay;
    *rx2_start = uplink_end_sec + delay + 1.0;
}
