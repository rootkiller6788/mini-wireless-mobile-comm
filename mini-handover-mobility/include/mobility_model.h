/**
 * @file mobility_model.h
 * @brief Mobility models for wireless network simulation (L3 Mathematical Structures)
 *
 * Implements mathematical models of user movement that drive handover events.
 * Each model represents a distinct stochastic or deterministic mobility pattern
 * used in 3GPP and academic research for handover performance evaluation.
 *
 * Knowledge Coverage:
 *   L3 - Mathematical structures: Random Walk (Brownian motion), Random Waypoint,
 *        Gauss-Markov, Levy Walk, Directional, Group Mobility (RPGM)
 *   L4 - Fundamental Laws: Diffusion equation for random walk, Doppler effect
 *
 * References:
 *   - 3GPP TR 38.901 §7 (Channel model, mobility models)
 *   - Camp, Boleng, Davies, "A Survey of Mobility Models" (2002)
 *   - Bettstetter, "Smooth is Better than Sharp" (MobiHoc 2001)
 */

#ifndef MOBILITY_MODEL_H
#define MOBILITY_MODEL_H

#include "handover_types.h"

/* ============================================================================
 * L3: Random Walk Mobility Model (Brownian Motion)
 * ============================================================================ */

/**
 * mob_random_walk_step - Execute one step of the Random Walk mobility model.
 *
 * The Random Walk model is the simplest mobility model, derived from Brownian
 * motion. At each time step, the UE randomly chooses a direction and speed
 * within configured bounds, independent of previous steps.
 *
 * Mathematical structure (2D diffusion process):
 *   dx = v · cos(θ) · dt + σ_x · dW_x
 *   dy = v · sin(θ) · dt + σ_y · dW_y
 *
 * where dW_x, dW_y are independent Wiener processes.
 *
 * Long-term behavior: MSD(t) ∝ t (diffusive regime).
 *
 * @param pos               Current position (modified in place).
 * @param dt_seconds        Time step duration.
 * @param mean_speed_mps     Mean movement speed.
 * @param speed_std_mps      Speed standard deviation.
 * @param turn_rate_rads     Angular change rate (rad/s).
 * @param boundary_x_min     Minimum X boundary.
 * @param boundary_x_max     Maximum X boundary.
 * @param boundary_y_min     Minimum Y boundary.
 * @param boundary_y_max     Maximum Y boundary.
 */
void mob_random_walk_step(UEPosition *pos,
                          double      dt_seconds,
                          double      mean_speed_mps,
                          double      speed_std_mps,
                          double      turn_rate_rads,
                          double      boundary_x_min,
                          double      boundary_x_max,
                          double      boundary_y_min,
                          double      boundary_y_max);

/* ============================================================================
 * L3: Random Waypoint Mobility Model (RWP)
 * ============================================================================ */

/**
 * mob_random_waypoint_step - Execute one step of Random Waypoint model.
 *
 * The Random Waypoint model (Johnson & Maltz, 1996) is the most widely used
 * mobility model in ad-hoc network simulation. A UE picks a random destination
 * and moves toward it at constant speed. Upon arrival, it pauses for a random
 * duration, then selects a new destination.
 *
 * Mathematical property: The spatial distribution of nodes is non-uniform,
 * concentrating toward the center of the simulation area (density wave).
 *
 * @param pos               Current UE position.
 * @param dt_seconds        Time step.
 * @param min_speed_mps      Minimum speed.
 * @param max_speed_mps      Maximum speed.
 * @param pause_time_ms      Pause time at destination.
 * @param area_width_m       Simulation area width.
 * @param area_height_m      Simulation area height.
 * @param[in,out] dest_x    Current destination X (updated if reached).
 * @param[in,out] dest_y    Current destination Y (updated if reached).
 * @param[in,out] pause_remaining_ms Remaining pause time.
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
                              double     *pause_remaining_ms);

/* ============================================================================
 * L3: Gauss-Markov Mobility Model
 * ============================================================================ */

