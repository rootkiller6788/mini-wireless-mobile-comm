/**
 * beamforming_array.h ? Antenna Array Geometry & Steering Vectors
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: ula_geometry, upa_geometry, uca_geometry
 *   L2 Core Concepts: Array factor, beam steering, half-power beamwidth,
 *                     grating lobes, element spacing criterion
 *   L3 Mathematical Structures: Spatial frequency, phase progression, DFT relationship
 *   L4 Fundamental Laws: lambda/2 spacing criterion, grating lobe condition
 *   L5 Algorithms: Steering vector generation, beam pattern computation
 *   L6 Canonical Problems: ULA beam scanning, null steering
 *
 * Reference: Balanis (2016) Antenna Theory, Ch.6 (Arrays)
 *            Van Trees (2002) Optimum Array Processing, Ch.2
 *            Johnson & Dudgeon (1993) Array Signal Processing
 *
 * All steering vectors are normalized such that a(theta) has ||a(theta)|| = sqrt(N)
 * to maintain consistency with standard array processing literature.
 */

#ifndef BEAMFORMING_ARRAY_H
#define BEAMFORMING_ARRAY_H

#include "beamforming_types.h"

/* ================================================================
 * L1: Array Geometry Types
 * ================================================================ */

/** Uniform Linear Array (ULA) geometry.
 *  N isotropic elements along z-axis with spacing d.
 *  This is the most basic and most analyzed array geometry.
 *  Maps to: 1D angular scanning, simplest DOA estimation. */
typedef struct {
    size_t num_elements;        /* Number of antenna elements, N     */
    double element_spacing;     /* Inter-element distance d (meters)  */
    double wavelength;          /* Carrier wavelength lambda (meters) */
    double freq_hz;             /* Carrier frequency f_c (Hz)        */
} ula_geometry;

/** Uniform Planar Array (UPA) geometry.
 *  N_y * N_z isotropic elements on yz-plane.
 *  Maps to: 2D angular scanning (azimuth + elevation).
 *  Used in: 3D beamforming, full-dimension MIMO (FD-MIMO). */
typedef struct {
    size_t num_elements_y;      /* Elements along y-axis (horizontal) */
    size_t num_elements_z;      /* Elements along z-axis (vertical)   */
    double element_spacing;     /* Inter-element distance d (meters)  */
    double wavelength;          /* Carrier wavelength lambda (meters) */
    double freq_hz;             /* Carrier frequency f_c (Hz)        */
} upa_geometry;

/** Uniform Circular Array (UCA) geometry.
 *  N isotropic elements on a circle.
 *  Advantage: 360-degree azimuth coverage with constant beamwidth.
 *  Maps to: omni-directional beamforming. */
typedef struct {
    size_t num_elements;        /* Number of antenna elements, N     */
    double radius;              /* Circle radius (meters)             */
    double wavelength;          /* Carrier wavelength lambda (meters) */
    double freq_hz;             /* Carrier frequency f_c (Hz)        */
} uca_geometry;

/** Element location in 3D space (for arbitrary array geometries).
 *  Maps to: arbitrary array manifold calibration. */
typedef struct {
    double x;                   /* x-coordinate (meters) */
    double y;                   /* y-coordinate (meters) */
    double z;                   /* z-coordinate (meters) */
} element_location;

/** Arbitrary array geometry ? user-defined element positions.
 *  Maps to: measured array manifolds, conformal arrays. */
typedef struct {
    element_location *positions; /* Array of element locations      */
    size_t num_elements;
    double wavelength;
    double freq_hz;
} arbitrary_array_geometry;

/* ================================================================
 * L1: Steering Direction Types
 * ================================================================ */

/** Steering direction (azimuth-only, for ULA).
 *  theta: angle from array broadside (radians).
 *  For ULA along z-axis: theta=0 is broadside (perpendicular to array axis).
 *  theta in [-pi/2, pi/2] for unambiguous range. */
