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

    curr_r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    NMG_CK_REGION(curr_r);
    curr_s = BU_LIST_FIRST(shell, &curr_r->s_hd);
    NMG_CK_SHELL(curr_s);

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

            curr_eu = BU_LIST_FIRST(edgeuse, &curr_s->eu_hd);

            NMG_CK_EDGEUSE(curr_eu);
            NMG_TEST_EDGEUSE(curr_eu);

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
