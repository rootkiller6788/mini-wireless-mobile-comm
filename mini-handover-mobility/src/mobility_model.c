/**
 * @file mobility_model.c
 * @brief Mobility model implementations (L3 Mathematical Structures)
 *
 * Each mobility model represents an independent mathematical structure
 * for UE movement patterns. These models drive handover events in simulation
 * and are essential for handover performance evaluation.
 *
 * References:
 *   - Camp, Boleng, Davies (2002), "A Survey of Mobility Models"
 *   - Bettstetter et al. (MobiHoc 2001), "Smooth is Better than Sharp"
 *   - 3GPP TR 38.901 §7
 */

#include "mobility_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --------------------------------------------------------------------------
 * Internal helper: uniform random in [0, 1)
 * Uses a simple LCG for reproducibility. In production, use a proper PRNG.
 * -------------------------------------------------------------------------- */
static unsigned int _mob_rand_seed = 123456789u;

static double _mob_uniform(void) {
    /* Linear Congruential Generator: X_{n+1} = (a*X_n + c) mod m
     * Parameters from Numerical Recipes: a=1664525, c=1013904223, m=2^32 */
    _mob_rand_seed = 1664525u * _mob_rand_seed + 1013904223u;
    return (double)(_mob_rand_seed & 0x7FFFFFFF) / 2147483648.0;
}

static double _mob_gaussian(double mean, double std) {
    /* Box-Muller transform */
    double u1 = _mob_uniform();
    double u2 = _mob_uniform();
    if (u1 < 1e-12) u1 = 1e-12;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + std * z;
}

static void _mob_reflect_boundary(UEPosition *pos,
                                  double xmin, double xmax,
                                  double ymin, double ymax) {
    if (pos->position_x < xmin) {
        pos->position_x = 2.0 * xmin - pos->position_x;
        pos->velocity_x = fabs(pos->velocity_x);
    }
    if (pos->position_x > xmax) {
        pos->position_x = 2.0 * xmax - pos->position_x;
        pos->velocity_x = -fabs(pos->velocity_x);
    }
    if (pos->position_y < ymin) {
        pos->position_y = 2.0 * ymin - pos->position_y;
        pos->velocity_y = fabs(pos->velocity_y);
    }
    if (pos->position_y > ymax) {
        pos->position_y = 2.0 * ymax - pos->position_y;
        pos->velocity_y = -fabs(pos->velocity_y);
    }
}

/* ============================================================================
 * L3: Random Walk Mobility Model
 *
 * The Random Walk model is derived from Brownian motion — the continuous-time
 * limit of a random walk with infinitesimally small steps. It is the
 * fundamental diffusion model in physics and probability theory.
 *
 * Diffusion equation (Fokker-Planck for Random Walk):
 *   ∂p(x,t)/∂t = D · ∇²p(x,t)
 *
 * where D = v²/(2γ) is the diffusion coefficient, v is speed, γ is the
 * turning rate. The solution is a Gaussian with MSD ∝ t.
 *
 * Implementation notes:
 *   At each step, perturb heading by random angle drawn from wrapped Gaussian.
 *   Adjust speed by random Gaussian perturbation.
 *   Apply reflecting boundary conditions.
 */
void mob_random_walk_step(UEPosition *pos,
                          double      dt_seconds,
                          double      mean_speed_mps,
                          double      speed_std_mps,
                          double      turn_rate_rads,
                          double      boundary_x_min,
                          double      boundary_x_max,
                          double      boundary_y_min,
                          double      boundary_y_max)
{
    if (!pos || dt_seconds <= 0.0) return;

    /* Perturb heading by Gaussian random turn */
    double heading_change = _mob_gaussian(0.0, turn_rate_rads * sqrt(dt_seconds));
    pos->heading_rad += heading_change;

    /* Normalize heading to [-π, π] */
    while (pos->heading_rad > M_PI)  pos->heading_rad -= 2.0 * M_PI;
    while (pos->heading_rad < -M_PI) pos->heading_rad += 2.0 * M_PI;

    /* Perturb speed */
    double speed = _mob_gaussian(mean_speed_mps, speed_std_mps);
    if (speed < 0.0) speed = fabs(speed);

    /* Update velocity components */
    pos->velocity_x = speed * cos(pos->heading_rad);
    pos->velocity_y = speed * sin(pos->heading_rad);
    pos->speed_mps = speed;

    /* Update position */
    pos->position_x += pos->velocity_x * dt_seconds;
    pos->position_y += pos->velocity_y * dt_seconds;

    /* Reflecting boundary */
    _mob_reflect_boundary(pos, boundary_x_min, boundary_x_max,
                          boundary_y_min, boundary_y_max);
}

