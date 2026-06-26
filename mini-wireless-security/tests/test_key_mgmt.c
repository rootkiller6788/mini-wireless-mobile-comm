/**
 * test_key_mgmt.c - Tests for key management
 */
#include "wireless_key_mgmt.h"
#include "wireless_auth.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int p=0,f_=0;
#define T(n) printf("  TEST: %-45s ... ", n)
#define P() do { printf("PASS\n"); p++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); f_++; return; } while(0)
#define C(c,m) do { if(!(c))F(m); } while(0)
static void t_prf(void) {
    T("WPA2 PRF");
    uint8_t k[32],c[76],o[48]; memset(k,0xAA,32); memset(c,0xBB,76);
    wpa2_prf(k,32,"Test",c,76,o,48);
    int nz=0; for(int i=0;i<48;i++)if(o[i])nz=1; C(nz,"all-zero"); P();
}
static void t_ptk(void) {
    T("PTK derivation");
    key_mgmt_ctx_t ctx; key_mgmt_init(&ctx);
    memcpy(ctx.ssid,"Test",4); ctx.ssid_len=4;
    uint8_t a[6]={0,1,2,3,4,5},s[6]={6,7,8,9,10,11};
    memcpy(ctx.authenticator_mac,a,6); memcpy(ctx.supplicant_mac,s,6);
    generate_nonce(ctx.anonce,32); generate_nonce(ctx.snonce,32);
    C(key_mgmt_derive_from_passphrase(&ctx,"test")==0,"PMK");
    C(derive_ptk(&ctx)==0,"PTK");
    const uint8_t*tk=key_mgmt_get_tk(&ctx); C(tk!=NULL,"NULL"); P();
}
static void t_hkdf(void) {
    T("HKDF");
    uint8_t o[32];
    hkdf((const uint8_t*)"salt",4,(const uint8_t*)"ikm",3,(const uint8_t*)"info",4,o,32);
    int nz=0; for(int i=0;i<32;i++)if(o[i])nz=1; C(nz,"all-zero"); P();
}
int main(void) {
    printf("=== Test: wireless_key_mgmt ===\n");
    t_prf(); t_ptk(); t_hkdf();
    printf("\n=== %d passed, %d failed ===\n",p,f_);
    return f_>0?1:0;
}
