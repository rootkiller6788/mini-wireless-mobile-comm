/**
 * test_beamforming.c — Assertion-based tests
 * Tests: L1 complex/vector/matrix, L2 steering, L4 grating/MIMO capacity,
 *        L5 SVD/eigen/MUSIC/waterfilling, L6 beam pattern/MU-MIMO
 */
#include "beamforming_types.h"
#include "beamforming_array.h"
#include "beamforming_precoder.h"
#include "beamforming_doa.h"
#include "beamforming_mimo.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

static int ok = 0, bad = 0;

#define T(n) do{printf("  %s: ",n);}while(0)
#define P do{printf("PASS\n");ok++;}while(0)
#define F(m) do{printf("FAIL (%s)\n",m);bad++;}while(0)
#define C(c) do{if(c)P;else F(#c);}while(0)
#define A(a,b,t) do{if(fabs((a)-(b))<(t))P;else{printf("FAIL (%.4f!=%.4f)\n",(double)(a),(double)(b));bad++;}}while(0)

static void test_complex(void) {
    printf("L1: Complex Numbers\n");
    T("add"); complex_double a=make_complex(3,4); complex_double b=make_complex(1,2);
    complex_double s=cadd(a,b); C(s.real==4 && s.imag==6);
    T("mul"); complex_double p=cmul(a,b); C(p.real==-5 && p.imag==10);
    T("conj"); complex_double c=cconj(a); C(c.real==3 && c.imag==-4);
    T("mag"); double m=complex_abs(a); A(m,5.0,1e-10);
    T("expj"); complex_double e=cexpj(M_PI/4); A(e.real,sqrt(2)/2,1e-9); A(e.imag,sqrt(2)/2,1e-9);
}

static void test_vector(void) {
    printf("L1: Vectors\n");
    T("alloc"); complex_vector v=cvec_alloc(10); C(v.length==10);
    T("norm0"); A(cvec_norm(&v),0,1e-10);
    T("dot"); for(size_t i=0;i<v.length;i++)v.data[i]=make_complex(1,0);
    complex_double d=cvec_dot(&v,&v); A(d.real,10.0,1e-10);
    T("norm1"); v.data[0]=make_complex(3,4); cvec_normalize(&v); A(cvec_norm(&v),1.0,1e-10);
    cvec_free(&v);
}

static void test_matrix(void) {
    printf("L3: Matrices\n");
    T("alloc"); complex_matrix A=cmat_alloc(3,3); C(A.rows==3&&A.cols==3);
    T("identity"); cmat_set_identity(&A);
    C(cmat_get(&A,0,0).real==1.0);
    T("matvec"); complex_vector x=cvec_alloc(3),y=cvec_alloc(3);
    for(size_t i=0;i<3;i++)x.data[i]=make_complex(1,0);
    cmat_mul_vec(&A,&x,&y); C(y.data[0].real==1.0);
    T("hermitian"); complex_matrix AH=cmat_alloc(3,3);
    cmat_set(&A,0,1,make_complex(2,3)); cmat_hermitian(&A,&AH);
    C(cmat_get(&AH,1,0).real==2&&cmat_get(&AH,1,0).imag==-3);
    T("frobenius"); A(cmat_frobenius_norm(&A),4.0,1e-9);
    T("is_herm"); C(cmat_is_hermitian(&A,1e-10)==0);
    cvec_free(&x);cvec_free(&y);cmat_free(&A);cmat_free(&AH);
}

static void test_steering(void) {
    printf("L2: Steering Vectors\n");
    T("ULA endfire"); ula_geometry ula=ula_init(3e9,8,0.5);
    complex_vector sv=cvec_alloc(8); steering_direction_1d d;
    d.theta_rad=M_PI/2; d.sin_theta=1.0;
    ula_steering_vector(&ula,d,&sv); A(sv.data[0].real,1.0,1e-9);
    cvec_free(&sv);
}

static void test_grating(void) {
    printf("L4: Grating Lobes\n");
    T("lambda/2 ok"); ula_geometry ula=ula_init(3e9,8,0.5);
    C(ula_check_grating_lobes(&ula,M_PI/2)==1);
    T("lambda bad"); ula_geometry ula2=ula_init(3e9,8,1.0);
    C(ula_check_grating_lobes(&ula2,M_PI/2)==0);
    T("opt spacing"); A(ula_optimal_spacing(0.1),0.05,1e-9);
}