/* ============================================================================
 * L3: Random Waypoint Mobility Model
 *
 * The Random Waypoint (RWP) model (Johnson & Maltz, 1996) is the de facto
 * standard mobility model in MANET research. Its key features are:
 *   - Destination selection: uniform random in [0, W] × [0, H]
 *   - Constant speed: uniform in [v_min, v_max]
 *   - Pause time: random duration at each destination
 *
 * Known issue: The spatial node distribution is non-uniform — nodes tend
 * to cluster near the center of the simulation area. This is because
 * longer trips through the center are more likely than trips near edges.
 *
 * Remediation: The "Random Direction" variant (Royer et al., 2001) produces
 * a uniform spatial distribution by moving nodes to the boundary before
 * changing direction.
 */
void mob_random_waypoint_step(UEPosition *pos,
                              double      dt_seconds,
                              double      min_speed_mps,
                              double      max_speed_mps,
                              double      pause_time_ms,
                              double      area_width_m,
                              double      area_height_m,
                              double     *dest_x,
                              double     *dest_y,
                              double     *pause_remaining_ms)
{
    if (!pos || !dest_x || !dest_y || !pause_remaining_ms) return;

    /* If currently pausing, decrement pause timer */
    if (*pause_remaining_ms > 0.0) {
        *pause_remaining_ms -= dt_seconds * 1000.0;
        pos->speed_mps = 0.0;
        pos->velocity_x = 0.0;
        pos->velocity_y = 0.0;
        return;
    }

    /* Check if destination is reached (within speed * dt) */
    double dx = *dest_x - pos->position_x;
    double dy = *dest_y - pos->position_y;
    double dist_to_dest = sqrt(dx * dx + dy * dy);

    double speed = pos->speed_mps;
    if (speed < 1e-6) {
        /* Pick a new speed and destination */
        speed = min_speed_mps + _mob_uniform() * (max_speed_mps - min_speed_mps);
        pos->speed_mps = speed;
        *dest_x = _mob_uniform() * area_width_m;
        *dest_y = _mob_uniform() * area_height_m;
        dx = *dest_x - pos->position_x;
        dy = *dest_y - pos->position_y;
        dist_to_dest = sqrt(dx * dx + dy * dy);
        pos->heading_rad = atan2(dy, dx);
    }

    double step_distance = speed * dt_seconds;

    if (step_distance >= dist_to_dest) {
        /* Arrived at destination */
        pos->position_x = *dest_x;
        pos->position_y = *dest_y;
        pos->velocity_x = 0.0;
        pos->velocity_y = 0.0;
        /* Begin pause */
        *pause_remaining_ms = pause_time_ms;
    } else {
        /* Move toward destination */
        pos->heading_rad = atan2(dy, dx);
        pos->velocity_x = speed * cos(pos->heading_rad);
        pos->velocity_y = speed * sin(pos->heading_rad);
        pos->position_x += pos->velocity_x * dt_seconds;
        pos->position_y += pos->velocity_y * dt_seconds;
    }
}

/* ============================================================================
 * L3: Gauss-Markov Mobility Model
 *
 * The Gauss-Markov model introduces temporal dependency through an
 * autoregressive process of order 1 (AR(1)):
 *
 *   s_t = α·s_{t-1} + (1-α)·μ + √(1-α²)·σ·w_t
 *
 * where w_t ~ N(0,1) and α ∈ [0,1] controls memory strength:
 *
 *   α → 0: no memory, pure random (Gaussian) changes
 *   α → 1: perfect memory, deterministic trajectory
 *
 * The name derives from the fact that {s_t} is a Gauss-Markov process
 * (the conditional distribution of s_t given s_{t-1} depends only on
 * s_{t-1}, not on earlier values).
 *
 * Stationary distribution (as t → ∞): N(μ, σ²).
 * Autocorrelation: ρ(k) = α^k (exponential decay).
 */
