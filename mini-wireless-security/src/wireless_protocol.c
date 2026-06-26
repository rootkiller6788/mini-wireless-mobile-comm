/**
 * wireless_protocol.c — Security Protocol Configuration and RSN IE
 *
 * Covers: Security protocol selection (WEP/WPA/WPA2/WPA3),
 *         RSN IE construction and negotiation,
 *         SAE Dragonfly key exchange basics,
 *         OWE (Opportunistic Wireless Encryption)
 *
 * Knowledge Levels: L1 (protocol type system), L6 (protocol configuration),
 *                   L8 (WPA3/SAE, OWE)
 *
 * References:
 *   IEEE 802.11-2020, Section 9.4.2.25: RSNE
 *   WPA3 Specification v3.0 (Wi-Fi Alliance)
 *   RFC 7664 — Dragonfly Key Exchange
 *   RFC 8110 — Opportunistic Wireless Encryption
 */

#include "wireless_protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * L6: Security Configuration Management
 * ============================================================================ */

void sec_config_init(sec_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(sec_config_t));
    cfg->protocol = SEC_PROTO_NONE;
    cfg->akm = AKM_RESERVED;
    cfg->pairwise = CIPHER_PAIRWISE_NONE;
    cfg->group = CIPHER_GROUP_NONE;
    cfg->mfp_capable = 0;
    cfg->mfp_required = 0;
    cfg->pmk_caching = 0;
    cfg->preauth_enabled = 0;
    cfg->use_8021x = 0;
    key_mgmt_init(&cfg->key_ctx);
}

int sec_config_set_wpa2_personal(sec_config_t *cfg,
                                  const uint8_t *ssid, size_t ssid_len,
                                  const char *passphrase)
{
    if (!cfg || !ssid || !passphrase) return -1;
    if (ssid_len > sizeof(cfg->ssid)) return -1;

    sec_config_init(cfg);

    cfg->protocol = SEC_PROTO_WPA2;
    cfg->akm = AKM_PSK;
    cfg->pairwise = CIPHER_PAIRWISE_CCMP;
    cfg->group = CIPHER_GROUP_CCMP;

    memcpy(cfg->ssid, ssid, ssid_len);
    cfg->ssid_len = ssid_len;
    strncpy(cfg->passphrase, passphrase, sizeof(cfg->passphrase) - 1);

    /* Set up key management */
    memcpy(cfg->key_ctx.ssid, ssid, ssid_len);
    cfg->key_ctx.ssid_len = ssid_len;
    cfg->key_ctx.cipher_suite = CIPHER_SUITE_CCMP;

    /* Derive PMK from passphrase */
    key_mgmt_derive_from_passphrase(&cfg->key_ctx, passphrase);

    return 0;
}

int sec_config_set_wpa3_personal(sec_config_t *cfg,
                                  const uint8_t *ssid, size_t ssid_len,
                                  const char *password)
{
    if (!cfg || !ssid || !password) return -1;
    if (ssid_len > sizeof(cfg->ssid)) return -1;

    sec_config_init(cfg);

    cfg->protocol = SEC_PROTO_WPA3;
    cfg->akm = AKM_SAE;
    cfg->pairwise = CIPHER_PAIRWISE_GCMP128;
    cfg->group = CIPHER_GROUP_GCMP128;
    cfg->mfp_required = 1;  /* WPA3 mandates MFP */
    cfg->mfp_capable = 1;

    memcpy(cfg->ssid, ssid, ssid_len);
    cfg->ssid_len = ssid_len;
    strncpy(cfg->passphrase, password, sizeof(cfg->passphrase) - 1);

    /* Initialize SAE context */
    cfg->sae.state = SAE_STATE_INIT;
    memcpy(cfg->sae.password, password,
           strlen(password) < sizeof(cfg->sae.password) ?
           strlen(password) : sizeof(cfg->sae.password) - 1);
    cfg->sae.password_len = strlen(password);

    return 0;
}

int sec_config_set_wpa2_enterprise(sec_config_t *cfg,
                                    const uint8_t *ssid, size_t ssid_len,
                                    int eap_type)
{
    if (!cfg || !ssid) return -1;
    if (ssid_len > sizeof(cfg->ssid)) return -1;

    sec_config_init(cfg);

    cfg->protocol = SEC_PROTO_WPA2;
    cfg->akm = AKM_8021X;
    cfg->pairwise = CIPHER_PAIRWISE_CCMP;
    cfg->group = CIPHER_GROUP_CCMP;
    cfg->use_8021x = 1;
    cfg->eap_type = eap_type;

    memcpy(cfg->ssid, ssid, ssid_len);
    cfg->ssid_len = ssid_len;

    return 0;
}

