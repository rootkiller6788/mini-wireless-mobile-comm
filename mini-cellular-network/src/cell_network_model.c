#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "cell_network_model.h"

void hex_to_cartesian(hex_coord_t h, double R, double *x, double *y) {
    *x = 1.7320508 * ((double)h.q + 0.5 * (double)h.r) * R;
    *y = 1.5 * (double)h.r * R;
}

double hex_distance(hex_coord_t a, hex_coord_t b, double R) {
    int dq = a.q - b.q, dr = a.r - b.r;
    int ds = (-a.q - a.r) - (-b.q - b.r);
    int steps = (abs(dq) > abs(dr)) ? abs(dq) : abs(dr);
    steps = (steps > abs(ds)) ? steps : abs(ds);
    return steps * 1.7320508 * R;
}


int hex_hop_distance(hex_coord_t a, hex_coord_t b) {
    int dq = a.q - b.q, dr = a.r - b.r;
    int ds = (-a.q - a.r) - (-b.q - b.r);
    int steps = (abs(dq) > abs(dr)) ? abs(dq) : abs(dr);
    steps = (steps > abs(ds)) ? steps : abs(ds);
    return steps;
}

int hex_equal(hex_coord_t a, hex_coord_t b) {
    return (a.q == b.q) && (a.r == b.r);
}

int hex_ring(hex_coord_t center, int radius, hex_coord_t *result, int max_results) {
    if (radius < 0 || max_results <= 0) return 0;
    if (radius == 0) {
        if (max_results >= 1) { result[0] = center; return 1; }
        return 0;
    }
    int max_n = 6 * radius;
    if (max_n > max_results) max_n = max_results;
    hex_coord_t cur; cur.q = center.q + radius; cur.r = center.r;
    int count = 0;
    int side_dir_order[6] = {4, 5, 0, 1, 2, 3};
    static const int hex_dirs[6][2] = {{1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1}};
    for (int side = 0; side < 6; side++) {
        int d = side_dir_order[side];
        for (int step = 0; step < radius && count < max_n; step++) {
            result[count] = cur; count++;
            cur.q += hex_dirs[d][0]; cur.r += hex_dirs[d][1];
        }
    }
    return count;
}

int hex_filled_grid(hex_coord_t center, int max_radius,
                    hex_coord_t *result, int max_results) {
    int total = 0;
    for (int r = 0; r <= max_radius && total < max_results; r++) {
        int written = hex_ring(center, r, result + total, max_results - total);
        total += written;
    }
    return total;
}

int freq_reuse_validate(int i, int j, int *n_out) {
    if (i < 0 || j < 0) return 0;
    if (i == 0 && j == 0) return 0;
    int N = i * i + i * j + j * j;
    if (N <= 0) return 0;
    if (n_out) *n_out = N;
    return 1;
}

double freq_reuse_d_over_r(int N) {
    if (N <= 0) return 0.0;
    return sqrt(3.0 * (double)N);
}

double freq_reuse_worst_sir_db(int N, double gamma, int n_interferers) {
    if (N <= 0 || gamma <= 0.0 || n_interferers <= 0) return -999.0;
    double d_over_r = freq_reuse_d_over_r(N);
    double sir_linear = pow(d_over_r, gamma) / (double)n_interferers;
    return 10.0 * log10(sir_linear);
}

int freq_reuse_assign_groups(cell_cluster_t *cluster, int N) {
    if (!cluster || N <= 0) return -1;
    int i_found = 0, j_found = 0, found = 0;
    for (int i = 0; i <= (int)sqrt(N) + 1 && !found; i++) {
        for (int j = 0; j <= (int)sqrt(N) + 1 && !found; j++) {
            if (i * i + i * j + j * j == N) { i_found = i; j_found = j; found = 1; }
        }
    }
    if (!found) {
        for (int k = 0; k < cluster->num_sites; k++) cluster->sites[k].freq_group = k % N;
        return cluster->num_sites;
    }
    for (int k = 0; k < cluster->num_sites; k++) {
        int q = cluster->sites[k].coord.q, r = cluster->sites[k].coord.r;
        int grp = (q * j_found - r * i_found) % N;
        if (grp < 0) grp += N;
        cluster->sites[k].freq_group = grp;
    }
    return cluster->num_sites;
}


double compute_cochannel_interference_dbm(cell_cluster_t *cluster, int victim_idx, double path_loss_exp) {
    if (!cluster || victim_idx < 0 || victim_idx >= cluster->num_sites) return -999.0;
    cell_site_t *victim = &cluster->sites[victim_idx];
    double ref_dist_m = 100.0;
    double pl_ref_db = 20.0 * log10(0.1) + 20.0 * log10(victim->params.center_freq_mhz) + 32.45;
    double interfer_linear_mw = 0.0;
    for (int i = 0; i < cluster->num_sites; i++) {
        if (i == victim_idx) continue;
        cell_site_t *interf = &cluster->sites[i];
        if (interf->freq_group != victim->freq_group) continue;
        if (!interf->is_active) continue;
        double d_m = hex_distance(victim->coord, interf->coord, cluster->inter_site_distance_m / 1.7320508);
        if (d_m < 1.0) d_m = 1.0;
        double pl_db = pl_ref_db + 10.0 * path_loss_exp * log10(d_m / ref_dist_m);
        if (pl_db < 0.0) pl_db = 0.0;
        double rx_interf_dbm = interf->params.tx_power_dbm + interf->params.antenna_gain_dbi
            + victim->params.antenna_gain_dbi - pl_db - interf->params.cable_loss_db;
        interfer_linear_mw += pow(10.0, rx_interf_dbm / 10.0);
    }
    if (interfer_linear_mw <= 0.0) return -200.0;
    return 10.0 * log10(interfer_linear_mw);
}

