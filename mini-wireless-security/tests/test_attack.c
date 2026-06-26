/**
 * test_attack.c - Tests for attacks and IDS
 */
#include "wireless_attack.h"
#include "wireless_auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
static int p=0,f_=0;
#define T(n) printf("  TEST: %-45s ... ", n)
#define P() do { printf("PASS\n"); p++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); f_++; return; } while(0)
#define C(c,m) do { if(!(c))F(m); } while(0)
static void t_fms(void) {
    T("FMS attack init");
    wep_fms_state_t st; wep_fms_init(&st,5);
    C(st.target_key_len==5,"len"); uint8_t iv[3]={3,255,0};
    wep_fms_add_packet(&st,iv,0x42);
    uint32_t t,w; int rb; wep_fms_stats(&st,&t,&w,&rb);
    C(t==1,"total"); if(st.captured)free(st.captured); P();
}
static void t_dict(void) {
    T("Dictionary attack");
    wpa2_dictionary_state_t st; pmkid_capture_t tgt;
    memset(&tgt,0,sizeof(tgt));
    memcpy(tgt.ssid,"Net",3); tgt.ssid_len=3;
    uint8_t m[6]={0,1,2,3,4,5};
    memcpy(tgt.ap_mac,m,6); memcpy(tgt.sta_mac,m,6);
    uint8_t pmk[32];
    pbkdf2_hmac_sha256((const uint8_t*)"pw",2,tgt.ssid,3,4096,pmk,32);
    handshake_compute_pmkid(pmk,m,m,tgt.pmkid);
    wpa2_dict_attack_init(&st,&tgt);
    C(wpa2_dict_attack_try(&st,"wrong")==0,"false positive");
    C(wpa2_dict_attack_try(&st,"pw")==1,"false negative");
    C(st.found==1,"found"); P();
}
static void t_ids(void) {
    T("Wireless IDS");
    ids_state_t ids; ids_init(&ids,60);
    for(int i=0;i<20;i++)ids_process_frame(&ids,0,12,0);
    C(ids_check_alarms(&ids)&0x01,"deauth alarm");
    ids_reset_alarms(&ids); C(ids_check_alarms(&ids)==0,"clear"); P();
}
int main(void) {
    printf("=== Test: wireless_attack ===\n");
    t_fms(); t_dict(); t_ids();
    printf("\n=== %d passed, %d failed ===\n",p,f_);
    return f_>0?1:0;
}
