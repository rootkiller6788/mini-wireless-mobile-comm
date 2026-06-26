/**
 * @file cell_network_model.h
 * @brief Cellular Network Model ? Hexagonal Grid & Frequency Reuse (L2)
 *
 * Reference: Molisch (2011) Ch. 17, "Cellular Principles"
 *            MacDonald, "The Cellular Concept" (1979), Bell System Technical Journal
 *            Goldsmith, "Wireless Communications" (2005) Ch. 15
 *
 * Implements hexagonal cell grid geometry, frequency reuse patterns,
 * co-channel interference calculation, and neighbor relation tables.
 */

#ifndef CELL_NETWORK_MODEL_H
#define CELL_NETWORK_MODEL_H

#include "cell_network_defs.h"

/* ================================================================
 * L2: Hexagonal Cell Grid ? Coordinate System
 * ================================================================ */

/** Axial coordinates for hexagonal grid (flat-top orientation).
 *  Using 2-axis representation: q (horizontal), r (diagonal).
 *  Third cube coordinate s = -q - r (implied).
 */
typedef struct {
    int q;  /* Column */
    int r;  /* Row */
} hex_coord_t;

/** A single cell in the deployment grid */
typedef struct {
    hex_coord_t  coord;
    uint32_t     cell_id;
    uint32_t     enb_id;
    cell_type_t  type;
    double       center_x_m;      /* Cartesian x (flat-top: x = sqrt(3)*(q + r/2)) */
    double       center_y_m;      /* Cartesian y (flat-top: y = 3/2 * r) */
    double       radius_m;        /* Cell radius (distance from center to vertex) */
    double       azimuth_deg;     /* Sector azimuth (0=N, 90=E) for 3-sector sites */
    int          sector_index;    /* 0,1,2 for 3-sector sites */
    int          freq_group;      /* Frequency reuse group (1..N) */
    gnb_params_t params;
    int          is_active;
} cell_site_t;

/* ================================================================
 * L2: Cell Cluster (set of cells in a deployment area)
 * ================================================================ */

typedef struct {
    cell_site_t  sites[MAX_GNB_IN_CLUSTER];
    int          num_sites;
    double       inter_site_distance_m;  /* ISD */
    double       area_sqkm;
    int          grid_radius;  /* Number of rings around center */
} cell_cluster_t;

/* ================================================================
 * L2: Frequency Reuse Pattern
 * ================================================================ */

/** Frequency reuse factor N = i^2 + i*j + j^2 (standard proof via hex geometry)
 *  N=1: full reuse, N=3: 1/3 reuse, N=4: 1/4, N=7: 1/7, N=12: 1/12
 */
typedef struct {
    int n_reuse;         /* Reuse factor N */
    int i;               /* Hex grid shift parameter i */
    int j;               /* Hex grid shift parameter j */
    int num_groups;
    double d_over_r;     /* Co-channel reuse distance ratio D/R = sqrt(3N) */
    double sir_worst_db; /* Worst-case SIR for path loss exponent gamma */
    int group_map[12];   /* Site frequency group assignment */
} freq_reuse_plan_t;

/* ================================================================
 * L2: Co-Channel Interference Matrix
 * ================================================================ */

typedef struct {
    double data[MAX_GNB_IN_CLUSTER][MAX_GNB_IN_CLUSTER];
    int    size;
} interference_matrix_t;

/* ================================================================
 * L2: Neighbor Relation Table (NRT) ? Automatic Neighbor Relation
 * ================================================================ */

typedef struct {
    uint32_t source_cell_id;
    uint32_t target_cell_id;
    ho_type_t ho_type;
    int       is_ho_allowed;
    int       is_remove_allowed;
    int       is_x2_available;
    int       ho_success_count;
    int       ho_failure_count;
    double    avg_rsrp_db;
} nrt_entry_t;

typedef struct {
    nrt_entry_t entries[256];
    int         num_entries;
} nrt_table_t;

/* ================================================================
 * L2: Cell Selection / Reselection Parameters
 * ================================================================ */

typedef struct {
    double q_rxlev_min_dbm;      /* Minimum required RX level */
    double q_qual_min_db;        /* Minimum required quality level */
    double q_rxlev_min_offset_db;/* Offset to Qrxlevmin */
    double q_hyst_db;            /* Hysteresis for reselection */
    double t_reselection_s;      /* Treselection timer (seconds) */
    double s_intra_search_db;    /* S-intrasearch threshold */
    double s_non_intra_search_db;/* S-nonintrasearch threshold */
    double thresh_serving_low_db;/* ThreshServingLow */
    double cell_resel_priority;  /* Absolute priority */
    int    is_barred;
} cell_reselection_params_t;

/* ================================================================
 * L3: Hex grid distance computation
 * ================================================================ */

/** Convert hex axial to cartesian (flat-top, cell_width = sqrt(3)*R) */
void hex_to_cartesian(hex_coord_t h, double cell_radius_m,
                      double *x, double *y);

/** Distance between two hex cells (center-to-center) */
double hex_distance(hex_coord_t a, hex_coord_t b, double cell_radius_m);

/** Hex distance in hex grid steps (number of cell hops) */
int hex_hop_distance(hex_coord_t a, hex_coord_t b);

/** Generate a ring of hex coordinates at given radius */
int hex_ring(hex_coord_t center, int radius,
             hex_coord_t *result, int max_results);

/** Generate filled hex grid (center + all rings up to max_radius) */
int hex_filled_grid(hex_coord_t center, int max_radius,
                    hex_coord_t *result, int max_results);

/* ================================================================
 * L2: Frequency reuse planning
 * ================================================================ */

/** Validate frequency reuse plan: N = i^2 + i*j + j^2 */
int freq_reuse_validate(int i, int j, int *n_out);

/** Compute D/R ratio for reuse factor N: D/R = sqrt(3N) */
double freq_reuse_d_over_r(int n);

/** Compute worst-case downlink SIR for reuse factor N,
 *  path loss exponent gamma, with n_interf interferers.
 *  SIR = (1/n_interf) * (D/R)^gamma
 */
double freq_reuse_worst_sir_db(int n_reuse, double path_loss_exp,
                               int n_interferers);

/** Assign frequency groups to cells in a hexagonal cluster */
int freq_reuse_assign_groups(cell_cluster_t *cluster, int n_reuse);

/* ================================================================
 * L2: Interference computation
 * ================================================================ */

/** Compute co-channel interference from all co-channel cells */
double compute_cochannel_interference_dbm(cell_cluster_t *cluster,
    int victim_idx, double path_loss_exp);

/** Build full interference matrix for a cluster */
int build_interference_matrix(cell_cluster_t *cluster,
    double path_loss_exp, interference_matrix_t *imat);

/* ================================================================
 * L2: NRT management
 * ================================================================ */

/** Initialize empty NRT */
void nrt_init(nrt_table_t *nrt);

/** Add or update neighbor relation */
void nrt_add(nrt_table_t *nrt, uint32_t src, uint32_t tgt,
             ho_type_t ho_type, int x2_avail);

/** Find neighbor relation entry */
nrt_entry_t *nrt_find(nrt_table_t *nrt, uint32_t src, uint32_t tgt);

/** Update HO statistics for a cell pair */
void nrt_update_ho_stats(nrt_table_t *nrt, uint32_t src, uint32_t tgt,
                          int success, double rsrp);

#endif /* CELL_NETWORK_MODEL_H */