int build_interference_matrix(cell_cluster_t *cluster, double path_loss_exp, interference_matrix_t *imat) {
    if (!cluster || !imat) return -1;
    int n = cluster->num_sites;
    if (n > MAX_GNB_IN_CLUSTER) n = MAX_GNB_IN_CLUSTER;
    imat->size = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) { imat->data[i][j] = -200.0; continue; }
            if (!cluster->sites[j].is_active) { imat->data[i][j] = -200.0; continue; }
            double d_m = hex_distance(cluster->sites[i].coord, cluster->sites[j].coord,
                cluster->inter_site_distance_m / 1.7320508);
            if (d_m < 1.0) d_m = 1.0;
            double pl_ref_db = 20.0 * log10(0.1) + 20.0 * log10(cluster->sites[j].params.center_freq_mhz) + 32.45;
            double pl_db = pl_ref_db + 10.0 * path_loss_exp * log10(d_m / 100.0);
            if (pl_db < 0.0) pl_db = 0.0;
            double rx_dbm = cluster->sites[j].params.tx_power_dbm + cluster->sites[j].params.antenna_gain_dbi
                + cluster->sites[i].params.antenna_gain_dbi - pl_db - cluster->sites[j].params.cable_loss_db;
            imat->data[i][j] = rx_dbm;
        }
    }
    return n;
}

void nrt_init(nrt_table_t *nrt) {
    if (!nrt) return;
    memset(nrt, 0, sizeof(*nrt));
}

void nrt_add(nrt_table_t *nrt, uint32_t src, uint32_t tgt, ho_type_t ho_type, int x2_avail) {
    if (!nrt) return;
    for (int i = 0; i < nrt->num_entries; i++) {
        if (nrt->entries[i].source_cell_id == src && nrt->entries[i].target_cell_id == tgt) {
            nrt->entries[i].ho_type = ho_type;
            nrt->entries[i].is_x2_available = x2_avail;
            return;
        }
    }
    if (nrt->num_entries < 256) {
        int i = nrt->num_entries++;
        nrt->entries[i].source_cell_id = src;
        nrt->entries[i].target_cell_id = tgt;
        nrt->entries[i].ho_type = ho_type;
        nrt->entries[i].is_ho_allowed = 1;
        nrt->entries[i].is_remove_allowed = 0;
        nrt->entries[i].is_x2_available = x2_avail;
    }
}

nrt_entry_t *nrt_find(nrt_table_t *nrt, uint32_t src, uint32_t tgt) {
    if (!nrt) return NULL;
    for (int i = 0; i < nrt->num_entries; i++)
        if (nrt->entries[i].source_cell_id == src && nrt->entries[i].target_cell_id == tgt)
            return &nrt->entries[i];
    return NULL;
}

void nrt_update_ho_stats(nrt_table_t *nrt, uint32_t src, uint32_t tgt, int success, double rsrp) {
    nrt_entry_t *e = nrt_find(nrt, src, tgt);
    if (!e) return;
    if (success) e->ho_success_count++; else e->ho_failure_count++;
    e->avg_rsrp_db = rsrp;
}

int cell_cluster_init_hexagonal(cell_cluster_t *cluster, int num_rings,
    double inter_site_distance_m, double cell_radius_m) {
    if (!cluster || num_rings < 0) return -1;
    memset(cluster, 0, sizeof(*cluster));
    cluster->inter_site_distance_m = inter_site_distance_m;
    hex_coord_t center = {0, 0};
    hex_coord_t coords[MAX_GNB_IN_CLUSTER];
    int n = hex_filled_grid(center, num_rings, coords, MAX_GNB_IN_CLUSTER);
    for (int i = 0; i < n; i++) {
        cluster->sites[i].coord = coords[i];
        cluster->sites[i].cell_id = (uint32_t)(i + 1);
        hex_to_cartesian(coords[i], cell_radius_m, &cluster->sites[i].center_x_m, &cluster->sites[i].center_y_m);
        cluster->sites[i].radius_m = cell_radius_m;
        cluster->sites[i].is_active = 1;
        cluster->sites[i].type = CELL_TYPE_MACRO;
    }
    cluster->num_sites = n;
    cluster->area_sqkm = n * 2.598 * cell_radius_m * cell_radius_m / 1e6;
    cluster->grid_radius = num_rings;
    return n;
}