void mob_gauss_markov_step(UEPosition *pos,
                           double      dt_seconds,
                           double      alpha,
                           double      mean_speed_mps,
                           double      speed_std_mps,
                           double      mean_direction_rad,
                           double      direction_std_rad,
                           double      boundary_x_min,
                           double      boundary_x_max,
                           double      boundary_y_min,
                           double      boundary_y_max)
{
    if (!pos || dt_seconds <= 0.0) return;

    /* Clamp alpha to valid range */
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    double alpha_complement = 1.0 - alpha;
    double noise_scale = sqrt(1.0 - alpha * alpha);

    /* Update speed: AR(1) process with reflecting at zero */
    double noise_speed = _mob_gaussian(0.0, speed_std_mps);
    double new_speed = alpha * pos->speed_mps + alpha_complement * mean_speed_mps
                     + noise_scale * noise_speed;
    if (new_speed < 0.0) new_speed = fabs(new_speed);
    pos->speed_mps = new_speed;

    /* Update direction: AR(1) with wrapping at ±π */
    double noise_dir = _mob_gaussian(0.0, direction_std_rad);
    double new_heading = alpha * pos->heading_rad
                       + alpha_complement * mean_direction_rad
                       + noise_scale * noise_dir;
    while (new_heading > M_PI)  new_heading -= 2.0 * M_PI;
    while (new_heading < -M_PI) new_heading += 2.0 * M_PI;
    pos->heading_rad = new_heading;

    /* Update velocity and position */
    pos->velocity_x = pos->speed_mps * cos(pos->heading_rad);
    pos->velocity_y = pos->speed_mps * sin(pos->heading_rad);
    pos->position_x += pos->velocity_x * dt_seconds;
    pos->position_y += pos->velocity_y * dt_seconds;

    /* Boundary reflection */
    _mob_reflect_boundary(pos, boundary_x_min, boundary_x_max,
                          boundary_y_min, boundary_y_max);
}

/* ============================================================================
 * L3: Levy Walk Mobility Model
 *
 * Levy flights are random walks whose step lengths follow a heavy-tailed
 * (power-law) distribution:
 *
 *   P(l) = (α-1)·l_min^{α-1} · l^{-α},  for l ≥ l_min, α > 1
 *
 * This generates "superdiffusive" behavior: MSD(t) ∝ t^{2/(α-1)} for
 * 2 < α < 3. In contrast, Brownian motion has MSD ∝ t (diffusive).
 *
 * Empirical evidence for Levy walks in human mobility:
 *   - Gonzalez et al. (Nature 2008): human trajectories show heavy tails
 *     with α ≈ 1.75 ± 0.15 for step lengths
 *   - Rhee et al. (INFOCOM 2008): human walk patterns at campus scale
 *     follow truncated Levy walk with α ≈ 1.5
 *
 * Generation method: inverse CDF sampling
 *   l = l_min · u^{-1/(α-1)}  where u ~ Uniform(0,1)
 */
void mob_levy_walk_step(UEPosition *pos,
                        double      dt_seconds,
                        double      alpha,
                        double      min_step_len_m,
                        double      mean_speed_mps,
                        double      area_width_m,
                        double      area_height_m,
                        double     *flight_remaining_s,
                        double     *current_heading)
{
    if (!pos || !flight_remaining_s || !current_heading) return;

    /* If still in current flight, continue */
    if (*flight_remaining_s > 0.0) {
        *flight_remaining_s -= dt_seconds;
        pos->speed_mps = mean_speed_mps;
        pos->heading_rad = *current_heading;
        pos->velocity_x = mean_speed_mps * cos(pos->heading_rad);
        pos->velocity_y = mean_speed_mps * sin(pos->heading_rad);
        pos->position_x += pos->velocity_x * dt_seconds;
        pos->position_y += pos->velocity_y * dt_seconds;
        return;
    }

    /* Generate new flight: random direction */
    *current_heading = _mob_uniform() * 2.0 * M_PI;
    pos->heading_rad = *current_heading;

    /* Generate flight length from power-law distribution
     * l = l_min * u^{-1/(α-1)} */
    double u = _mob_uniform();
    if (u < 1e-12) u = 1e-12;
    double exponent = -1.0 / (alpha - 1.0);

    /* Clamp exponent to avoid overflow (α-1 could be very small) */
    if (fabs(exponent) > 10.0) exponent = (exponent > 0) ? 10.0 : -10.0;

    double flight_length = min_step_len_m * pow(u, exponent);

    /* Cap flight length to area diagonal to avoid excessive flights */
    double diag = sqrt(area_width_m * area_width_m + area_height_m * area_height_m);
    if (flight_length > diag) flight_length = diag;

    /* Flight time */
    *flight_remaining_s = flight_length / mean_speed_mps;
    if (*flight_remaining_s < dt_seconds) {
        /* Very short flight, just do it */
        double dist = mean_speed_mps * dt_seconds;
        pos->position_x += dist * cos(*current_heading);
        pos->position_y += dist * sin(*current_heading);
        *flight_remaining_s = 0.0;
    } else {
        *flight_remaining_s -= dt_seconds;
        pos->speed_mps = mean_speed_mps;
        pos->velocity_x = mean_speed_mps * cos(pos->heading_rad);
        pos->velocity_y = mean_speed_mps * sin(pos->heading_rad);
        pos->position_x += pos->velocity_x * dt_seconds;
        pos->position_y += pos->velocity_y * dt_seconds;
    }

    /* Wrap around (toroidal boundary) — Levy walks explore large areas */
    while (pos->position_x < 0) pos->position_x += area_width_m;
    while (pos->position_x > area_width_m) pos->position_x -= area_width_m;
    while (pos->position_y < 0) pos->position_y += area_height_m;
    while (pos->position_y > area_height_m) pos->position_y -= area_height_m;
}

