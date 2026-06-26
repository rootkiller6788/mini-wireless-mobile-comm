/**
 * beamforming_array.c - Antenna Array Geometry, Steering Vectors, Beam Patterns
 *
 * L2: Array factor computation, DAS beamformer
 * L3: Spatial frequency, phase progression
 * L4: Grating lobe condition (Nyquist spatial sampling theorem)
 * L6: ULA beam scanning, null steering (canonical beamforming problems)
 *
 * Reference: Balanis (2016) Antenna Theory, Ch.6
 *            Van Trees (2002) Optimum Array Processing, Ch.2-3
 *            Mailloux (2005) Phased Array Antenna Handbook
 */

#include "beamforming_array.h"
#include <stdio.h>
#include <float.h>

/* ============ Array Geometry Initialization ============ */

ula_geometry ula_init(double freq_hz, size_t num_elements, double spacing_factor) {
    ula_geometry a;
    a.num_elements = num_elements;
    a.freq_hz = freq_hz;
    a.wavelength = 3.0e8 / freq_hz;
    a.element_spacing = spacing_factor * a.wavelength;
    return a;
}

upa_geometry upa_init(double freq_hz, size_t n_y, size_t n_z,
                      double spacing_factor) {
    upa_geometry a;
    a.num_elements_y = n_y;
    a.num_elements_z = n_z;
    a.freq_hz = freq_hz;
    a.wavelength = 3.0e8 / freq_hz;
    a.element_spacing = spacing_factor * a.wavelength;
    return a;
}

uca_geometry uca_init(double freq_hz, size_t num_elements,
                      double radius_factor) {
    uca_geometry a;
    a.num_elements = num_elements;
    a.freq_hz = freq_hz;
    a.wavelength = 3.0e8 / freq_hz;
    a.radius = radius_factor * a.wavelength;
    return a;
}

/* ============ Steering Vector Generation (L2) ============ */

void ula_steering_vector(const ula_geometry *array,
                         steering_direction_1d dir,
                         complex_vector *steering_vec) {
    /* ULA along z-axis. Steering for incidence angle theta from broadside:
     *   a_n(theta) = exp(-j * k * n * d * cos(theta)),  n = 0,...,N-1
     * where k = 2*pi/lambda is the wavenumber.
     *
     * Physical interpretation: The phase at element n relative to element 0
     * is -k * (propagation distance difference) = -k * n*d*cos(theta).
     * cos(theta) projects the element spacing along the wave direction.
     *
     * For uniform weights, the array factor is a Dirichlet kernel in
     * spatial frequency domain: AF = sin(N*psi/2)/sin(psi/2). */
    if (!array || !steering_vec) return;
    if (steering_vec->length != array->num_elements) return;

    double k = 2.0 * M_PI / array->wavelength;
    double psi = k * array->element_spacing * cos(dir.theta_rad);

    for (size_t n = 0; n < array->num_elements; n++)
        steering_vec->data[n] = cexpj(-psi * ((double)n));
}

void upa_steering_vector(const upa_geometry *array,
                         steering_direction_2d dir,
                         complex_vector *steering_vec) {
    /* UPA on yz-plane. Elements at (n_y*d, n_z*d).
     * Wave vector k = (2*pi/lambda) * [sin(theta)cos(phi), sin(theta)sin(phi), cos(theta)]^T
     * Element position r = [n_y*d, 0, n_z*d]^T
     * Phase = k.r = k*d * [n_y*sin(theta)*cos(phi) + n_z*cos(theta)]
     *
     * This is the 2D extension of ULA steering ? the phase is the sum of
     * horizontal and vertical phase gradients. */
    if (!array || !steering_vec) return;
    size_t N = array->num_elements_y * array->num_elements_z;
    if (steering_vec->length != N) return;

    double k = 2.0 * M_PI / array->wavelength;
    double psi_y = k * array->element_spacing * sin(dir.theta_rad) * cos(dir.phi_rad);
    double psi_z = k * array->element_spacing * cos(dir.theta_rad);

    for (size_t ny = 0; ny < array->num_elements_y; ny++)
        for (size_t nz = 0; nz < array->num_elements_z; nz++) {
            size_t idx = ny * array->num_elements_z + nz;
            double phase = -psi_y * ((double)ny) - psi_z * ((double)nz);
            steering_vec->data[idx] = cexpj(phase);
        }
}

