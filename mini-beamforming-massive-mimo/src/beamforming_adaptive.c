#include "beamforming_adaptive.h"
#include "beamforming_array.h"
#include "beamforming_types.h"
#include "beamforming_doa.h"
#include <stdio.h>
#include <float.h>
void lms_init(lms_beamformer *bmf, size_t M, double mu, double leakage, const complex_vector *iw) {
    if (!bmf) return;
    bmf->weights = cvec_alloc(M);
    bmf->mu = mu; bmf->leakage = leakage; bmf->num_elements = M; bmf->iteration = 0;
    bmf->e_history = 0.0; bmf->mse_estimate = 0.0;
    if (iw) cvec_copy(iw, &bmf->weights);
    else if (M > 0) bmf->weights.data[0] = make_complex(1.0, 0.0);
}

complex_double lms_update(lms_beamformer *bmf, const complex_vector *x, complex_double d) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    complex_double e = csub(d, y);
    double lf = 1.0 - bmf->mu * bmf->leakage;
    for (size_t i = 0; i < bmf->num_elements; i++) {
        complex_double gt = cmul(make_complex(bmf->mu, 0.0), cmul(cconj(e), x->data[i]));
        bmf->weights.data[i] = cadd(cmul(bmf->weights.data[i], make_complex(lf, 0.0)), gt);
    }
    bmf->e_history = complex_abs(e);
    bmf->mse_estimate = 0.95 * bmf->mse_estimate + 0.05 * bmf->e_history * bmf->e_history;
    bmf->iteration++; return y;
}

complex_double lms_momentum_update(lms_beamformer *bmf, const complex_vector *x, complex_double d, double momentum) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    complex_double e = csub(d, y);
    double em = bmf->mu * (1.0 + momentum);
    for (size_t i = 0; i < bmf->num_elements; i++) {
        complex_double g = cmul(make_complex(em, 0.0), cmul(cconj(e), x->data[i]));
        bmf->weights.data[i] = cadd(bmf->weights.data[i], g);
    }
    bmf->e_history = complex_abs(e); bmf->iteration++;
    return y;
}
void lms_free(lms_beamformer *bmf) { if (bmf) cvec_free(&bmf->weights); }
void nlms_init(nlms_beamformer *bmf, size_t M, double mu_0, double eps, const complex_vector *iw) {
    if (!bmf) return;
    bmf->weights = cvec_alloc(M); bmf->mu_0 = mu_0; bmf->epsilon = eps;
    bmf->num_elements = M; bmf->iteration = 0;
    if (iw) cvec_copy(iw, &bmf->weights);
    else if (M > 0) bmf->weights.data[0] = make_complex(1.0, 0.0);
}
complex_double nlms_update(nlms_beamformer *bmf, const complex_vector *x, complex_double d) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    complex_double e = csub(d, y);
    double nsq = cvec_norm(x); nsq *= nsq;
    double mn = bmf->mu_0 / (bmf->epsilon + nsq);
    for (size_t i = 0; i < bmf->num_elements; i++) {
        complex_double g = cmul(make_complex(mn, 0.0), cmul(cconj(e), x->data[i]));
        bmf->weights.data[i] = cadd(bmf->weights.data[i], g);
    }
    bmf->iteration++; return y;
}
void nlms_free(nlms_beamformer *bmf) { if (bmf) cvec_free(&bmf->weights); }
void rls_init(rls_beamformer *bmf, size_t M, double lambda, double delta, const complex_vector *iw) {
    if (!bmf) return;
    bmf->weights = cvec_alloc(M); bmf->P = cmat_alloc(M, M);
    bmf->lambda = lambda; bmf->delta = delta; bmf->num_elements = M; bmf->iteration = 0;
    bmf->gain_vector_norm = NULL;
    for (size_t i = 0; i < M; i++) bmf->P.data[i * M + i] = make_complex(1.0 / delta, 0.0);
    if (iw) cvec_copy(iw, &bmf->weights);
    else if (M > 0) bmf->weights.data[0] = make_complex(1.0, 0.0);
}
complex_double rls_update(rls_beamformer *bmf, const complex_vector *x, complex_double d) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    complex_double e = csub(d, y);
    size_t M = bmf->num_elements;
    complex_vector Px = cvec_alloc(M); cmat_mul_vec(&bmf->P, x, &Px);
    complex_double xHPx = cvec_dot(x, &Px);
    double dn = bmf->lambda + xHPx.real; if (dn < 1e-10) dn = 1e-10;
    complex_vector k = cvec_alloc(M);
    for (size_t i = 0; i < M; i++) k.data[i] = cmul(Px.data[i], make_complex(1.0 / dn, 0.0));
    for (size_t i = 0; i < M; i++)
        for (size_t j = 0; j < M; j++) {
            complex_double up = cmul(k.data[i], cconj(Px.data[j]));
            bmf->P.data[i * M + j] = cmul(csub(bmf->P.data[i * M + j], up), make_complex(1.0 / bmf->lambda, 0.0));
        }
    for (size_t i = 0; i < M; i++)
        bmf->weights.data[i] = cadd(bmf->weights.data[i], cmul(k.data[i], cconj(e)));
    bmf->iteration++; cvec_free(&Px); cvec_free(&k); return y;
}
complex_double rls_qr_update(rls_beamformer *bmf, const complex_vector *x, complex_double d) {
    return rls_update(bmf, x, d);
}
void rls_free(rls_beamformer *bmf) {
    if (bmf) { cvec_free(&bmf->weights); cmat_free(&bmf->P); }
}