/* ============================================================================
 * L3: Directional (Manhattan/Highway) Mobility Model
 *
 * Models constrained movement where UE follows predefined paths:
 *   - Manhattan grid: movement along streets (N-S or E-W), turns at
 *     intersections with probability p_turn
 *   - Highway: movement along a corridor with lane discipline
 *
 * Mathematical structure: Piecewise-linear trajectories with random
 * direction changes at discrete points (intersections). The model
 * produces a non-isotropic spatial distribution reflecting urban topology.
 *
 * Application: Urban small-cell handover, V2I (Vehicle-to-Infrastructure)
 * communication in ITS (Intelligent Transportation Systems).
 */
void mob_directional_step(UEPosition *pos,
                          double      dt_seconds,
                          double      speed_mps,
                          double      heading_rad,
                          double      lane_width_m,
                          double      intersection_proximity_m,
                          double      turn_probability,
                          double      grid_spacing_m)
{
    if (!pos || dt_seconds <= 0.0) return;
    (void)lane_width_m; /* Reserved for lane-constrained dynamics */

    pos->speed_mps = speed_mps;
    pos->heading_rad = heading_rad;

    /* Snap heading to cardinal directions (0, π/2, π, 3π/2) */
    double snapped = round(heading_rad / (M_PI / 2.0)) * (M_PI / 2.0);
    pos->heading_rad = snapped;

    /* Move */
    pos->velocity_x = speed_mps * cos(snapped);
    pos->velocity_y = speed_mps * sin(snapped);
    pos->position_x += pos->velocity_x * dt_seconds;
    pos->position_y += pos->velocity_y * dt_seconds;

    /* Check if near an intersection (position near multiple of grid_spacing_m) */
    double nearest_x_intersection = round(pos->position_x / grid_spacing_m) * grid_spacing_m;
    double nearest_y_intersection = round(pos->position_y / grid_spacing_m) * grid_spacing_m;

    double dx_intersection = fabs(pos->position_x - nearest_x_intersection);
    double dy_intersection = fabs(pos->position_y - nearest_y_intersection);

    /* If close to an intersection, maybe turn */
    if (dx_intersection < intersection_proximity_m
        && dy_intersection < intersection_proximity_m) {
        if (_mob_uniform() < turn_probability) {
            /* Turn: randomly choose left, right, or continue */
            double r = _mob_uniform();
            double new_heading = snapped;
            if (r < 0.333) {
                new_heading = snapped + M_PI / 2.0;  /* Turn left */
            } else if (r < 0.667) {
                new_heading = snapped - M_PI / 2.0;  /* Turn right */
            }
            /* else continue straight */

            pos->heading_rad = new_heading;
            /* Snap to grid */
            pos->position_x = nearest_x_intersection;
            pos->position_y = nearest_y_intersection;
        }
    }

    /* Keep heading in [-π, π] */
    while (pos->heading_rad > M_PI)  pos->heading_rad -= 2.0 * M_PI;
    while (pos->heading_rad < -M_PI) pos->heading_rad += 2.0 * M_PI;
}

