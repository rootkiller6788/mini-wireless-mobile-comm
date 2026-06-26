#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_model.h"
#include "../include/cell_network_link.h"
#include "../include/cell_network_scheduler.h"
#include "../include/cell_network_handover.h"
#include "../include/cell_network_power.h"

#define EPSILON 0.01
#define ASSERT_NEAR(a,b) do { double _a=(a),_b=(b); if (fabs(_a-_b)>EPSILON) {     printf("FAIL: %s=%.2f != %.2f at %d\n", #a, _a, _b, __LINE__); assert(0); } } while(0)

static int trun=0, tpass=0;
#define T(n) do { printf("  [TEST] " n "... "); trun++; } while(0)
#define P() do { printf("PASS\n"); tpass++; } while(0)

extern double erlang_b_blocking_prob(int,double);
extern int erlang_b_required_channels(double,double);
extern double erlang_c_waiting_prob(int,double);
extern double md1_avg_queue_length(double,double);

void test_defs(void) {
    T("numerology"); ASSERT_NEAR(numer_scs_khz(0),15.0); ASSERT_NEAR(numer_scs_khz(1),30.0); P();
    T("PCI"); uint32_t pci=pci_compute(100,2); assert(pci==302); P();
    T("NR band"); ASSERT_NEAR(nr_band_center_freq_mhz(NR_BAND_N78),3550.0); P();
    T("5QI"); qos_profile_t p[10]; assert(qos_init_standard_profiles(p,10)==10); P();
}

void test_hex(void) {
    T("hex coord"); double x,y; hex_coord_t h={2,1}; hex_to_cartesian(h,500.0,&x,&y); ASSERT_NEAR(x,2165.06); P();
    T("hex ring"); hex_coord_t ctr={0,0},ring[20]; assert(hex_ring(ctr,2,ring,20)==12); P();
    T("hex grid"); hex_coord_t g[40]; assert(hex_filled_grid(ctr,2,g,40)==19); P();
    T("freq reuse"); int N; assert(freq_reuse_validate(1,1,&N)&&N==3); P();
    T("NRT"); nrt_table_t nrt; nrt_init(&nrt); nrt_add(&nrt,1,2,HO_TYPE_X2,1); assert(nrt.num_entries==1); P();
}

void test_pathloss(void) {
    T("FSPL"); ASSERT_NEAR(fspl_db(1.0,2000.0),98.47); P();
    T("Hata"); assert(hata_urban_macro_db(900.0,5.0,30.0,1.5)>100.0); P();
    T("COST231"); assert(cost231_hata_db(1800.0,2.0,30.0,1.5,0.0)>100.0); P();
    T("3GPP UMa"); assert(tr38901_uma_los_db(1000.0,1000.0,3.5,25.0,1.5)>50.0); P();
}

void test_sinr(void) {
    T("noise floor"); ASSERT_NEAR(noise_floor_dbm(180000.0,5.0),-116.38); P();
    T("SINR"); double rx=rx_power_dbm(30.0,0.0,0.0,100.0,2.0,10.0); double s=sinr_db(rx,-120.0,-116.38); assert(s>0.0); P();
}

void test_shannon(void) {
    T("capacity"); ASSERT_NEAR(shannon_capacity_bps(1e6,100.0),6658211.48); P();
    T("link budget"); link_budget_t lb=compute_downlink_budget(43.0,15.0,2.0,0.0,5.0,20e6,0.0,120.0); assert(lb.tx_eirp_dbm>50.0); P();
}

void test_cqi(void) {
    T("CQI"); cqi_t c=sinr_to_cqi(15.0); assert(c>=10&&c<=12); P();
}