void uca_steering_vector(const uca_geometry *array,
                         steering_direction_2d dir,
                         complex_vector *steering_vec) {
    /* UCA: N elements uniformly spaced on a circle of radius R.
     * Element n at azimuth angle 2*pi*n/N.
     * Phase = k*R*sin(theta)*cos(phi - 2*pi*n/N)
     *
     * Key property: 360-degree azimuth coverage with uniform beamwidth.
     * The Bessel function J_0 determines the azimuth pattern. */
    if (!array || !steering_vec) return;
    if (steering_vec->length != array->num_elements) return;

    double k = 2.0 * M_PI / array->wavelength;
    double kr = k * array->radius * sin(dir.theta_rad);

    for (size_t n = 0; n < array->num_elements; n++) {
        double phi_n = 2.0 * M_PI * ((double)n) / ((double)array->num_elements);
        double phase = -kr * cos(dir.phi_rad - phi_n);
        steering_vec->data[n] = cexpj(phase);
    }
}

/* ============ Array Factor (L2) ============ */

complex_double ula_array_factor(const ula_geometry *array,
                                const complex_vector *weights,
                                steering_direction_1d dir) {
    /* AF(theta) = w^H a(theta) = sum_{n=0}^{N-1} w_n^* a_n(theta).
     * This is the spatial convolution of weights with the array manifold. */
    complex_vector sv = cvec_alloc(array->num_elements);
    ula_steering_vector(array, dir, &sv);
    complex_double af = cvec_dot(weights, &sv);
    cvec_free(&sv);
    return af;
}

complex_double upa_array_factor(const upa_geometry *array,
                                const complex_vector *weights,
                                steering_direction_2d dir) {
    size_t N = array->num_elements_y * array->num_elements_z;
    complex_vector sv = cvec_alloc(N);
    upa_steering_vector(array, dir, &sv);
    complex_double af = cvec_dot(weights, &sv);
    cvec_free(&sv);
    return af;
}

/* ============ Beam Pattern (L2, L6) ============ */

beam_pattern ula_beam_pattern(const ula_geometry *array,
                              const complex_vector *weights,
                              double theta_min_rad,
                              double theta_max_rad,
                              size_t num_angles) {
    /* Computes |AF(theta)|^2 across angular scan with metrics extraction.
     *
     * Extracted features:
     *   - Main lobe direction (peak of pattern)
     *   - 3dB (half-power) beamwidth via linear interpolation
     *   - Peak sidelobe level (excluding main lobe region)
     *
     * The 3dB beamwidth for a uniform ULA is approximately:
     *   HPBW = 0.886 * lambda/(N*d) [radians] at broadside
     *   = 51 * lambda/(N*d) [degrees]
     * This is the Rayleigh resolution limit.
     *
     * Reference: Balanis (2016), Sec.6.4.3. */
    beam_pattern bp;
    bp.num_angles = num_angles;
    bp.angles_rad = (double*)malloc(num_angles * sizeof(double));
    bp.gain_linear = (double*)malloc(num_angles * sizeof(double));
    bp.gain_db = (double*)malloc(num_angles * sizeof(double));
    bp.half_power_bw = 0.0;
    bp.max_sidelobe_db = -1000.0;

    double max_gain = -1.0;
    size_t max_idx = 0;
    double dth = (theta_max_rad - theta_min_rad) / ((double)(num_angles - 1));

    for (size_t i = 0; i < num_angles; i++) {
        double theta = theta_min_rad + dth * ((double)i);
        bp.angles_rad[i] = theta;

        steering_direction_1d dir;
        dir.theta_rad = theta;
        dir.sin_theta = sin(theta);

        complex_double af = ula_array_factor(array, weights, dir);
        double gain = complex_abs(af) * complex_abs(af);
        bp.gain_linear[i] = gain;
        bp.gain_db[i] = (gain > 1e-300) ? 10.0 * log10(gain) : -300.0;

        if (gain > max_gain) { max_gain = gain; max_idx = i; }
    }

    bp.steering_angle = bp.angles_rad[max_idx];

    /* 3dB beamwidth via linear interpolation */
    double half_gain = max_gain * 0.5;
    double theta_low = bp.angles_rad[0], theta_high = bp.angles_rad[num_angles - 1];

    for (size_t i = max_idx; i > 0; i--) {
        if (bp.gain_linear[i] < half_gain) {
            double f0 = bp.gain_linear[i], f1 = bp.gain_linear[i + 1];
            double t0 = bp.angles_rad[i], t1 = bp.angles_rad[i + 1];
            if (fabs(f1 - f0) > 1e-300)
                theta_low = t0 + (half_gain - f0) * (t1 - t0) / (f1 - f0);
            break;
        }
    }
    for (size_t i = max_idx; i < num_angles - 1; i++) {
        if (bp.gain_linear[i] < half_gain) {
            size_t ii = (i > 0) ? i - 1 : 0;
            double f0 = bp.gain_linear[ii], f1 = bp.gain_linear[i];
            double t0 = bp.angles_rad[ii], t1 = bp.angles_rad[i];
            if (fabs(f1 - f0) > 1e-300)
                theta_high = t0 + (half_gain - f0) * (t1 - t0) / (f1 - f0);
            break;
        }
    }
    bp.half_power_bw = fabs(theta_high - theta_low);

    /* Peak sidelobe (outside main lobe region) */
    for (size_t i = 0; i < num_angles; i++) {
        double dist = fabs(bp.angles_rad[i] - bp.steering_angle);
        if (dist < 1.5 * bp.half_power_bw && i != max_idx) continue;
        if (bp.gain_db[i] > bp.max_sidelobe_db)
            bp.max_sidelobe_db = bp.gain_db[i];
    }

    /* Normalize to peak = 0 dB */
    if (max_gain > 1e-300) {
        double offset = 10.0 * log10(max_gain);
        for (size_t i = 0; i < num_angles; i++) bp.gain_db[i] -= offset;
        bp.max_sidelobe_db -= offset;
    }

    return bp;
}