/**
 * mob_gauss_markov_step - Execute one step of the Gauss-Markov mobility model.
 *
 * The Gauss-Markov model (Liang & Haas, 1999) introduces temporal dependency
 * in mobility patterns. Velocity at time t depends on velocity at t-1 through
 * a memory parameter α ∈ [0, 1]:
 *
 *   v_t = α · v_{t-1} + (1-α) · μ + sqrt(1-α²) · σ · w_{t-1}
 *   θ_t = α · θ_{t-1} + (1-α) · μ_θ + sqrt(1-α²) · σ_θ · w_{θ,t-1}
 *
 * Where w ~ N(0,1) is Gaussian noise, μ is the asymptotic mean.
 *
 * α = 0: Pure random walk (no memory)
 * α = 1: Linear motion (perfect memory)
 *
 * @param pos               Current UE position (updated in place).
 * @param dt_seconds        Time step.
 * @param alpha             Memory parameter [0, 1].
 * @param mean_speed_mps     Asymptotic mean speed.
 * @param speed_std_mps      Speed standard deviation.
 * @param mean_direction_rad Asymptotic mean direction.
 * @param direction_std_rad  Direction standard deviation.
 * @param boundary_x_min/max Boundary constraints.
 * @param boundary_y_min/max Boundary constraints.
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
                           double      boundary_y_max);

/* ============================================================================
 * L3: Levy Walk Mobility Model
 * ============================================================================ */

/**
 * mob_levy_walk_step - Execute one step of the Levy Walk mobility model.
 *
 * The Levy Walk model captures human mobility patterns where step lengths
 * follow a heavy-tailed (power-law) distribution:
 *
 *   P(l) ∝ l^{-α},  with 1 < α < 3
 *
 * This produces "superdiffusive" behavior characterized by occasional long
 * flights interspersed with short local movements — matching empirical
 * observations of human mobility (Gonzalez et al., Nature 2008).
 *
 * MSD(t) ∝ t^{2/(α-1)} for 2 < α < 3 (superdiffusive regime).
 *
 * @param pos               Current UE position.
 * @param dt_seconds        Time step.
 * @param alpha             Levy exponent (1 < α < 3).
 * @param min_step_len_m     Minimum step length.
 * @param mean_speed_mps     Mean flight speed.
 * @param area_width_m       Area width.
 * @param area_height_m      Area height.
 * @param[in,out] flight_remaining_s Remaining flight time.
 * @param[in,out] current_heading Current heading.
 */
void mob_levy_walk_step(UEPosition *pos,
                        double      dt_seconds,
                        double      alpha,
                        double      min_step_len_m,
                        double      mean_speed_mps,
                        double      area_width_m,
                        double      area_height_m,
                        double     *flight_remaining_s,
                        double     *current_heading);

/* ============================================================================
 * L3: Directional (Highway/Manhattan) Mobility Model
 * ============================================================================ */

/**
 * mob_directional_step - Execute one step of the Directional mobility model.
 *
 * Models constrained mobility such as vehicles on highways (1D/2D lanes)
 * or pedestrians in Manhattan grid topology. Movement is restricted to
 * predefined paths, with turning at intersections governed by probability.
 *
 * Applications: VANET handover, urban small-cell deployment.
 *
 * @param pos               Current UE position.
 * @param dt_seconds        Time step.
 * @param speed_mps          Current speed along path.
 * @param heading_rad       Direction of travel.
 * @param lane_width_m       Lane width (for lateral constraint).
 * @param intersection_proximity_m Distance within which turning is allowed.
 * @param turn_probability  Probability of turning at intersection [0, 1].
 * @param grid_spacing_m     Distance between intersections.
 */
void mob_directional_step(UEPosition *pos,
                          double      dt_seconds,
                          double      speed_mps,
                          double      heading_rad,
                          double      lane_width_m,
                          double      intersection_proximity_m,
                          double      turn_probability,
                          double      grid_spacing_m);

/* ============================================================================
 * L3: Reference Point Group Mobility (RPGM)
 * ============================================================================ */