void test_scheduling(void) {
    T("round-robin"); sched_context_t ctx; sched_init(&ctx,SCHED_RR,100,1.0,0.01);
    sched_add_ue(&ctx,1,15.0,1000); sched_add_ue(&ctx,2,5.0,500);
    sched_decision_t dec=sched_round_robin(&ctx); assert(dec.num_allocated==2); P();
    T("max-C/I"); sched_init(&ctx,SCHED_MAX_CI,50,1.0,0.01);
    sched_add_ue(&ctx,1,5.0,500); sched_add_ue(&ctx,2,25.0,1000);
    dec=sched_max_ci(&ctx); assert(dec.total_cell_throughput_bps>0.0); P();
    T("proportional fair"); sched_init(&ctx,SCHED_PROPORTIONAL_FAIR,50,1.0,0.01);
    sched_add_ue(&ctx,1,10.0,500); sched_add_ue(&ctx,2,20.0,1000);
    dec=sched_proportional_fair(&ctx); assert(dec.num_allocated>=0); P();
    T("Jain fairness"); double t1[3]={10,10,10}; ASSERT_NEAR(sched_jain_fairness(t1,3),1.0); P();
}

void test_handover(void) {
    T("A3 event"); ho_config_t cfg; ho_config_init_default(&cfg);
    assert(ho_evaluate_event(HO_EVENT_A3,&cfg,-100.0,-90.0)==1); P();
    T("L3 filter"); double f=ho_l3_filter(-90.0,-85.0,4); assert(f>-90.0&&f<-85.0); P();
    T("ping-pong"); uint32_t h[]={1,2,1,2,1,3}; assert(ho_detect_ping_pong(h,6,2,1000.0)==1); P();
    T("MRO"); ho_config_t cfg2; ho_config_init_default(&cfg2); double o=cfg2.a3_offset_db;
    ho_mro_optimize(&cfg2,0.08,0.01,0.01); assert(cfg2.a3_offset_db>o); P();
}

void test_power(void) {
    T("OLPC"); double p=nr_olpc_power_dbm(-90.0,0.8,100.0,10,23.0); assert(p<=23.0); P();
    T("CLPC"); double f=nr_clpc_accumulate(0.0,2); ASSERT_NEAR(f,1.0); P();
    T("headroom"); double ph=nr_power_headroom_db(23.0,15.0); ASSERT_NEAR(ph,8.0); P();
    T("water-fill"); double g[4]={0.001,0.0005,0.0002,0.0001}, pw[4], s=0;
    int r=dl_waterfill_power(g,4,1.0,0.01,pw); assert(r==0);
    for(int i=0;i<4;i++) s+=pw[i];
    assert(s>0.0); P();
}

void test_capacity(void) {
    T("Erlang B"); double pb=erlang_b_blocking_prob(10,5.0); assert(pb>0.0); P();
    T("Erlang C"); double pw=erlang_c_waiting_prob(10,7.0); assert(pw>0.0); P();
    T("M/D/1"); assert(md1_avg_queue_length(5.0,10.0)>0.0); P();
}

void test_deployment(void) {
    T("coverage plan"); coverage_plan_input_t inp; memset(&inp,0,sizeof(inp));
    inp.target_area_sqkm=100.0; inp.coverage_probability=0.95;
    inp.freq_mhz=2140.0; inp.h_bs_m=30.0; inp.h_ue_m=1.5;
    inp.tx_power_dbm=43.0; inp.tx_gain_dbi=15.0; inp.rx_gain_dbi=0.0;
    inp.noise_figure_db=5.0; inp.bandwidth_hz=20e6; inp.target_sinr_db=-5.0;
    inp.shadow_std_dev_db=8.0; inp.penetration_loss_db=15.0;
    coverage_plan_output_t out=plan_coverage(&inp); assert(out.num_cells>0); P();
}

int main(void) {
    printf("=== mini-cellular-network Test Suite ===\n");
    test_defs(); test_hex(); test_pathloss(); test_sinr();
    test_shannon(); test_cqi(); test_scheduling();
    test_handover(); test_power(); test_capacity(); test_deployment();
    printf("\n=== %d/%d tests passed ===\n", tpass, trun);
    return (tpass==trun) ? 0 : 1;
}
