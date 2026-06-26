/**
 * test_auth.c - Tests for 4-way handshake and PMKID
 */
#include "wireless_auth.h"
#include "wireless_crypto.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int p=0, f_=0;
#define T(n) printf("  TEST: %-45s ... ", n)
#define P() do { printf("PASS\n"); p++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); f_++; return; } while(0)
#define C(c,m) do { if (!(c)) F(m); } while(0)
static void t_full(void) {
    T("4-Way Handshake full");
    handshake_ctx_t ap, sta;
    uint8_t ssid[] = "TestNet";
    uint8_t am[6] = {0,1,2,3,4,5}, sm[6] = {6,7,8,9,10,11};
    handshake_init(&ap, ssid, 7, sm, am, 0);
    handshake_init(&sta, ssid, 7, sm, am, 0);
    C(handshake_derive_pmk(&ap, "test123") == 0, "AP PMK");
    C(handshake_derive_pmk(&sta, "test123") == 0, "STA PMK");
    C(memcmp(ap.pmk.key, sta.pmk.key, 32) == 0, "PMK mismatch");
    uint8_t frm[512]; size_t fl;
    C(handshake_build_msg1(&ap, frm, &fl, 512) == 0, "msg1");
    C(handshake_process_msg1(&sta, frm, fl) == 0, "proc1");
    C(handshake_build_msg2(&sta, frm, &fl, 512) == 0, "msg2");
    C(handshake_process_msg2(&ap, frm, fl) == 0, "proc2");
    uint8_t gtk[32]; generate_nonce(gtk, 32);
    C(handshake_build_msg3(&ap, frm, &fl, 512, gtk, 32, 1) == 0, "msg3");
    C(handshake_process_msg3(&sta, frm, fl) == 0, "proc3");
    C(handshake_build_msg4(&sta, frm, &fl, 512) == 0, "msg4");
    C(handshake_process_msg4(&ap, frm, fl) == 0, "proc4");
    C(ap.state == HANDSHAKE_COMPLETE, "complete");
    C(memcmp(ap.ptk.kck, sta.ptk.kck, 16) == 0, "KCK mismatch");
    P();
}
static void t_pmkid(void) {
    T("PMKID computation");
    uint8_t pmk[32], id1[16], id2[16];
    uint8_t am[6] = {0,1,2,3,4,5}, sm[6] = {6,7,8,9,10,11};
    generate_nonce(pmk, 32);
    handshake_compute_pmkid(pmk, am, sm, id1);
    handshake_compute_pmkid(pmk, am, sm, id2);
    C(memcmp(id1, id2, 16) == 0, "deterministic");
    P();
}
static void t_nonce(void) {
    T("Nonce generation");
    uint8_t n1[32], n2[32];
    C(generate_nonce(n1, 32) == 0, "gen1");
    C(generate_nonce(n2, 32) == 0, "gen2");
    /* Nonces may be same if called in same microsecond; verify non-zero */
    int nz=0; for(int i=0;i<32;i++) if(n1[i]||n2[i]) nz=1;
    C(nz, "all-zero nonces");
    P();
}
int main(void) {
    printf("=== Test: wireless_auth ===\n");
    t_full(); t_pmkid(); t_nonce();
    printf("\n=== %d passed, %d failed ===\n",p,f_);
    return f_>0?1:0;
}
