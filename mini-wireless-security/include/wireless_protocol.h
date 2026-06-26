/**
 * wireless_protocol.h — Wireless Security Protocols: WEP, WPA, WPA2, WPA3
 *
 * Covers: Protocol state machines, cipher suite negotiation, RSN IE,
 *         SAE (Simultaneous Authentication of Equals), OWE, DPP
 * Knowledge Levels: L1 (protocol enums), L2 (security evolution concept),
 *                   L6 (WPA2/WPA3 protocol implementation),
 *                   L8 (WPA3/SAE, OWE)
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless (WPA3/SAE Dragonfly)
 *   MIT 6.858 — Computer Systems Security (WiFi security evolution)
 *   Georgia Tech ECE 6601 — Communications (protocol security)
 *
 * References:
 *   IEEE 802.11-2020: Wireless LAN Medium Access Control
 *   IEEE 802.11i-2004: Security Enhancements
 *   WPA3 Specification v3.0 (Wi-Fi Alliance)
 *   RFC 7664: Dragonfly Key Exchange (used in SAE)
 */

#ifndef WIRELESS_PROTOCOL_H
#define WIRELESS_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wireless_crypto.h"
#include "wireless_auth.h"
#include "wireless_key_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Security Protocol Type Definitions
 * ============================================================================ */

/** Security protocol generations */
typedef enum {
    SEC_PROTO_NONE      = 0,     /* Open network */
    SEC_PROTO_WEP       = 1,     /* Wired Equivalent Privacy (broken) */
    SEC_PROTO_WPA       = 2,     /* Wi-Fi Protected Access (TKIP) */
    SEC_PROTO_WPA2      = 3,     /* WPA2 (AES-CCMP) */
    SEC_PROTO_WPA3      = 4,     /* WPA3 (AES-GCMP + SAE) */
    SEC_PROTO_WPA3_ENT  = 5      /* WPA3-Enterprise (192-bit) */
} security_protocol_t;

/** Authentication and Key Management (AKM) suite selectors */
typedef enum {
    AKM_RESERVED            = 0,
    AKM_8021X               = 1,     /* 802.1X (WPA-Enterprise) */
    AKM_PSK                 = 2,     /* Pre-Shared Key (WPA-Personal) */
    AKM_FT_8021X            = 3,     /* Fast BSS Transition + 802.1X */
    AKM_FT_PSK              = 4,     /* Fast BSS Transition + PSK */
    AKM_8021X_SHA256        = 5,     /* 802.1X with SHA-256 */
    AKM_PSK_SHA256          = 6,     /* PSK with SHA-256 */
    AKM_SAE                 = 8,     /* Simultaneous Auth of Equals (WPA3) */
    AKM_FT_SAE              = 9,     /* FT + SAE */
    AKM_OWE                 = 18,    /* Opportunistic Wireless Encryption */
    AKM_DPP                 = 19     /* Device Provisioning Protocol */
} akm_suite_t;

/** Cipher suite pairwise selectors */
typedef enum {
    CIPHER_PAIRWISE_NONE    = 0,
    CIPHER_PAIRWISE_TKIP    = 2,     /* WPA */
    CIPHER_PAIRWISE_CCMP    = 4,     /* WPA2 (AES-CCMP-128) */
    CIPHER_PAIRWISE_GCMP128 = 8,     /* WPA3 (AES-GCMP-128) */
    CIPHER_PAIRWISE_GCMP256 = 9,     /* WPA3-Enterprise (AES-GCMP-256) */
    CIPHER_PAIRWISE_CCMP256 = 10     /* WPA3-Enterprise (AES-CCMP-256) */
} cipher_pairwise_t;

/** Group cipher suite selectors */
typedef enum {
    CIPHER_GROUP_NONE       = 0,
    CIPHER_GROUP_WEP40      = 1,
    CIPHER_GROUP_TKIP       = 2,
    CIPHER_GROUP_WEP104     = 3,
    CIPHER_GROUP_CCMP       = 4,
    CIPHER_GROUP_GCMP128    = 8,
    CIPHER_GROUP_GCMP256    = 9
} cipher_group_t;