int sec_config_set_wpa3_enterprise(sec_config_t *cfg,
                                    const uint8_t *ssid, size_t ssid_len)
{
    if (!cfg || !ssid) return -1;
    if (ssid_len > sizeof(cfg->ssid)) return -1;

    sec_config_init(cfg);

    /* WPA3-Enterprise uses 192-bit security level */
    cfg->protocol = SEC_PROTO_WPA3_ENT;
    cfg->akm = AKM_8021X_SHA256;
    cfg->pairwise = CIPHER_PAIRWISE_GCMP256;
    cfg->group = CIPHER_GROUP_GCMP256;
    cfg->use_8021x = 1;
    cfg->mfp_required = 1;
    cfg->mfp_capable = 1;

    memcpy(cfg->ssid, ssid, ssid_len);
    cfg->ssid_len = ssid_len;

    return 0;
}

int sec_config_set_owe(sec_config_t *cfg,
                        const uint8_t *ssid, size_t ssid_len)
{
    if (!cfg || !ssid) return -1;
    if (ssid_len > sizeof(cfg->ssid)) return -1;

    sec_config_init(cfg);

    cfg->protocol = SEC_PROTO_WPA3;  /* OWE is considered a WPA3 feature */
    cfg->akm = AKM_OWE;
    cfg->pairwise = CIPHER_PAIRWISE_GCMP128;
    cfg->group = CIPHER_GROUP_GCMP128;

    memcpy(cfg->ssid, ssid, ssid_len);
    cfg->ssid_len = ssid_len;

    return 0;
}

int sec_config_get_security_level(const sec_config_t *cfg)
{
    if (!cfg) return 0;

    /* Rating: 0 (open) to 5 (WPA3-Enterprise 192-bit) */
    switch (cfg->protocol) {
    case SEC_PROTO_NONE:      return 0;
    case SEC_PROTO_WEP:       return 1;  /* Broken, but "encrypted" */
    case SEC_PROTO_WPA:       return 2;  /* TKIP, known weaknesses */
    case SEC_PROTO_WPA2:      return (cfg->use_8021x ? 4 : 3);
    case SEC_PROTO_WPA3:      return 4;
    case SEC_PROTO_WPA3_ENT:  return 5;
    default:                  return 0;
    }
}

const char* sec_config_protocol_name(security_protocol_t proto)
{
    switch (proto) {
    case SEC_PROTO_NONE:      return "Open (no encryption)";
    case SEC_PROTO_WEP:       return "WEP (broken)";
    case SEC_PROTO_WPA:       return "WPA (TKIP, deprecated)";
    case SEC_PROTO_WPA2:      return "WPA2 (AES-CCMP)";
    case SEC_PROTO_WPA3:      return "WPA3 (AES-GCMP + SAE)";
    case SEC_PROTO_WPA3_ENT:  return "WPA3-Enterprise (192-bit)";
    default:                  return "Unknown";
    }
}

/* ============================================================================
 * L5: RSN IE Construction and Parsing
 * ============================================================================ */

/**
 * 802.11 OUI for WPA/WPA2 cipher suites
 *
 * OUI 00-0F-AC is the Wi-Fi Alliance assigned OUI for 802.11 cipher suites.
 */
static const uint8_t oui_wfa[3] = {0x00, 0x0F, 0xAC};

