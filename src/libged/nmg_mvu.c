/*                         N M G _ M V U . C
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
/** @file libged/nmg_mvu.c
 *
 * The nmg_mvu command.
 *
 */

#include "common.h"
#include "nmg.h"

#include <signal.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_private.h"

int
ged_nmg_mvu(struct ged *gedp, int argc, const char *argv[])
{
	struct rt_db_internal internal;
	struct directory *dp;
	struct model* m;
	const char* name;
	struct nmgregion* r;
	struct shell* s;
	struct vertexuse* vu;
	struct vertex_g* vg;
	point_t v;
	struct nmgregion* curr_r; struct shell* curr_s; struct vertex_g* curr_vg;
	int idx; int found;
	long* upptr;

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

	/* Need to find vertex with coordinates given on CLI. Use the parent struct
	 * of the found vertex as upptr. Here, this is a shell.
	 * TODO: the point of this routine is to re-assign a vertexuse
	 * to an *existing* vertex. Right now, this routine finds a shell
	 * with the specified vertex and replaces the vertexuse in that shell.
	 * we need to find a vertexuse *not* in a shell, such as an edge, and
	 * reassign it a *new* vertexuse. CLI doesn't use vertex id, so coord may
	 * be non-unique to multiple vertices in nmg model.
	 */
	found = 0;
	while (BU_LIST_WHILE(curr_r, nmgregion, &m->r_hd)) {
	    while (BU_LIST_WHILE(curr_s, shell, &r->s_hd)) {
	        curr_vg = curr_s->vu_p->v_p->vg_p;
	        if ( curr_vg ) {
				if ( curr_vg->coord[0] == v[0] && curr_vg->coord[1] == v[1]
				   && curr_vg->coord[2] == v[2] ) {
						found = 1;
						break; /* found vertex geom */
				}
    		}
    	}
    	if (found) break;
	}

    if ( !found ) {
    	bu_vls_printf(gedp->ged_result_str, "Vertex not found.");
    	return GED_HELP;
    }

	/* Allocated new vertexuse for the found vertex. */
    upptr = (long *)curr_s;
	vu = (struct vertexuse*)nmg_mvvu((long*)upptr, m);
	GET_VERTEX_G(vg, m); vu->v_p->vg_p = vg;

	for ( idx = 0; idx < ELEMENTS_PER_POINT; idx++ ) {
		vu->v_p->vg_p->coord[idx] = v[idx];
	}

	nmg_mvu(vu->v_p, upptr);

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