/** RSN Capabilities bitfield */
#define RSN_CAP_PREAUTH         0x0001
#define RSN_CAP_NO_PAIRWISE      0x0002
#define RSN_CAP_PTKSA_CACHE_1    0x0004   /* 1 PTKSA replay counter */
#define RSN_CAP_PTKSA_CACHE_4    0x0008   /* 4 PTKSA replay counters */
#define RSN_CAP_PTKSA_CACHE_16   0x000C   /* 16 PTKSA replay counters */
#define RSN_CAP_MFP_REQUIRED     0x0040   /* Management Frame Protection req */
#define RSN_CAP_MFP_CAPABLE       0x0080   /* Management Frame Protection cap */
#define RSN_CAP_PEERKEY_ENABLED  0x0200
#define RSN_CAP_SPP_AMSDU_CAP    0x0400
#define RSN_CAP_SPP_AMSDU_REQ    0x0800

/* ============================================================================
 * RSN Information Element (IEEE 802.11, Section 9.4.2.25)
 * ============================================================================ */

/** RSN IE header */
typedef struct {
    uint8_t  element_id;        /* 0x30 = RSN IE */
    uint8_t  length;            /* Variable */
    uint16_t version;           /* RSN version (1) */
} rsn_ie_header_t;

/** RSN IE body — contains cipher and AKM suite lists */
typedef struct {
    /* Group cipher suite (4 bytes OUI + type) */
    uint8_t  group_cipher_oui[3];
    uint8_t  group_cipher_type;

    /* Pairwise cipher suite count */
    uint16_t pairwise_count;
    /* Followed by: pairwise_count * 4-byte cipher suite selectors */

    /* AKM suite count */
    uint16_t akm_count;
    /* Followed by: akm_count * 4-byte AKM suite selectors */

    /* RSN capabilities */
    uint16_t rsn_capabilities;

    /* PMKID count (optional) */
    uint16_t pmkid_count;
    /* Followed by: pmkid_count * 16-byte PMKID values */

    /* Group management cipher suite (optional) */
    /* 4 bytes */
} rsn_ie_body_t;

/* ============================================================================
 * WPA3 SAE (Simultaneous Authentication of Equals) — RFC 7664
 * ============================================================================ */

/**
 * SAE is a zero-knowledge password proof (ZKPP) protocol based on
 * the Dragonfly key exchange.  It replaces the 4-way handshake PSK
 * in WPA3-Personal.
 *
 * Core idea: Both parties commit to a password element P (derived
 * from the password via hash-to-curve), exchange scalars and elements,
 * and confirm knowledge of the shared secret.
 *
 * L4 Theorem: Under the Computational Diffie-Hellman (CDH) assumption
 * on the elliptic curve, SAE provides mutual authentication and a
 * secure session key.
 */

/** SAE finite field element (for MODP groups) */
typedef struct {
    uint8_t  data[384];          /* Big-integer representation */
    size_t   len;                /* Actual length */
} sae_element_t;

/** SAE commit message (first exchange) */
typedef struct {
    sae_element_t scalar;        /* Private scalar */
    sae_element_t element;       /* Public element */
} sae_commit_t;

/** SAE confirm message (second exchange) */
typedef struct {
    uint8_t  confirm[32];        /* HMAC-SHA256 based confirmation */
} sae_confirm_t;

/** SAE state machine */
typedef enum {
    SAE_STATE_INIT = 0,
    SAE_STATE_COMMITTED,         /* Commit sent */
    SAE_STATE_CONFIRMED,         /* Confirm sent/received */
    SAE_STATE_ACCEPTED,          /* Handshake complete */
    SAE_STATE_REJECTED           /* Verification failed */
} sae_state_t;

/** SAE handshake context */
typedef struct {
    sae_state_t    state;
    uint8_t        password[64]; /* User password */
    size_t         password_len;

    /* Derived password element P = H(pw | MAC1 | MAC2) on curve */
    uint8_t        peer_mac[6];
    uint8_t        my_mac[6];

    /* Commit values */
    sae_element_t  my_scalar;
    sae_element_t  my_element;
    sae_element_t  peer_scalar;
    sae_element_t  peer_element;

    /* Derived shared secret */
    uint8_t        pmk[PMK_LEN];
    uint8_t        pmkid[WPA_PMKID_LEN];

    /* Anti-clogging token (defense against DoS) */
    uint8_t        token[32];
    int            token_len;
    int            token_requested;
} sae_ctx_t;