real_matrix upa_beam_pattern_2d(const upa_geometry *array,
                                const complex_vector *weights,
                                size_t n_phi, size_t n_theta) {
    /* 2D beam pattern over azimuth x elevation grid.
     * Returns real_matrix of size n_phi x n_theta containing gains. */
    real_matrix bp_2d = rmat_alloc(n_phi, n_theta);
    if (!bp_2d.data) return bp_2d;

    double dphi = 2.0 * M_PI / ((double)n_phi);
    double dth = M_PI / ((double)n_theta);

    for (size_t ip = 0; ip < n_phi; ip++) {
        for (size_t it = 0; it < n_theta; it++) {
            steering_direction_2d dir;
            dir.phi_rad = dphi * ((double)ip);
            dir.theta_rad = dth * ((double)it);
            complex_double af = upa_array_factor(array, weights, dir);
            bp_2d.data[ip * n_theta + it] = complex_abs(af) * complex_abs(af);
        }
    }
    return bp_2d;
}

void beam_pattern_free(beam_pattern *bp) {
    if (!bp) return;
    if (bp->angles_rad) { free(bp->angles_rad); bp->angles_rad = NULL; }
    if (bp->gain_linear) { free(bp->gain_linear); bp->gain_linear = NULL; }
    if (bp->gain_db) { free(bp->gain_db); bp->gain_db = NULL; }
    bp->num_angles = 0;
}

/* ============ Element Spacing Constraints (L4) ============ */

int ula_check_grating_lobes(const ula_geometry *array, double theta_max_rad) {
    /* Grating lobe condition (Nyquist spatial sampling):
     *   d/lambda < 1 / (1 + |sin(theta_max)|)
     *
     * For broadside-only (theta_max=0): d < lambda (any spacing ok)
     * For full scan (theta_max=pi/2): d < lambda/2
     * For scan to 60 degrees: d < 0.535*lambda
     *
     * Physical origin: When d > 0.5*lambda, the spatial frequency
     * exceeds the Nyquist rate, causing aliasing (grating lobes)
     * in the visible region |sin(theta)| <= 1.
     *
     * This is the spatial equivalent of the Nyquist-Shannon sampling
     * theorem for time-domain signals. */
    if (!array) return 0;
    double ratio = array->element_spacing / array->wavelength;
    double limit = 1.0 / (1.0 + fabs(sin(theta_max_rad)));
    return (ratio <= limit) ? 1 : 0;
}

