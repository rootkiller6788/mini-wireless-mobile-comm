/**
 * test_protocol.c - Tests for security protocols
 */
#include "wireless_protocol.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int pass=0,f_=0;
#define T(n) printf("  TEST: %-45s ... ", n)
#define P() do { printf("PASS\n"); pass++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); f_++; return; } while(0)
#define C(c,m) do { if(!(c))F(m); } while(0)
static void t_wpa2(void) {
    T("WPA2-Personal config");
    sec_config_t cfg; uint8_t s[]="MyWiFi";
    C(sec_config_set_wpa2_personal(&cfg,s,6,"pass")==0,"setup");
    C(cfg.protocol==SEC_PROTO_WPA2,"proto"); C(cfg.akm==AKM_PSK,"akm"); P();
}
static void t_wpa3(void) {
    T("WPA3-Personal config");
    sec_config_t cfg; uint8_t s[]="WPA3";
    C(sec_config_set_wpa3_personal(&cfg,s,4,"pw")==0,"setup");
    C(cfg.protocol==SEC_PROTO_WPA3,"proto"); C(cfg.mfp_required==1,"mfp"); P();
}
static void t_rsn_ie(void) {
    T("RSN IE round-trip");
    sec_config_t cc,parsed; uint8_t b[256],s[]="Net";
    sec_config_set_wpa2_personal(&cc,s,3,"pw");
    int l=rsn_ie_build(&cc,b,256); C(l>0,"build");
    C(rsn_ie_parse(b,l,&parsed)==0,"parse"); C(parsed.protocol==SEC_PROTO_WPA2,"match"); P();
}
int main(void) {
    printf("=== Test: wireless_protocol ===\n");
    t_wpa2(); t_wpa3(); t_rsn_ie();
    printf("\n=== %d passed, %d failed ===\n",pass,f_);
    return f_>0?1:0;
}