/* ============================================================================
 * L3: Reference Point Group Mobility (RPGM)
 *
 * RPGM (Hong et al., 1999) models coordinated group movement such as:
 *   - Military units moving in formation
 *   - Emergency responders converging on disaster site
 *   - Tourists following a guide
 *
 * Each member i moves according to:
 *   U_i(t) = RP(t) + RM_i(t)
 *
 * where RP(t) is the group reference point (moves along a group trajectory)
 * and RM_i(t) is the individual random motion component.
 *
 * The RM_i components are independent random walks with bounded deviation
 * from RP, ensuring group cohesion.
 */
void mob_group_rpgm_step(UEPosition *group_center,
                         UEPosition *members,
                         int         num_members,
                         double      dt_seconds,
                         double      group_speed_mps,
                         double      group_heading_rad,
                         double      member_deviation_m,
                         double      boundary_x_min,
                         double      boundary_x_max,
                         double      boundary_y_min,
                         double      boundary_y_max)
{
    if (!group_center || !members || num_members <= 0 || dt_seconds <= 0.0) return;

    /* Move the group reference point */
    group_center->velocity_x = group_speed_mps * cos(group_heading_rad);
    group_center->velocity_y = group_speed_mps * sin(group_heading_rad);
    group_center->speed_mps = group_speed_mps;
    group_center->heading_rad = group_heading_rad;
    group_center->position_x += group_center->velocity_x * dt_seconds;
    group_center->position_y += group_center->velocity_y * dt_seconds;

    /* Boundary check for group center */
    _mob_reflect_boundary(group_center, boundary_x_min, boundary_x_max,
                          boundary_y_min, boundary_y_max);

    /* Move each member: follow group center + individual random deviation */
    for (int i = 0; i < num_members; i++) {
        /* Random deviation */
        double dev_angle = _mob_uniform() * 2.0 * M_PI;
        double dev_dist = _mob_uniform() * member_deviation_m;

        /* Target position = group center + deviation offset */
        double target_x = group_center->position_x + dev_dist * cos(dev_angle);
        double target_y = group_center->position_y + dev_dist * sin(dev_angle);

        /* Member moves toward its target with some speed */
        double dx = target_x - members[i].position_x;
        double dy = target_y - members[i].position_y;
        double dist = sqrt(dx * dx + dy * dy);

        double mem_speed = group_speed_mps * (1.0 + 0.5 * _mob_uniform());
        double step = mem_speed * dt_seconds;

        if (step >= dist) {
            members[i].position_x = target_x;
            members[i].position_y = target_y;
        } else {
            double angle = atan2(dy, dx);
            members[i].position_x += step * cos(angle);
            members[i].position_y += step * sin(angle);
        }

        members[i].heading_rad = atan2(dy, dx);
        members[i].speed_mps = mem_speed;
        members[i].velocity_x = mem_speed * cos(members[i].heading_rad);
        members[i].velocity_y = mem_speed * sin(members[i].heading_rad);
    }
}

/* ============================================================================
 * L3: Mean Square Displacement (MSD) Analysis
 *
 * MSD(τ) = (1/(N-τ)) · Σ_{t=0}^{N-τ-1} |r(t+τ) - r(t)|²
 *
 * The MSD is the primary tool for characterizing mobility regimes:
 *
 *   MSD ∝ τ   → normal diffusion (random walk, Brownian)
 *   MSD ∝ τ²  → ballistic motion (straight line, constant velocity)
 *   MSD ∝ τ^β, β>1 → superdiffusion (Levy walk, 2<α<3)
 *   MSD ∝ τ^β, β<1 → subdiffusion (obstacles, caging)
 *
 * Log-log slope estimation via linear regression:
 *   log(MSD) = β · log(τ) + c
 */
void mob_compute_msd(const UEPosition *positions,
                     int               num_points,
                     double           *msd_array,
                     int               max_lag)
{
    if (!positions || !msd_array || num_points <= 1 || max_lag <= 0) return;

    if (max_lag >= num_points) max_lag = num_points - 1;

    for (int lag = 1; lag <= max_lag; lag++) {
        double sum_sq_displacement = 0.0;
        int count = 0;

        for (int t = 0; t + lag < num_points; t++) {
            double dx = positions[t + lag].position_x - positions[t].position_x;
            double dy = positions[t + lag].position_y - positions[t].position_y;
            sum_sq_displacement += dx * dx + dy * dy;
            count++;
        }

        msd_array[lag - 1] = (count > 0) ? (sum_sq_displacement / count) : 0.0;
    }
}