/**
 * mob_group_rpgm_step - Execute one step of the RPGM group mobility model.
 *
 * RPGM (Hong et al., 1999) models group movement where group members follow
 * a logical "reference point" (group center) with individual random deviations.
 * Used for: military squad movement, emergency responder teams, tourist groups.
 *
 *   U_i(t) = RP(t) + RM_i(t)
 *
 * Where RP is the reference point trajectory and RM_i is the random motion
 * vector of member i relative to the reference point.
 *
 * @param group_center      Group reference point position (updated in place).
 * @param members           Array of member positions (updated in place).
 * @param num_members       Number of group members.
 * @param dt_seconds        Time step.
 * @param group_speed_mps    Speed of group center.
 * @param group_heading_rad  Direction of group center.
 * @param member_deviation_m Max random deviation of members from center.
 * @param boundary_x_min/max Area boundaries.
 * @param boundary_y_min/max Area boundaries.
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
                         double      boundary_y_max);

/* ============================================================================
 * L3: Mobility Statistics and Metrics
 * ============================================================================ */

/**
 * mob_compute_mean_square_displacement - Compute MSD from position trace.
 *
 * MSD(τ) = ⟨|r(t+τ) - r(t)|²⟩
 *
 * The MSD reveals the mobility regime:
 *   MSD ∝ τ   → diffusive (random walk)
 *   MSD ∝ τ²  → ballistic (straight line)
 *   MSD ∝ τ^β → anomalous (superdiffusive β>1, subdiffusive β<1)
 *
 * @param positions         Array of UE positions over time.
 * @param num_points        Number of position samples.
 * @param[out] msd_array    Mean square displacement for each lag.
 * @param max_lag           Maximum lag to compute.
 */
void mob_compute_msd(const UEPosition *positions,
                     int               num_points,
                     double           *msd_array,
                     int               max_lag);

/**
 * mob_classify_state - Classify UE mobility state from speed.
 *
 * Implements the 3GPP mobility state detection (TS 36.304 §5.2.4.3):
 *   Count number of cell reselections within T_CRmax.
 *   N_CR > N_CR_H → High-mobility
 *   N_CR_M ≤ N_CR ≤ N_CR_H → Medium-mobility
 *   Otherwise → Normal-mobility
 *
 * @param speed_mps         Current UE speed in m/s.
 * @param high_speed_threshold_mps Threshold for high-speed classification.
 * @param medium_speed_threshold_mps Threshold for medium-speed classification.
 * @return Classified MobilityState.
 */
MobilityState mob_classify_state(double speed_mps,
                                 double high_speed_threshold_mps,
                                 double medium_speed_threshold_mps);

/**
 * mob_compute_doppler_shift - Compute maximum Doppler shift.
 *
 * L4: Doppler Effect
 *   f_d = (v / c) · f_c · cos(θ)
 *
 * where v = UE speed, c = speed of light, f_c = carrier frequency, θ = angle
 * between velocity vector and line-of-sight to base station.
 *
 * @param speed_mps         UE speed in m/s.
 * @param carrier_freq_hz   Carrier frequency in Hz.
 * @param angle_rad         Angle between velocity and LOS (0 = directly toward).
 * @return Doppler shift in Hz.
 */
double mob_compute_doppler_shift(double speed_mps,
                                 double carrier_freq_hz,
                                 double angle_rad);

/**
 * mob_compute_coherence_time - Compute channel coherence time.
 *
 * L4: Coherence time approximation (Clarke's model):
 *   T_c ≈ 9 / (16π · f_d)  ≈ 0.423 / f_d_max
 *
 * This is the time duration over which the channel impulse response is
 * essentially invariant — critical for handover parameter tuning.
 *
 * @param max_doppler_hz    Maximum Doppler shift in Hz.
 * @return Coherence time in seconds.
 */
double mob_compute_coherence_time(double max_doppler_hz);

/**
 * mob_handover_rate_estimate - Estimate handover rate from mobility parameters.
 *
 * For a random walk model in a hexagonal cell layout (cell radius R):
 *   Expected HO rate ≈ 2·v / (π·R)  (crossings per second)
 *
 * This is a first-order approximation used for handover resource dimensioning.
 *
 * @param speed_mps          Mean UE speed.
 * @param cell_radius_m      Cell radius in meters.
 * @return Estimated handover rate (handovers per second).
 */
double mob_handover_rate_estimate(double speed_mps, double cell_radius_m);

#endif /* MOBILITY_MODEL_H */