int rsn_ie_build(const sec_config_t *cfg, uint8_t *buf, size_t max_len)
{
    size_t pos = 0;
    uint8_t pair_count, akm_count;

    if (!cfg || !buf || max_len < 20) return -1;

    /* Element ID */
    buf[pos++] = 0x30; /* RSN IE */

    /* Length field (computed after IE body built; see final write-back below) */
    size_t len_pos = pos;
    buf[pos++] = 0x00;

    /* Version: 0x01 0x00 (little-endian 1) */
    buf[pos++] = 0x01;
    buf[pos++] = 0x00;

    /* Group cipher suite: OUI + type */
    memcpy(buf + pos, oui_wfa, 3); pos += 3;
    buf[pos++] = (uint8_t)cfg->group;

    /* Pairwise cipher suite count */
    pair_count = 1;
    buf[pos++] = pair_count;
    buf[pos++] = 0x00;

    /* Pairwise cipher suite list */
    memcpy(buf + pos, oui_wfa, 3); pos += 3;
    buf[pos++] = (uint8_t)cfg->pairwise;

    /* AKM suite count */
    akm_count = 1;
    buf[pos++] = akm_count;
    buf[pos++] = 0x00;

    /* AKM suite list */
    memcpy(buf + pos, oui_wfa, 3); pos += 3;
    buf[pos++] = (uint8_t)cfg->akm;

    /* RSN Capabilities */
    {
        uint16_t caps = 0;
        if (cfg->mfp_capable) caps |= RSN_CAP_MFP_CAPABLE;
        if (cfg->mfp_required) caps |= RSN_CAP_MFP_REQUIRED;
        if (cfg->pmk_caching) caps |= RSN_CAP_PTKSA_CACHE_4;
        if (cfg->preauth_enabled) caps |= RSN_CAP_PREAUTH;
        buf[pos++] = (uint8_t)(caps & 0xFF);
        buf[pos++] = (uint8_t)(caps >> 8);
    }

    /* PMKID count (0 for initial association) */
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;

    /* Optional: Group Management Cipher Suite for MFP */
    if (cfg->mfp_capable) {
        memcpy(buf + pos, oui_wfa, 3); pos += 3;
        buf[pos++] = 0x06; /* BIP (Broadcast/Multicast Integrity Protocol) */
    }

    /* Fill in length */
    buf[len_pos] = (uint8_t)(pos - len_pos - 1);

    return (int)pos;
}

int rsn_ie_parse(const uint8_t *buf, size_t len, sec_config_t *cfg)
{
    size_t pos;
    uint16_t pair_count, akm_count, pmkid_count;

    if (!buf || !cfg || len < 8) return -1;
    if (buf[0] != 0x30) return -1;  /* Not an RSN IE */

    sec_config_init(cfg);

    pos = 2;  /* Skip element ID and length */

    /* Version */
    uint16_t version = buf[pos] | (buf[pos+1] << 8);
    if (version != 1) return -1;
    pos += 2;

    /* Group cipher suite */
    if (memcmp(buf + pos, oui_wfa, 3) == 0) {
        cfg->group = (cipher_group_t)buf[pos + 3];
    }
    pos += 4;

    /* Pairwise cipher count */
    pair_count = buf[pos] | (buf[pos+1] << 8);
    pos += 2;

    /* Pairwise cipher list */
    if (pair_count > 0 && pos + 4 * pair_count <= len) {
        if (memcmp(buf + pos, oui_wfa, 3) == 0) {
            cfg->pairwise = (cipher_pairwise_t)buf[pos + 3];
        }
        pos += 4 * pair_count;
    }

    /* AKM count */
    akm_count = buf[pos] | (buf[pos+1] << 8);
    pos += 2;

    /* AKM list */
    if (akm_count > 0 && pos + 4 * akm_count <= len) {
        if (memcmp(buf + pos, oui_wfa, 3) == 0) {
            cfg->akm = (akm_suite_t)buf[pos + 3];
        }
        pos += 4 * akm_count;
    }

    /* RSN capabilities */
    if (pos + 2 <= len) {
        uint16_t caps = buf[pos] | (buf[pos+1] << 8);
        cfg->mfp_capable  = (caps & RSN_CAP_MFP_CAPABLE) ? 1 : 0;
        cfg->mfp_required  = (caps & RSN_CAP_MFP_REQUIRED) ? 1 : 0;
        cfg->preauth_enabled = (caps & RSN_CAP_PREAUTH) ? 1 : 0;
        pos += 2;
    }

    /* PMKID count */
    if (pos + 2 <= len) {
        pmkid_count = buf[pos] | (buf[pos+1] << 8);
        pos += 2;
        if (pmkid_count > 0) {
            cfg->pmk_caching = 1;
            pos += pmkid_count * 16;  /* Skip PMKID list */
        }
    }

    /* Determine protocol from cipher and AKM */
    if (cfg->pairwise == CIPHER_PAIRWISE_GCMP256 ||
        cfg->pairwise == CIPHER_PAIRWISE_CCMP256) {
        cfg->protocol = SEC_PROTO_WPA3_ENT;
    } else if (cfg->akm == AKM_SAE || cfg->akm == AKM_FT_SAE) {
        cfg->protocol = SEC_PROTO_WPA3;
    } else if (cfg->pairwise == CIPHER_PAIRWISE_CCMP) {
        cfg->protocol = SEC_PROTO_WPA2;
    } else if (cfg->pairwise == CIPHER_PAIRWISE_TKIP) {
        cfg->protocol = SEC_PROTO_WPA;
    } else if (cfg->group == CIPHER_GROUP_WEP40 ||
               cfg->group == CIPHER_GROUP_WEP104) {
        cfg->protocol = SEC_PROTO_WEP;
    }

    return 0;
}