void cma_init(cma_beamformer *bmf, size_t M, double mu, double R2, const complex_vector *iw) {
    if (!bmf) return;
    bmf->weights = cvec_alloc(M);
    bmf->mu = mu; bmf->R2 = R2; bmf->num_elements = M; bmf->iteration = 0;
    bmf->modulus_error = 0.0;
    if (iw) cvec_copy(iw, &bmf->weights);
    else if (M > 0) bmf->weights.data[0] = make_complex(1.0, 0.0);
}
complex_double cma_update(cma_beamformer *bmf, const complex_vector *x) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    double ym = complex_abs(y); bmf->modulus_error = ym * ym - bmf->R2;
    complex_double err = cmul(y, make_complex(bmf->modulus_error, 0.0));
    for (size_t i = 0; i < bmf->num_elements; i++) {
        complex_double g = cmul(make_complex(bmf->mu, 0.0), cmul(cconj(err), x->data[i]));
        bmf->weights.data[i] = csub(bmf->weights.data[i], g);
    }
    bmf->iteration++; return y;
}
complex_double mma_update(cma_beamformer *bmf, const complex_vector *x) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    double Rr = bmf->R2 / 2.0, Ri = bmf->R2 / 2.0;
    complex_double err = make_complex(y.real * (y.real * y.real - Rr),
                                      y.imag * (y.imag * y.imag - Ri));
    for (size_t i = 0; i < bmf->num_elements; i++) {
        complex_double g = cmul(make_complex(bmf->mu, 0.0), cmul(cconj(err), x->data[i]));
        bmf->weights.data[i] = csub(bmf->weights.data[i], g);
    }
    bmf->iteration++; return y;
}
void cma_free(cma_beamformer *bmf) { if (bmf) cvec_free(&bmf->weights); }
int smi_compute_weights(const array_snapshot_buffer *snap, const ula_geometry *array,
                        steering_direction_1d sd, smi_beamformer *bmf) {
    if (!snap || !array || !bmf) return -1;
    size_t M = snap->num_elements; bmf->num_elements = M; bmf->weights = cvec_alloc(M);
    complex_matrix R = cmat_alloc(M, M); estimate_covariance(snap, &R);
    complex_matrix Ri = cmat_alloc(M, M); svd_pinv(&R, &Ri, 1e-10);
    complex_vector a = cvec_alloc(M); ula_steering_vector(array, sd, &a);
    cmat_mul_vec(&Ri, &a, &bmf->weights);
    complex_double aHRa = cvec_dot(&a, &bmf->weights);
    if (complex_abs(aHRa) > 1e-300)
        cvec_scale(&bmf->weights, make_complex(1.0 / complex_abs(aHRa), 0.0));
    bmf->converged = (snap->num_snapshots >= 2 * M) ? 1.0 : 0.0;
    bmf->snapshots_used = (unsigned int)snap->num_snapshots;
    cmat_free(&R); cmat_free(&Ri); cvec_free(&a); return 0;
}
int loaded_smi_compute(const array_snapshot_buffer *snap, const ula_geometry *array,
                       steering_direction_1d sd, double gamma, smi_beamformer *bmf) {
    if (!snap || !array || !bmf) return -1;
    size_t M = snap->num_elements; bmf->num_elements = M; bmf->weights = cvec_alloc(M);
    complex_matrix R = cmat_alloc(M, M); estimate_covariance(snap, &R);
    for (size_t i = 0; i < M; i++) {
        complex_double v = cmat_get(&R, i, i);
        cmat_set(&R, i, i, cadd(v, make_complex(gamma, 0.0)));
    }
    complex_matrix Ri = cmat_alloc(M, M); svd_pinv(&R, &Ri, 1e-10);
    complex_vector a = cvec_alloc(M); ula_steering_vector(array, sd, &a);
    cmat_mul_vec(&Ri, &a, &bmf->weights);
    complex_double aHRa = cvec_dot(&a, &bmf->weights);
    if (complex_abs(aHRa) > 1e-300)
        cvec_scale(&bmf->weights, make_complex(1.0 / complex_abs(aHRa), 0.0));
    bmf->converged = 1.0; bmf->snapshots_used = (unsigned int)snap->num_snapshots;
    cmat_free(&R); cmat_free(&Ri); cvec_free(&a); return 0;
}
void smi_free(smi_beamformer *bmf) { if (bmf) cvec_free(&bmf->weights); }