double ula_optimal_spacing(double wavelength) {
    return wavelength * 0.5;
}

/* ============ DAS Beamformer (L6) ============ */

void ula_das_weights(const ula_geometry *array,
                     steering_direction_1d steer_dir,
                     complex_vector *weights) {
    /* Delay-and-Sum: w = a(theta_steer)* / N
     * where a*(theta) is the conjugate steering vector.
     *
     * The DAS beamformer compensates the propagation delays so that
     * signals from theta_steer arrive in-phase at the output:
     *   y = w^H x = (1/N) sum_n a_n(theta) x_n
     *
     * At the steering direction: w^H a(theta_steer) = 1 (unit gain).
     * Array gain = N (coherent combining of N independent elements).
     *
     * Beam pattern: sin(N*psi/2)/(N*sin(psi/2)) where psi = kd(cos(theta)-cos(theta_s)). */
    if (!array || !weights) return;
    if (weights->length != array->num_elements) return;

    steering_direction_1d sv_dir;
    sv_dir.theta_rad = steer_dir.theta_rad;
    sv_dir.sin_theta = steer_dir.sin_theta;

    ula_steering_vector(array, sv_dir, weights);
    for (size_t n = 0; n < array->num_elements; n++)
        weights->data[n] = cmul(cconj(weights->data[n]),
            make_complex(1.0 / ((double)array->num_elements), 0.0));
}

int ula_null_steering(const ula_geometry *array,
                      const real_vector *null_directions_rad,
                      steering_direction_1d steer_dir,
                      complex_vector *weights) {
    /* Null-steering via Gram-Schmidt orthogonalization.
     *
     * Places nulls at specified directions while maintaining gain in
     * the steering direction. For K nulls with K < N-1:
     *
     * 1. Start with DAS weights toward steer_dir.
     * 2. For each null direction k:
     *    - Project current weights onto steering vector for null_k
     *    - Subtract projection (orthogonalize)
     * 3. Re-normalize to restore unit gain in steering direction.
     *
     * This is equivalent to the linearly constrained minimum power
     * beamformer when the noise is spatially white.
     *
     * Complexity: O(N*K) for K nulls and N elements. */
    if (!array || !null_directions_rad || !weights) return -1;
    if (weights->length != array->num_elements) return -2;

    size_t N = array->num_elements, K = null_directions_rad->length;
    ula_das_weights(array, steer_dir, weights);
    if (K == 0) return 0;

    for (size_t k = 0; k < K; k++) {
        complex_vector a_null = cvec_alloc(N);
        steering_direction_1d nd;
        nd.theta_rad = null_directions_rad->data[k];
        nd.sin_theta = sin(nd.theta_rad);
        ula_steering_vector(array, nd, &a_null);

        /* Orthogonalize: w -= (a^H w)/(a^H a) * a = (inner/||a||^2) * a */
        complex_double inner = cvec_dot(&a_null, weights);
        double norm_sq = cvec_norm(&a_null);
        if (norm_sq > 1e-300) {
            complex_double coeff = cmul(inner, make_complex(1.0 / norm_sq, 0.0));
            for (size_t n = 0; n < N; n++)
                weights->data[n] = csub(
                    weights->data[n], cmul(coeff, a_null.data[n]));
        }
        cvec_free(&a_null);
    }

    /* Restore unit gain constraint: w^H a(theta_steer) = 1 */
    complex_vector a_steer = cvec_alloc(N);
    ula_steering_vector(array, steer_dir, &a_steer);
    complex_double gain = cvec_dot(weights, &a_steer);
    double gm = complex_abs(gain);
    if (gm > 1e-300)
        cvec_scale(weights, make_complex(1.0 / gm, 0.0));
    cvec_free(&a_steer);

    return 0;
}

/* ============ Snapshot Buffer Utils ============ */

array_snapshot_buffer snapshot_buffer_alloc(size_t M, size_t N_snapshots) {
    array_snapshot_buffer buf;
    buf.num_elements = M;
    buf.num_snapshots = N_snapshots;
    buf.data = cmat_alloc(M, N_snapshots);
    return buf;
}

void snapshot_buffer_free(array_snapshot_buffer *buf) {
    if (buf) cmat_free(&buf->data);
}