static void test_svd(void) {
    printf("L5: SVD\n");
    T("2x2 identity"); complex_matrix A=cmat_alloc(2,2); cmat_set_identity(&A);
    svd_config cfg={50,1e-10,0}; svd_result svd=svd_result_alloc(2,2);
    C(svd_compute(&A,&svd,&cfg)==0);
    A(svd.sigma[0],1.0,0.02); A(svd.sigma[1],1.0,0.02);
    svd_result_free(&svd);cmat_free(&A);
}

static void test_eigen(void) {
    printf("L5: Eigenvalues\n");
    T("2x2 identity"); complex_matrix A=cmat_alloc(2,2); cmat_set_identity(&A);
    C(cmat_is_hermitian(&A,1e-10)==1);
    eigendecomp_result evd=eigen_alloc(2);
    C(eigen_sym_decomp(&A,&evd,100,1e-10)==0);
    A(evd.eigenvalues[0],1.0,0.02); A(evd.eigenvalues[1],1.0,0.02);
    cmat_free(&A); eigen_free(&evd);
}

static void test_beam(void) {
    printf("L6: Beam Pattern\n");
    T("8-el DAS"); ula_geometry ula=ula_init(3e9,8,0.5);
    complex_vector w=cvec_alloc(8); steering_direction_1d s;
    s.theta_rad=M_PI/2; s.sin_theta=1.0;
    ula_das_weights(&ula,s,&w);
    beam_pattern bp=ula_beam_pattern(&ula,&w,-M_PI/2,M_PI/2,361);
    C(bp.num_angles==361); beam_pattern_free(&bp); cvec_free(&w);
}

static void test_capacity(void) {
    printf("L4: Shannon MIMO Capacity\n");
    T("2x2 @ 10dB"); mimo_channel ch; channel_rayleigh_iid(2,2,&ch);
    channel_normalize(&ch);
    mimo_capacity cap=mimo_capacity_alloc(2);
    C(mimo_capacity_openloop(&ch,10.0,&cap)==0);
    C(cap.capacity_bps_hz>0.0);
    mimo_capacity_free(&cap); channel_free(&ch);
}

static void test_waterfill(void) {
    printf("L5: Waterfilling\n");
    T("2-stream"); double snr[2]={10,1};
    waterfilling_result wf=waterfilling_alloc(2);
    C(waterfilling_compute(snr,2,1.0,&wf)==0);
    C(wf.power_allocation[0]>=wf.power_allocation[1]);
    A(wf.total_power,1.0,0.1);
    waterfilling_free(&wf);
}

static void test_music(void) {
    printf("L5: MUSIC DOA\n");
    T("1src@30deg"); ula_geometry ula=ula_init(3e9,8,0.5); size_t M=ula.num_elements;
    size_t N=100; double src=30.0*M_PI/180.0;
    array_snapshot_buffer buf=snapshot_buffer_alloc(M,N);
    complex_vector sv=cvec_alloc(M); steering_direction_1d d;
    d.theta_rad=src; d.sin_theta=sin(src);
    ula_steering_vector(&ula,d,&sv);
    for(size_t n=0;n<N;n++){
        double ph=2*M_PI*((double)rand())/((double)RAND_MAX);
        complex_double sig=cexpj(ph);
        for(size_t m=0;m<M;m++){
            double nr=0.1*(((double)rand())/((double)RAND_MAX)-0.5);
            double ni=0.1*(((double)rand())/((double)RAND_MAX)-0.5);
            cmat_set(&buf.data,m,n,cadd(cmul(sv.data[m],sig),make_complex(nr,ni)));
        }
    }
    music_config mc=music_config_default(M,N,1); mc.angle_grid_size=181;
    doa_result dr=doa_result_alloc(1,mc.angle_grid_size);
    C(doa_music(&buf,&ula,&mc,&dr)==0); C(dr.num_sources>0);
    doa_result_free(&dr); snapshot_buffer_free(&buf); cvec_free(&sv);
}

static void test_mu_mimo(void) {
    printf("L6: MU-MIMO Sum Rate\n");
    T("MRT 4x4"); mimo_channel ch; channel_rayleigh_iid(4,4,&ch);
    channel_normalize(&ch);
    precoder_result pr=precoder_result_alloc(4,1,4);
    precoder_mrt(&ch.H,&pr);
    double rate=mu_mimo_sum_rate(&ch.H,&pr,1.0); C(rate>0.0);
    precoder_result_free(&pr); channel_free(&ch);
}

int main(void){
    printf("=== Beamforming & Massive MIMO Tests ===\n\n");
    test_complex(); test_vector(); test_matrix(); test_steering();
    test_grating(); test_svd(); test_eigen(); test_beam();
    test_capacity(); test_waterfill(); test_music(); test_mu_mimo();
    printf("\n=== %d passed, %d failed ===\n",ok,bad);
    return bad>0?1:0;
}
