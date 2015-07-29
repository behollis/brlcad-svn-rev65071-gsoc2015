/*                         N M G _ K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/nmg_kill.c
 *
 * The kill V subcommand for nmg top-level command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_private.h"

/**
 * Returns -
 * Pointer to magic-number/structure-base pointer array,
 * indexed by nmg structure index.
 * Caller is responsible for freeing it.
 */
uint32_t **
nmg_m_struct_count2(register struct nmg_struct_counts *ctr, const struct model *m)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct loopuse *lu;
    struct loop *l;
    struct edgeuse *eu;
    struct edge *e;
    struct vertexuse *vu;
    struct vertex *v;
    register uint32_t **ptrs;

#define NMG_UNIQ_INDEX(_p, _type)   \
    if ((_p)->index > m->maxindex) { \
    bu_log("%p (%s) has index %ld, m->maxindex=%ld\n", (void *)(_p), \
           bu_identify_magic(*((uint32_t *)(_p))), (_p)->index, m->maxindex); \
    bu_bomb("nmg_m_struct_count index overflow\n"); \
    } \
    if (ptrs[(_p)->index] == NULL) { \
    ptrs[(_p)->index] = (uint32_t *)(_p); \
    ctr->_type++; \
    }

#define UNIQ_VU(_vu) { \
    NMG_CK_VERTEXUSE(_vu); \
    NMG_UNIQ_INDEX(_vu, vertexuse); \
    if (_vu->a.magic_p) switch (*_vu->a.magic_p) { \
        case NMG_VERTEXUSE_A_PLANE_MAGIC: \
            NMG_UNIQ_INDEX(_vu->a.plane_p, vertexuse_a_plane); \
            break; \
        case NMG_VERTEXUSE_A_CNURB_MAGIC: \
            NMG_UNIQ_INDEX(_vu->a.cnurb_p, vertexuse_a_cnurb); \
            break; \
        } \
    v = _vu->v_p; \
    NMG_CK_VERTEX(v); \
    NMG_UNIQ_INDEX(v, vertex); \
    if (v->vg_p) { \
        NMG_CK_VERTEX_G(v->vg_p); \
        NMG_UNIQ_INDEX(v->vg_p, vertex_g); \
    } \
    }

    NMG_CK_MODEL(m);
    memset((char *)ctr, 0, sizeof(*ctr));

    ptrs = (uint32_t **)bu_calloc(m->maxindex+1, sizeof(uint32_t *), "nmg_m_count ptrs[]");

    NMG_UNIQ_INDEX(m, model);
    ctr->max_structs = m->maxindex;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
    NMG_CK_REGION(r);
    NMG_UNIQ_INDEX(r, region);
    if (r->ra_p) {
        NMG_CK_REGION_A(r->ra_p);
        NMG_UNIQ_INDEX(r->ra_p, region_a);
    }
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
        NMG_CK_SHELL(s);
        NMG_UNIQ_INDEX(s, shell);
        if (s->sa_p) {
        NMG_CK_SHELL_A(s->sa_p);
        NMG_UNIQ_INDEX(s->sa_p, shell_a);
        }
        /* Faces in shell */
        for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
        NMG_CK_FACEUSE(fu);
        NMG_UNIQ_INDEX(fu, faceuse);
        f = fu->f_p;
        NMG_CK_FACE(f);
        NMG_UNIQ_INDEX(f, face);
        if (f->g.magic_p) switch (*f->g.magic_p) {
            case NMG_FACE_G_PLANE_MAGIC:
                NMG_UNIQ_INDEX(f->g.plane_p, face_g_plane);
                break;
            case NMG_FACE_G_SNURB_MAGIC:
                NMG_UNIQ_INDEX(f->g.snurb_p, face_g_snurb);
                break;
            }
        /* Loops in face */
        for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
            NMG_CK_LOOPUSE(lu);
            NMG_UNIQ_INDEX(lu, loopuse);
            l = lu->l_p;
            NMG_CK_LOOP(l);
            NMG_UNIQ_INDEX(l, loop);
            if (l->lg_p) {
            NMG_CK_LOOP_G(l->lg_p);
            NMG_UNIQ_INDEX(l->lg_p, loop_g);
            }
            if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
            /* Loop of Lone vertex */
            ctr->face_lone_verts++;
            vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
            UNIQ_VU(vu);
            continue;
            }
            ctr->face_loops++;
            for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
            ctr->face_edges++;
            NMG_CK_EDGEUSE(eu);
            NMG_UNIQ_INDEX(eu, edgeuse);
            e = eu->e_p;
            NMG_CK_EDGE(e);
            NMG_UNIQ_INDEX(e, edge);
            if (eu->g.magic_p) switch (*eu->g.magic_p) {
                case NMG_EDGE_G_LSEG_MAGIC:
                    NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
                    break;
                case NMG_EDGE_G_CNURB_MAGIC:
                    NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
                    break;
                }
            vu = eu->vu_p;
            UNIQ_VU(vu);
            }
        }
        }
        /* Wire loops in shell */
        for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
        NMG_CK_LOOPUSE(lu);
        NMG_UNIQ_INDEX(lu, loopuse);
        l = lu->l_p;
        NMG_CK_LOOP(l);
        NMG_UNIQ_INDEX(l, loop);
        if (l->lg_p) {
            NMG_CK_LOOP_G(l->lg_p);
            NMG_UNIQ_INDEX(l->lg_p, loop_g);
        }
        if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
            ctr->wire_lone_verts++;
            /* Wire loop of Lone vertex */
            vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
            UNIQ_VU(vu);
            continue;
        }
        ctr->wire_loops++;
        for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
            NMG_CK_EDGEUSE(eu);
            NMG_UNIQ_INDEX(eu, edgeuse);
            e = eu->e_p;
            NMG_CK_EDGE(e);
            NMG_UNIQ_INDEX(e, edge);
            if (eu->g.magic_p) switch (*eu->g.magic_p) {
                case NMG_EDGE_G_LSEG_MAGIC:
                NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
                break;
                case NMG_EDGE_G_CNURB_MAGIC:
                NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
                break;
            }
            vu = eu->vu_p;
            UNIQ_VU(vu);
            ctr->wire_loop_edges++;
        }
        }
        /* Wire edges in shell */
        for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
        NMG_CK_EDGEUSE(eu);
        ctr->wire_edges++;
        NMG_UNIQ_INDEX(eu, edgeuse);
        e = eu->e_p;
        NMG_CK_EDGE(e);
        NMG_UNIQ_INDEX(e, edge);
        if (eu->g.magic_p) switch (*eu->g.magic_p) {
            case NMG_EDGE_G_LSEG_MAGIC:
                NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
                break;
            case NMG_EDGE_G_CNURB_MAGIC:
                NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
                break;
            }
        vu = eu->vu_p;
        UNIQ_VU(vu);
        }
        /* Lone vertex in shell */
        vu = s->vu_p;
        if (vu) {
        ctr->shells_of_lone_vert++;
        UNIQ_VU(vu);
        }
    }
    }
    /* Caller must free them */
    return ptrs;