typedef struct {
    double theta_rad;           /* Azimuth angle from broadside    */
    double sin_theta;           /* sin(theta) ? spatial frequency   */
} steering_direction_1d;

/** Steering direction (azimuth + elevation, for UPA).
 *  phi: azimuth angle in xy-plane from x-axis (radians).
 *  theta: elevation angle from z-axis (radians).
 *  Maps to: 3D beam steering, FD-MIMO. */
typedef struct {
    double phi_rad;             /* Azimuth (0 to 2*pi)            */
    double theta_rad;           /* Elevation (0 to pi)             */
} steering_direction_2d;

/* ================================================================
 * L1: Received Signal Types
 * ================================================================ */

/** Snapshot ? one sampling instant of array output.
 *  y[n] = sum_k s_k[n] a(theta_k) + noise[n]
 *  Maps to: received signal at time n across M antenna elements. */
typedef struct {
    complex_vector data;        /* Length = M (num elements)       */
    double timestamp;           /* Sample time (seconds)           */
} array_snapshot;

/** Array snapshot buffer ? multiple time samples.
 *  Maps to: data matrix X = [y[0], y[1], ..., y[N_s-1]] for DOA. */
typedef struct {
    complex_matrix data;        /* M x N_s matrix                  */
    size_t num_elements;        /* M                               */
    size_t num_snapshots;       /* N_s                             */
} array_snapshot_buffer;

/* ================================================================
 * L2: Array Steering Vector API
 * ================================================================ */

/** ULA steering vector: a(theta) = exp(-j k d cos(theta) * n)
 *  where k = 2*pi/lambda, n = 0,...,N-1 (element index).
 *  L2 Concept: Phase progression across array elements.
 *  Derived from: plane wave assumption ? delay tau_n = n * d * cos(theta) / c.
 *  Reference: Van Trees (2002) Optimum Array Processing, Eq.2.2.
 *  Complexity: O(N). */
void ula_steering_vector(const ula_geometry *array,
                         steering_direction_1d dir,
                         complex_vector *steering_vec);

/** UPA steering vector: a(phi, theta) ? 2D planar wave.
 *  a(phi,theta)[n_y, n_z] = exp(-j k [n_y d sin(theta)cos(phi) + n_z d cos(theta)])
 *  L2 Concept: 2D spatial frequency decomposition.
 *  Reference: Heath et al. (2016) Foundations of MIMO, ?7.3.
 *  Complexity: O(N_y * N_z). */
void upa_steering_vector(const upa_geometry *array,
                         steering_direction_2d dir,
                         complex_vector *steering_vec);

/** UCA steering vector: a(phi, theta) for circular array.
 *  Phase = k * R * sin(theta) * cos(phi - 2*pi*n/N).
 *  L2 Concept: Circular symmetry in azimuth.
 *  Complexity: O(N). */
void uca_steering_vector(const uca_geometry *array,
                         steering_direction_2d dir,
                         complex_vector *steering_vec);

/* ================================================================
 * L2: Array Factor Computation
 * ================================================================ */

/** ULA array factor: AF(theta) = sum_{n=0}^{N-1} w_n* a_n(theta).
 *  w_n are the complex beamforming weights.
 *  L2 Concept: Linear superposition of element contributions.
 *  The array factor is the spatial equivalent of the FIR filter frequency response.
 *  Reference: Balanis (2016) Antenna Theory, Eq.6-52.
 *  Complexity: O(N). */
complex_double ula_array_factor(const ula_geometry *array,
                                const complex_vector *weights,
                                steering_direction_1d dir);

/** UPA array factor: AF(phi,theta) with 2D weights.
 *  L2 Concept: 2D spatial filtering.
 *  Complexity: O(N_y * N_z). */
complex_double upa_array_factor(const upa_geometry *array,
                                const complex_vector *weights,
                                steering_direction_2d dir);

/* ================================================================
 * L2: Beam Pattern (Radiation Pattern)
 * ================================================================ */