void kalman_init(kalman_beamformer *bmf, size_t M, double q, double r, const complex_vector *iw) {
    if (!bmf) return;
    bmf->weights = cvec_alloc(M);
    bmf->P = cmat_alloc(M, M); bmf->Q = cmat_alloc(M, M);
    bmf->R = r; bmf->num_elements = M; bmf->iteration = 0;
    for (size_t i = 0; i < M; i++) {
        bmf->P.data[i * M + i] = make_complex(1.0, 0.0);
        bmf->Q.data[i * M + i] = make_complex(q, 0.0);
    }
    if (iw) cvec_copy(iw, &bmf->weights);
    else if (M > 0) bmf->weights.data[0] = make_complex(1.0, 0.0);
}
complex_double kalman_update(kalman_beamformer *bmf, const complex_vector *x, complex_double d) {
    if (!bmf || !x) return make_complex(0.0, 0.0);
    complex_double y = cvec_dot(&bmf->weights, x);
    complex_double e = csub(d, y);
    size_t M = bmf->num_elements;
    for (size_t i = 0; i < M * M; i++) bmf->P.data[i] = cadd(bmf->P.data[i], bmf->Q.data[i]);
    complex_vector Px = cvec_alloc(M); cmat_mul_vec(&bmf->P, x, &Px);
    complex_double xHPx = cvec_dot(x, &Px);
    double dn = bmf->R + xHPx.real; if (dn < 1e-10) dn = 1e-10;
    complex_vector k = cvec_alloc(M);
    for (size_t i = 0; i < M; i++) k.data[i] = cmul(Px.data[i], make_complex(1.0 / dn, 0.0));
    for (size_t i = 0; i < M; i++)
        for (size_t j = 0; j < M; j++) {
            complex_double kx = cmul(k.data[i], cconj(x->data[j]));
            for (size_t l = 0; l < M; l++)
                bmf->P.data[i * M + j] = csub(bmf->P.data[i * M + j], cmul(kx, bmf->P.data[l * M + j]));
        }
    for (size_t i = 0; i < M; i++)
        bmf->weights.data[i] = cadd(bmf->weights.data[i], cmul(k.data[i], cconj(e)));
    bmf->iteration++; cvec_free(&Px); cvec_free(&k); return y;
}
void kalman_free(kalman_beamformer *bmf) {
    if (bmf) { cvec_free(&bmf->weights); cmat_free(&bmf->P); cmat_free(&bmf->Q); }
}
double compute_adaptive_sinr(const complex_vector *w, const ula_geometry *a,
                             double st, double ia, double inr, double snr) {
    if (!w || !a) return 0.0;
    steering_direction_1d sd, id;
    sd.theta_rad = st; sd.sin_theta = sin(st);
    id.theta_rad = ia; id.sin_theta = sin(ia);
    complex_vector as = cvec_alloc(a->num_elements);
    complex_vector ai = cvec_alloc(a->num_elements);
    ula_steering_vector(a, sd, &as); ula_steering_vector(a, id, &ai);
    complex_double sg = cvec_dot(w, &as), ig = cvec_dot(w, &ai);
    double sp = complex_abs(sg) * complex_abs(sg) * snr;
    double ip = complex_abs(ig) * complex_abs(ig) * inr;
    double np = cvec_norm(w) * cvec_norm(w);
    cvec_free(&as); cvec_free(&ai);
    double den = ip + np;
    return (den > 1e-300) ? (sp / den) : 0.0;
}
real_vector compute_learning_curve(lms_beamformer *bmf, const array_snapshot_buffer *data,
                                   const complex_vector *dseq, size_t n_iter) {
    real_vector curve = rvec_alloc(n_iter);
    for (size_t n = 0; n < n_iter && n < data->num_snapshots; n++) {
        complex_vector snap = cvec_view(&data->data.data[n * data->num_elements], data->num_elements);
        complex_double d = dseq ? dseq->data[n] : make_complex(1.0, 0.0);
        lms_update(bmf, &snap, d);
        curve.data[n] = bmf->mse_estimate;
    }
    return curve;
}
double estimate_misadjustment_lms(double mu, const complex_matrix *R_xx) {
    if (!R_xx) return 0.0;
    double tr = 0.0;
    for (size_t i = 0; i < R_xx->rows; i++) tr += R_xx->data[i * R_xx->cols + i].real;
    return 0.5 * mu * tr;
}
double lms_convergence_time_constant(double mu, double lambda_min) {
    if (mu <= 0.0 || lambda_min <= 0.0) return 1e10;
    return 1.0 / (4.0 * mu * lambda_min);
}
