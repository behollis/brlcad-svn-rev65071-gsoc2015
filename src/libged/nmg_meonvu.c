/*                         N M G _ M E O N V U . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/nmg_meonvu.c
 *
 * The nmg_meonvu command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_private.h"

int
ged_nmg_meonvu(struct ged *gedp, int argc, const char *argv[])
{
    /* user can specify a vertex coord on CLI. routine searches for
     * the first encountered vertexuse that uses a vertex instance
     * with the coordinates specified. note: this is not what the
     * internal api function is meant to do. it uses a *vertexuse*. todo: pass
     * a *vertexuse* if possible from the CLI, or find another way to define
     * similar construction routine that is more suitable for the CLI.
     * once the vertexuse / vertex / geom is found, an "empty" edge with the
     * same topological vertex(use?) is inserted into the model.
     */

    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    struct nmgregion* r;
    struct shell* s;
    point_t v;
    struct nmgregion* curr_r;
    struct shell* curr_s;
    struct vertex_g* curr_vg;
    struct vertexuse* curr_vu;
    int idx;
    int found;

    static const char *usage = "nmg_name x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 5) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[1];

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

    v[0] = atof(argv[2]); v[1] = atof(argv[3]); v[2] = atof(argv[4]);

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    NMG_CK_REGION(r);
    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    /* Finds a vertex with coordinates given on CLI. Searches on vertexuses
     * that are contained in a shell. todo: this should probably be
     * expanded to search for vertex / vertexuse over all parent struct's in
     * the model.
     */
    found = 0;
    curr_r = (struct nmgregion*)&m->r_hd;
    curr_s = (struct shell*)&r->s_hd;
    while ((curr_r = BU_LIST_PNEXT(nmgregion, curr_r))
            != (struct nmgregion*)&m->r_hd) {
        while ((curr_s = BU_LIST_PNEXT(shell, curr_s))
                != (struct shell*)&r->s_hd) {
            curr_vu = curr_s->vu_p;
            if ( curr_vu ) {
                if ( curr_vu->v_p ) {
                    /* assumes struct vertex has geom coord */
                    curr_vg = curr_vu->v_p->vg_p;
                    if ( curr_vg->coord[0] == v[0] && curr_vg->coord[1] == v[1]
                       && curr_vg->coord[2] == v[2] ) {
                            found = 1;
                            break; /* found vertex geom */
                    }
                }
            }
        }
        if (found) break;
    }

    if ( !found ) {
        bu_vls_printf(gedp->ged_result_str, "Vertex not found.");
        return GED_HELP;
    }

    nmg_meonvu(curr_vu);

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
