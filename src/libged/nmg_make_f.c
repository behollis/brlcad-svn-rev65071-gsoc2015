/*                         N M G _ M A K E _ F. C
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
/** @file libged/nmg_make_v.c
 *
 * The make F subcommand for nmg top-level command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_private.h"

struct tmp_v {
    struct vertex *vp;
};

struct faceuse*
make_face(const struct model* m, long int* v_ids, struct vertex*** vt, int num_verts)
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
    int idx;
    int found_idx;
    struct tmp_v* verts;

    NMG_CK_MODEL(m);

    verts = (struct tmp_v *)NULL;
    verts = (struct tmp_v *)bu_calloc(num_verts,
            sizeof(struct tmp_v), "verts");

    found_idx = 0;
    for (idx = 0; idx < num_verts; idx++) {
        for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
            NMG_CK_REGION(r);

            if (r->ra_p) {
                NMG_CK_REGION_A(r->ra_p);
            }

            for (BU_LIST_FOR(s, shell, &r->s_hd)) {
                NMG_CK_SHELL(s);

                if (s->sa_p) {
                    NMG_CK_SHELL_A(s->sa_p);
                }

                /* Faces in shell */
                for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
                    NMG_CK_FACEUSE(fu);
                    f = fu->f_p;
                    NMG_CK_FACE(f);

                    if (f->g.magic_p) switch (*f->g.magic_p) {
                        case NMG_FACE_G_PLANE_MAGIC:
                            break;
                        case NMG_FACE_G_SNURB_MAGIC:
                            break;
                    }

                    /* Loops in face */
                    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
                        NMG_CK_LOOPUSE(lu);
                        l = lu->l_p;
                        NMG_CK_LOOP(l);

                        if (l->lg_p) {
                            NMG_CK_LOOP_G(l->lg_p);
                        }

                        if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
                            /* Loop of Lone vertex */
                            vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);

                            /* check and remove vertexuse */
                            NMG_CK_VERTEXUSE(vu);
                            v = vu->v_p;
                            NMG_CK_VERTEX(v);

                            if ( v_ids[idx] == v->index ) {
                                if ( idx == found_idx ) {
                                    NMG_CK_VERTEX(v);
                                    NMG_CK_VERTEX_G(v->vg_p);
                                    verts[idx].vp = v;
                                    vt[idx] = &verts[idx].vp;
                                    found_idx++;
                                }
                            }

                            continue;
                        }

                        for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
                            NMG_CK_EDGEUSE(eu);
                            e = eu->e_p;
                            NMG_CK_EDGE(e);

                            if (eu->g.magic_p) {
                                switch (*eu->g.magic_p) {
                                case NMG_EDGE_G_LSEG_MAGIC:
                                    break;
                                case NMG_EDGE_G_CNURB_MAGIC:
                                    break;
                                }
                            }

                            vu = eu->vu_p;

                            /* check and remove vertexuse */
                            NMG_CK_VERTEXUSE(vu);
                            v = vu->v_p;
                            NMG_CK_VERTEX(v);

                            if ( v_ids[idx] == v->index ) {
                                if ( idx == found_idx ) {
                                    NMG_CK_VERTEX(v);
                                    NMG_CK_VERTEX_G(v->vg_p);
                                    verts[idx].vp = v;
                                    vt[idx] = &verts[idx].vp;
                                    found_idx++;
                                }
                            }
                        }
                    }
                }

                /* Wire loops in shell */
                for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
                    NMG_CK_LOOPUSE(lu);
                    l = lu->l_p;
                    NMG_CK_LOOP(l);

                    if (l->lg_p) {
                        NMG_CK_LOOP_G(l->lg_p);
                    }

                    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
                        /* Wire loop of Lone vertex */
                        vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
                        /* check and remove vertexuse */
                        NMG_CK_VERTEXUSE(vu);
                        v = vu->v_p;

                        if ( v_ids[idx] == v->index ) {
                            if ( idx == found_idx ) {
                                NMG_CK_VERTEX(v);
                                NMG_CK_VERTEX_G(v->vg_p);
                                verts[idx].vp = v;
                                vt[idx] = &verts[idx].vp;
                                found_idx++;
                            }
                        }

                        continue;
                    }

                    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
                        NMG_CK_EDGEUSE(eu);
                        e = eu->e_p;
                        NMG_CK_EDGE(e);

                        if (eu->g.magic_p) switch (*eu->g.magic_p) {
                            case NMG_EDGE_G_LSEG_MAGIC:
                            break;
                            case NMG_EDGE_G_CNURB_MAGIC:
                            break;
                        }
                        vu = eu->vu_p;

                        /* check and remove vertexuse */
                        NMG_CK_VERTEXUSE(vu);
                        v = vu->v_p;
                        NMG_CK_VERTEX(v);

                        if ( v_ids[idx] == v->index ) {
                            if ( idx == found_idx ) {
                                NMG_CK_VERTEX(v);
                                NMG_CK_VERTEX_G(v->vg_p);
                                verts[idx].vp = v;
                                vt[idx] = &verts[idx].vp;
                                found_idx++;
                            }
                        }
                    }
                }

                /* Wire edges in shell */
                for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
                    NMG_CK_EDGEUSE(eu);
                    e = eu->e_p;
                    NMG_CK_EDGE(e);

                    if (eu->g.magic_p) {
                        switch (*eu->g.magic_p) {
                        case NMG_EDGE_G_LSEG_MAGIC:
                            break;
                        case NMG_EDGE_G_CNURB_MAGIC:
                            break;
                        }
                    }

                    vu = eu->vu_p;

                    /* check and remove vertexuse */
                    NMG_CK_VERTEXUSE(vu);
                    v = vu->v_p;
                    NMG_CK_VERTEX(v);

                    if ( v_ids[idx] == v->index ) {
                        if ( idx == found_idx ) {
                            NMG_CK_VERTEX(v);
                            NMG_CK_VERTEX_G(v->vg_p);
                            verts[idx].vp = v;
                            vt[idx] = &verts[idx].vp;
                            found_idx++;
                        }
                    }
                }

                /* Lone vertex in shell */
                vu = s->vu_p;

                if (vu) {
                    /* check and remove vertexuse */
                    NMG_CK_VERTEXUSE(vu);
                    v = vu->v_p;

                    if ( v_ids[idx] == v->index ) {
                        if ( idx == found_idx ) {
                            NMG_CK_VERTEX(v);
                            NMG_CK_VERTEX_G(v->vg_p);
                            verts[idx].vp = v;
                            vt[idx] = &verts[idx].vp;
                            found_idx++;
                        }
                    }
                }
            }
        }
    }

    fu = NULL;
    if ( found_idx == num_verts ) {
        r = BU_LIST_FIRST(nmgregion, &m->r_hd);
        s = BU_LIST_FIRST(shell, &r->s_hd);

        NMG_CK_REGION(r);
        NMG_CK_SHELL(s);

        fu = nmg_cmface( s, vt, num_verts );
    }

    bu_free((char *) verts, "verts");

    return fu;
}