int rsn_ie_negotiate(const sec_config_t *ap_cfg,
                      const sec_config_t *sta_cfg,
                      sec_config_t *result)
{
    if (!ap_cfg || !sta_cfg || !result) return -1;

    sec_config_init(result);

    /* Try to agree on highest common security level */

    /* WPA3 Enterprise (192-bit) → both must support it */
    if (ap_cfg->protocol == SEC_PROTO_WPA3_ENT &&
        sta_cfg->protocol >= SEC_PROTO_WPA3_ENT &&
        ap_cfg->akm == AKM_8021X_SHA256 &&
        sta_cfg->akm == AKM_8021X_SHA256) {
        sec_config_set_wpa3_enterprise(result, ap_cfg->ssid, ap_cfg->ssid_len);
        return 0;
    }

    /* WPA3 Personal (SAE) */
    if (ap_cfg->akm == AKM_SAE && sta_cfg->akm == AKM_SAE) {
        result->protocol = SEC_PROTO_WPA3;
        result->akm = AKM_SAE;
        result->pairwise = CIPHER_PAIRWISE_GCMP128;
        result->group = CIPHER_GROUP_GCMP128;
        result->mfp_required = 1;
        result->mfp_capable = 1;
        return 0;
    }

    /* WPA2 Enterprise */
    if (ap_cfg->akm == AKM_8021X && sta_cfg->akm == AKM_8021X) {
        sec_config_set_wpa2_enterprise(result, ap_cfg->ssid, ap_cfg->ssid_len,
                                        ap_cfg->eap_type);
        return 0;
    }

    /* WPA2 Personal */
    if (ap_cfg->akm == AKM_PSK && sta_cfg->akm == AKM_PSK) {
        result->protocol = SEC_PROTO_WPA2;
        result->akm = AKM_PSK;
        result->pairwise = CIPHER_PAIRWISE_CCMP;
        result->group = CIPHER_GROUP_CCMP;
        return 0;
    }

    /* OWE */
    if (ap_cfg->akm == AKM_OWE && sta_cfg->akm == AKM_OWE) {
        sec_config_set_owe(result, ap_cfg->ssid, ap_cfg->ssid_len);
        return 0;
    }

    /* No common security — fall back to open? */
    return -1;
}

/* ============================================================================
 * L8: SAE (Dragonfly) — Simplified implementation
 * ============================================================================ */

/**
 * sae_derive_password_element — Derive PWE (Password Element)
 *
 * In a full SAE implementation, this uses hash-to-curve to find
 * a point on the elliptic curve.  For this simplified version,
 * we derive a deterministic secret from the password for commit.
 *
 * L4 Theorem (RFC 7664): The Dragonfly key exchange is secure
 * against passive attacks under the CDH assumption and against
 * active attacks if the password has sufficient entropy.
 */
static void sae_derive_password_element(const sae_ctx_t *ctx,
                                         uint8_t *pwe, size_t pwe_len)
{
    uint8_t input[6 + 6 + 64]; /* MyMAC || PeerMAC || Password */
    size_t pos = 0;

    memcpy(input + pos, ctx->my_mac, 6);     pos += 6;
    memcpy(input + pos, ctx->peer_mac, 6);    pos += 6;
    memcpy(input + pos, ctx->password, ctx->password_len);
    pos += ctx->password_len;

    /* PWE = Hash(MyMAC || PeerMAC || Password) */
    sha256_hash(input, pos, pwe);
    if (pwe_len > SHA256_DIGEST_SIZE) {
        memset(pwe + SHA256_DIGEST_SIZE, 0, pwe_len - SHA256_DIGEST_SIZE);
    }
}