/* ============================================================================
 * OWE (Opportunistic Wireless Encryption) — RFC 8110
 * ============================================================================ */

/**
 * OWE provides encryption for open networks without user authentication.
 * Uses Diffie-Hellman key exchange embedded in 802.11 association.
 */

typedef struct {
    uint8_t   public_key[32];     /* DH public key (X25519) */
    uint8_t   private_key[32];    /* DH private key */
    uint8_t   shared_secret[32];  /* Derived shared secret */
    uint8_t   pmk[PMK_LEN];       /* Pairwise Master Key */
} owe_ctx_t;

/* ============================================================================
 * L6: Protocol Configuration and Management
 * ============================================================================ */

/**
 * Security configuration for a BSS (AP) or STA (client)
 */
typedef struct {
    security_protocol_t protocol;  /* Current protocol */
    akm_suite_t         akm;       /* AKM suite in use */
    cipher_pairwise_t   pairwise;  /* Unicast cipher */
    cipher_group_t      group;     /* Group cipher */

    /* WPA2-Personal parameters */
    char     passphrase[64];
    uint8_t  ssid[32];
    size_t   ssid_len;

    /* 802.1X parameters (for Enterprise) */
    int      use_8021x;
    int      eap_type;

    /* Key management */
    key_mgmt_ctx_t key_ctx;

    /* SAE context (WPA3-Personal) */
    sae_ctx_t sae;

    /* OWE context */
    owe_ctx_t owe;

    /* Management Frame Protection */
    int      mfp_required;
    int      mfp_capable;

    /* PMK caching */
    int      pmk_caching;

    /* Pre-authentication */
    int      preauth_enabled;
} sec_config_t;

/* ============================================================================
 * L5: Protocol State Management Functions
 * ============================================================================ */

/**
 * sec_config_init — Initialize security configuration with defaults
 */
void sec_config_init(sec_config_t *cfg);

/**
 * sec_config_set_wpa2_personal — Configure for WPA2-Personal (PSK)
 */
int sec_config_set_wpa2_personal(sec_config_t *cfg,
                                  const uint8_t *ssid, size_t ssid_len,
                                  const char *passphrase);

/**
 * sec_config_set_wpa3_personal — Configure for WPA3-Personal (SAE)
 */
int sec_config_set_wpa3_personal(sec_config_t *cfg,
                                  const uint8_t *ssid, size_t ssid_len,
                                  const char *password);

/**
 * sec_config_set_wpa2_enterprise — Configure for WPA2-Enterprise (802.1X)
 */
int sec_config_set_wpa2_enterprise(sec_config_t *cfg,
                                    const uint8_t *ssid, size_t ssid_len,
                                    int eap_type);

/**
 * sec_config_set_wpa3_enterprise — Configure for WPA3-Enterprise (192-bit)
 */
int sec_config_set_wpa3_enterprise(sec_config_t *cfg,
                                    const uint8_t *ssid, size_t ssid_len);

/**
 * sec_config_set_owe — Configure for OWE (encrypted open)
 */
int sec_config_set_owe(sec_config_t *cfg,
                        const uint8_t *ssid, size_t ssid_len);

/**
 * sec_config_get_security_level — Return security level (0-5 stars)
 */
int sec_config_get_security_level(const sec_config_t *cfg);

/**
 * sec_config_protocol_name — Return human-readable protocol name
 */
const char* sec_config_protocol_name(security_protocol_t proto);

/* ============================================================================
 * L5: RSN IE Construction / Parsing
 * ============================================================================ */

/**
 * rsn_ie_build — Build an RSN IE from security configuration
 * @return Size of constructed IE, or -1 on error
 */
int rsn_ie_build(const sec_config_t *cfg, uint8_t *buf, size_t max_len);

/**
 * rsn_ie_parse — Parse an RSN IE into security configuration
 * @return 0 on success, -1 on invalid IE
 */
int rsn_ie_parse(const uint8_t *buf, size_t len, sec_config_t *cfg);

/**
 * rsn_ie_negotiate — Negotiate best common security between AP and STA RSN IEs
 * @return 0 if common security found, -1 if incompatible
 */
int rsn_ie_negotiate(const sec_config_t *ap_cfg,
                      const sec_config_t *sta_cfg,
                      sec_config_t *result);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_PROTOCOL_H */