int
ged_nmg_make_f(struct ged* gedp, int argc, const char* argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    struct vertex*** vstructs;
    long int v_ids[50] = {0}; /* default max of 50 vertices per face */
    int num_verts;
    struct faceuse* fu;
    struct shell* s;
    struct nmgregion* r;
    struct bn_tol tol;
    int idx;

    static const char *usage = "make F v0 v1 ... vn";

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

    /* check for less than three vertices per face */
    if (argc < 5 ) {
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

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    if (BU_LIST_IS_EMPTY(&m->r_hd)) {
        r = nmg_mrsv(m);
        s = BU_LIST_FIRST(shell, &r->s_hd);
    } else {
        r = BU_LIST_FIRST(nmgregion, &m->r_hd);
        s = BU_LIST_FIRST(shell, &r->s_hd);
    }

    NMG_CK_REGION(r);
    NMG_CK_SHELL(s);

    /* process faces */
    num_verts = 0;
    for (idx = 3; idx < argc; idx++) {
        if ( ! BU_STR_EQUAL( "F", argv[idx] ) ) {
            v_ids[num_verts++] = atof(argv[idx]);
        }
        if ( BU_STR_EQUAL( "F", argv[idx] )|| idx == argc - 1 ) {
            if ( num_verts >= 3 ) {
				vstructs = (struct vertex ***) bu_calloc( num_verts,
									   sizeof(struct vertex **), "face_verts");
				fu = make_face(m, v_ids, vstructs, num_verts);
				bu_free((char *) vstructs, "face_verts");

				if ( fu ) {
					/* assign face geometry */
					if (s) {
					   for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
						   if (fu->orientation != OT_SAME) continue;
						   nmg_calc_face_g(fu);
					   }
					}

					tol.magic = BN_TOL_MAGIC;
					tol.dist = 0.0005;
					tol.dist_sq = tol.dist * tol.dist;
					tol.perp = 1e-6;
					tol.para = 1 - tol.perp;

					nmg_rebound(m, &tol);
				}
				num_verts = 0;
            } else {
            	num_verts = 0;
            }
    	}
    }

    if ( wdb_put_internal(gedp->ged_wdbp, name, &internal, 1.0) < 0 ) {
        bu_vls_printf(gedp->ged_result_str, "wdb_put_internal(%s)", argv[1]);
        rt_db_free_internal(&internal);
        return GED_ERROR;
    }

    rt_db_free_internal(&internal);

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