/* ============================================================================
 * Mobility State Classification (3GPP TS 36.304)
 *
 * Classifies UE into mobility states based on speed. This is used for
 * scaling mobility parameters:
 *   - High-speed UE: use shorter TTT, lower hysteresis
 *   - Stationary UE: use longer TTT, higher hysteresis
 *
 * 3GPP scaling rules (TS 36.304 §5.2.4.3.1):
 *   High-mobility:  Scale TTT and T_reselection by sf-High
 *   Medium-mobility: Scale TTT and T_reselection by sf-Medium
 */
MobilityState mob_classify_state(double speed_mps,
                                 double high_speed_threshold_mps,
                                 double medium_speed_threshold_mps)
{
    if (speed_mps >= high_speed_threshold_mps) {
        return MOB_HIGH_SPEED;
    } else if (speed_mps >= medium_speed_threshold_mps) {
        return MOB_MEDIUM;
    } else if (speed_mps > 0.5) { /* 0.5 m/s ≈ 1.8 km/h walking */
        return MOB_NORMAL;
    } else {
        return MOB_STATIONARY;
    }
}

/* ============================================================================
 * L4: Doppler Shift Computation
 *
 * Doppler Effect (Christian Doppler, 1842):
 *   When a signal source and receiver are in relative motion, the observed
 *   frequency differs from the transmitted frequency:
 *
 *   f_observed = f_c · (c + v_r) / (c + v_s)
 *
 * For mobile UE (receiver moving, source stationary):
 *   f_d = (v/c) · f_c · cos(θ)
 *
 * where v = UE speed, c = 3×10⁸ m/s, f_c = carrier frequency,
 * θ = angle between velocity and line-of-sight.
 *
 * The Doppler shift causes frequency dispersion (Doppler spread), which
 * determines the channel coherence time and affects handover parameter
 * selection (shorter TTT for high f_d).
 */
double mob_compute_doppler_shift(double speed_mps,
                                 double carrier_freq_hz,
                                 double angle_rad)
{
    const double SPEED_OF_LIGHT = 299792458.0; /* m/s */
    return (speed_mps / SPEED_OF_LIGHT) * carrier_freq_hz * cos(angle_rad);
}

/* ============================================================================
 * L4: Coherence Time Computation
 *
 * Channel coherence time T_c is the time duration over which the channel
 * can be considered invariant. It is inversely proportional to the
 * maximum Doppler spread.
 *
 * Clarke's model (1968) — isotropic scattering:
 *   T_c ≈ 9 / (16π · f_d_max) ≈ 0.423 / f_d_max  (correlation ≥ 0.5)
 *
 * Practical rule of thumb:
 *   T_c ≈ sqrt(9/(16π)) / f_d_max ≈ 0.423 / f_d_max
 *
 * For handover parameter tuning:
 *   - If T_c < TTT, measurements may decorrelate before TTT expires
 *   - TTT should be set proportional to T_c for reliable decisions
 *
 * Example: At 2.6 GHz, 120 km/h → f_d ≈ 289 Hz → T_c ≈ 1.46 ms
 */
double mob_compute_coherence_time(double max_doppler_hz)
{
    if (max_doppler_hz < 1e-12) return 1e12; /* Practically infinite for static */
    return 0.423 / max_doppler_hz;
}

/* ============================================================================
 * Handover Rate Estimation
 *
 * For a mobile UE in a hexagonal cell layout with cell radius R:
 *
 * Expected handover rate (crossings per second):
 *   λ_HO ≈ 2·v / (π·R)
 *
 * Derivation: For random direction of movement (uniform in [0, 2π)), the
 * probability that a step crosses a cell boundary is proportional to
 * the length of the chord through the cell. Averaging over all entry
 * points and directions gives the above formula.
 *
 * For N UEs, total handover rate: Λ_HO = N · λ_HO
 * This is used for network dimensioning (X2 link capacity, MME load).
 *
 * References:
 *   - Xie & Kuek (1993), "Handover analyses formula", IEEE VTC
 *   - 3GPP TR 36.839 (Mobility enhancements in heterogeneous networks)
 */
double mob_handover_rate_estimate(double speed_mps, double cell_radius_m)
{
    if (cell_radius_m < 1e-6) return 1e9; /* Avoid division by zero */
    return (2.0 * speed_mps) / (M_PI * cell_radius_m);
}