#undef UNIQ_VU
}

int
ged_nmg_kill_v(struct ged* gedp, int argc, const char* argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    point_t v;
    struct nmgregion* curr_r;
    struct shell* curr_s;
    struct vertex_g* curr_vg;
    struct vertexuse* curr_vu;
    struct edgeuse* curr_eu;
    int found;
#if 0
    struct loopuse* curr_lu;
    struct bn_tol t;
    BN_TOL_INIT(&t)
#endif

    struct nmg_struct_counts counts;

    static const char *usage = "kill V coords";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 6) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[0];

    if ( (dp=db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET))
        == RT_DIR_NULL ) {
        bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", name);
        return GED_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, gedp->ged_wdbp->dbip,
        bn_mat_identity, &rt_uniresource) < 0) {
        bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
        return GED_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
        bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
        rt_db_free_internal(&internal);
        return GED_ERROR;
    }

    v[0] = atof(argv[3]); v[1] = atof(argv[4]); v[2] = atof(argv[5]);

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    nmg_m_struct_count2(&counts, m);


#if 0

#if 0
    curr_r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    NMG_CK_REGION(curr_r);

    curr_s = BU_LIST_FIRST(shell, &curr_r->s_hd);
    NMG_CK_SHELL(curr_s);
#endif

    curr_r = (struct nmgregion*)&m->r_hd;
    curr_s = (struct shell*)&curr_r->s_hd;

    /* Finds a vertex with coordinates given on CLI. Traverses all parent
     * regions and shells. Checks all edgeuses for the vertex structure.
     */
    found = 0;
    do { /* traverse a region */
        do { /* traverse a shell */

            /* check the vertexuse for the current shell */
            curr_vu = curr_s->vu_p;
            if ( curr_vu ) {
                if ( curr_vu->v_p ) {
                    /* assumes struct vertex has geom coord */
                    curr_vg = curr_vu->v_p->vg_p;

                    if ( VNEAR_EQUAL(curr_vg->coord, v, BN_TOL_DIST) ) {
                        nmg_kvu(curr_vu);
                        found = 1;
                    }
                }
            }

            curr_eu = (struct edgeuse*)&curr_s->eu_hd;

#if 0
            NMG_CK_EDGEUSE(curr_eu);
            NMG_TEST_EDGEUSE(curr_eu);
#endif
            /* traverse the edgeuses for the current shell */
            do { /* traverse an edgeuse */
                curr_vu = curr_eu->vu_p;
                NMG_CK_VERTEXUSE(curr_vu);
                if ( curr_vu ) {
                    if ( curr_vu->v_p ) {
                        /* assumes struct vertex has geom coord */
                        curr_vg = curr_vu->v_p->vg_p;
                        if ( VNEAR_EQUAL(curr_vg->coord, v, BN_TOL_DIST) ) {
                            nmg_kvu(curr_vu);
                            found = 1;
                        }
                    }
                }
            } while ( (curr_eu = BU_LIST_PNEXT(edgeuse, curr_eu))
                    != (struct edgeuse*)&curr_s->eu_hd );

        } while ( (curr_s = BU_LIST_PNEXT(shell, curr_s))
                != (struct shell*)&curr_r->s_hd );
    } while ( (curr_r = BU_LIST_PNEXT(nmgregion, curr_r))
            != (struct nmgregion*)&m->r_hd );

    if ( !found ) {
        bu_vls_printf(gedp->ged_result_str, "Vertex not found.");
        return GED_HELP;
    }

#endif

    return GED_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
