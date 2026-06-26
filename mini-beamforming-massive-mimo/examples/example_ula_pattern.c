/**
 * example_ula_pattern.c — ULA Beam Pattern Visualization
 * L6 Canonical Problem: Compute and display beam pattern for 8-element ULA
 * Demonstrates: DAS beamforming, beamwidth, sidelobe level
 */
#include "beamforming_types.h"
#include "beamforming_array.h"
#include <stdio.h>

int main(void) {
    printf("=== ULA Beam Pattern Example ===\n\n");
    /* 8-element ULA at 3 GHz, lambda/2 spacing */
    ula_geometry ula = ula_init(3.0e9, 8, 0.5);
    printf("Array: N=%zu elements, d=%.3f mm, f=%.1f GHz\n",
           ula.num_elements, ula.element_spacing * 1000.0, ula.freq_hz / 1e9);

    /* DAS beamformer steering to broadside (theta=90 degrees from axis,
     * which is 0 degrees from broadside) */
    complex_vector weights = cvec_alloc(ula.num_elements);
    steering_direction_1d steer_dir;
    steer_dir.theta_rad = M_PI / 2.0;  /* Broadside: cos(pi/2)=0 */
    steer_dir.sin_theta = 1.0;
    ula_das_weights(&ula, steer_dir, &weights);

    /* Compute beam pattern: full -90 to +90 degree scan */
    beam_pattern bp = ula_beam_pattern(&ula, &weights,
        -M_PI / 2.0, M_PI / 2.0, 181);

    printf("Steering angle: %.1f deg\n", bp.steering_angle * 180.0 / M_PI);
    printf("Half-power beamwidth: %.2f deg\n", bp.half_power_bw * 180.0 / M_PI);
    printf("Peak sidelobe level: %.1f dB\n", bp.max_sidelobe_db);
    printf("Theoretical HPBW: %.2f deg\n",
           0.886 * ula.wavelength / (ula.num_elements * ula.element_spacing) * 180.0 / M_PI);

    /* Print beam pattern samples */
    printf("\nAngle(deg)  Gain(dB)\n");
    printf("--------------------\n");
    for (size_t i = 0; i < bp.num_angles; i += 10) {
        double angle_deg = bp.angles_rad[i] * 180.0 / M_PI;
        printf("%9.1f  %8.1f\n", angle_deg, bp.gain_db[i]);
    }

    beam_pattern_free(&bp);
    cvec_free(&weights);
    printf("\nDone.\n");
    return 0;
}