/** Compute full beam pattern for ULA: |AF(theta)|^2 vs theta scan.
 *  scan_range: [theta_min, theta_max] with num_angles points.
 *  steering_angle: main lobe direction for delay-and-sum beamformer.
 *  L2 Concept: Beam pattern = spatial frequency response.
 *  L6 Canonical Problem: Plot beam pattern vs angle.
 *  Returns: filled beam_pattern struct (caller must free internal arrays). */
beam_pattern ula_beam_pattern(const ula_geometry *array,
                              const complex_vector *weights,
                              double theta_min_rad,
                              double theta_max_rad,
                              size_t num_angles);

/** Compute beam pattern for UPA ? 2D scan over (phi, theta) grid.
 *  L6 Canonical Problem: 2D radiation pattern.
 *  Returns: gain_linear as a matrix of size n_phi * n_theta. */
real_matrix upa_beam_pattern_2d(const upa_geometry *array,
                                const complex_vector *weights,
                                size_t n_phi, size_t n_theta);

/** Free beam_pattern internal arrays. */
void beam_pattern_free(beam_pattern *bp);

/* ================================================================
 * L4: Element Spacing Constraints
 * ================================================================ */

/** Check if element spacing avoids grating lobes for given scan range.
 *  L4 Fundamental Law: For scan range [-theta_max, theta_max]:
 *  d/lambda <= 1 / (1 + |sin(theta_max)|)
 *  If d/lambda > 0.5, grating lobes may appear.
 *  Reference: Mailloux (2005) Phased Array Antenna Handbook, ?2.3.
 *  Returns: 1 if grating-lobe-free, 0 if grating lobes present. */
int ula_check_grating_lobes(const ula_geometry *array, double theta_max_rad);

/** Optimal element spacing for given frequency (lambda/2).
 *  L4 Fundamental Law: d = lambda/2 avoids grating lobes for all scan angles.
 *  This is the Nyquist spatial sampling theorem for arrays. */
double ula_optimal_spacing(double wavelength);

/* ================================================================
 * L6: Classical Beamformers
 * ================================================================ */

/** Delay-and-Sum (DAS) beamformer weights: w = a(theta_steer) / N.
 *  L6 Canonical Problem: Basic beam steering.
 *  This is the simplest beamformer ? compensates propagation delays
 *  to coherently add signals from the desired direction.
 *  Reference: Van Trees (2002) Optimum Array Processing, ?6.2.
 *  Complexity: O(N). */
void ula_das_weights(const ula_geometry *array,
                     steering_direction_1d steer_dir,
                     complex_vector *weights);

/** Null-steering weights: places nulls at specified directions.
 *  Uses linear constraints: w^H a(theta_null) = 0 for each null.
 *  L6 Canonical Problem: Interference nulling.
 *  This is the deterministic counterpart to adaptive nulling (MVDR).
 *  Complexity: O(N * K^2) for K nulls.
 *  Reference: Applebaum (1976) Adaptive Arrays. */
int ula_null_steering(const ula_geometry *array,
                      const real_vector *null_directions_rad,
                      steering_direction_1d steer_dir,
                      complex_vector *weights);

/* ================================================================
 * Utility Functions
 * ================================================================ */

/** Initialize ULA geometry with standard parameters.
 *  freq_hz: carrier frequency in Hz.
 *  num_elements: number of antennas.
 *  spacing_factor: d/lambda ratio (0.5 = lambda/2). */
ula_geometry ula_init(double freq_hz, size_t num_elements, double spacing_factor);

/** Initialize UPA geometry. */
upa_geometry upa_init(double freq_hz, size_t n_y, size_t n_z, double spacing_factor);

/** Initialize UCA geometry.
 *  radius_factor: R/lambda ratio. */
uca_geometry uca_init(double freq_hz, size_t num_elements, double radius_factor);

#endif /* BEAMFORMING_ARRAY_H */