/**
 * sae_commit — Generate SAE commit (scalar + element)
 *
 * Simplified: scalar = random, element = Hash(scalar || PWE)
 */
int sae_commit(sae_ctx_t *ctx, sae_commit_t *commit)
{
    uint8_t pwe[32];
    uint8_t commit_input[64 + 32]; /* scalar || PWE */

    if (!ctx || !commit) return -1;

    /* Derive password element */
    sae_derive_password_element(ctx, pwe, sizeof(pwe));

    /* Generate random scalar */
    if (generate_nonce(commit->scalar.data, 32) < 0) return -1;
    commit->scalar.len = 32;

    /* Element = Hash(scalar || PWE) — simplified EC point derivation */
    memcpy(commit_input, commit->scalar.data, 32);
    memcpy(commit_input + 32, pwe, 32);
    sha256_hash(commit_input, 64, commit->element.data);
    commit->element.len = 32;

    ctx->state = SAE_STATE_COMMITTED;
    return 0;
}

/**
 * sae_process_commit — Process peer's commit and generate confirm
 */
int sae_process_commit(sae_ctx_t *ctx,
                        const sae_commit_t *peer_commit,
                        sae_confirm_t *my_confirm)
{
    uint8_t shared_key[32];
    uint8_t confirm_input[32 + 32]; /* scalar || peer_scalar */

    if (!ctx || !peer_commit || !my_confirm) return -1;
    if (ctx->state != SAE_STATE_COMMITTED) return -1;

    /* Store peer commit */
    memcpy(&ctx->peer_scalar, &peer_commit->scalar, sizeof(sae_element_t));
    memcpy(&ctx->peer_element, &peer_commit->element, sizeof(sae_element_t));

    /* Shared secret = Hash(my_scalar || peer_scalar || peer_element)
       (simplified — real SAE uses EC Diffie-Hellman) */
    memcpy(confirm_input, ctx->my_scalar.data, 32);
    memcpy(confirm_input + 32, peer_commit->scalar.data, 32);
    sha256_hash(confirm_input, 64, shared_key);

    /* PMK = shared_key */
    memcpy(ctx->pmk, shared_key, PMK_LEN);

    /* Confirm = HMAC(shared_key, my_scalar || peer_scalar || my_element || peer_element) */
    {
        uint8_t c_data[128];
        size_t cp = 0;
        memcpy(c_data + cp, ctx->my_scalar.data, 32);   cp += 32;
        memcpy(c_data + cp, ctx->peer_scalar.data, 32);  cp += 32;
        memcpy(c_data + cp, ctx->my_element.data, 32);    cp += 32;
        memcpy(c_data + cp, ctx->peer_element.data, 32);  cp += 32;

        hmac_sha256(shared_key, 32, c_data, cp, my_confirm->confirm);
    }

    ctx->state = SAE_STATE_CONFIRMED;
    return 0;
}

/**
 * sae_verify_confirm — Verify peer's confirm
 */
int sae_verify_confirm(sae_ctx_t *ctx, const sae_confirm_t *peer_confirm)
{
    uint8_t shared_key[32];
    uint8_t expected_confirm[32];

    if (!ctx || !peer_confirm) return -1;
    if (ctx->state != SAE_STATE_CONFIRMED) return -1;

    /* Recompute shared secret */
    {
        uint8_t confirm_input[64];
        memcpy(confirm_input, ctx->my_scalar.data, 32);
        memcpy(confirm_input + 32, ctx->peer_scalar.data, 32);
        sha256_hash(confirm_input, 64, shared_key);
    }

    /* Expected confirm (swap scalar order for peer's perspective) */
    {
        uint8_t c_data[128];
        size_t cp = 0;
        memcpy(c_data + cp, ctx->peer_scalar.data, 32);  cp += 32;
        memcpy(c_data + cp, ctx->my_scalar.data, 32);    cp += 32;
        memcpy(c_data + cp, ctx->peer_element.data, 32);  cp += 32;
        memcpy(c_data + cp, ctx->my_element.data, 32);    cp += 32;

        hmac_sha256(shared_key, 32, c_data, cp, expected_confirm);
    }

    if (constant_time_memcmp(expected_confirm, peer_confirm->confirm, 32) != 0) {
        ctx->state = SAE_STATE_REJECTED;
        return -1;
    }

    ctx->state = SAE_STATE_ACCEPTED;
    return 0;
}
