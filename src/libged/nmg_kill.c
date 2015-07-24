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
#include "vmath.h"
#include "raytrace.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "../mged/mged.h"
#include "../mged/sedit.h"
#include "../mged/mged_dm.h"

#include "./ged_private.h"

#if 0
#include <math.h>
#include <signal.h>

#include "vmath.h"


#endif

int labelvert();

int
ged_nmg_kill_v(struct ged * gedp, int UNUSED(argc), const char* argv[])
{
#if 0
    struct directory *dp;
    static const char *usage = "kill V vertex-list";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_HELP;
    }
#endif

    labelvert(gedp,argv[0]);

#if 0
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "fn")) != -1) {
    switch (c) {
        case 'f':
        force = 1;
        break;
        case 'n':
        nflag = 1;
        break;
        default:
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_ERROR;
    }
    }

    if ((force + nflag) > 1) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (nflag) {
    bu_vls_printf(gedp->ged_result_str, "{");
    for (i = 1; i < argc; i++)
        bu_vls_printf(gedp->ged_result_str, "%s ", argv[i]);
    bu_vls_printf(gedp->ged_result_str, "} {}");

    return GED_OK;
    }

    for (i = 1; i < argc; i++) {
    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], verbose)) != RT_DIR_NULL) {
        if (!force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
        bu_vls_printf(gedp->ged_result_str, "You attempted to delete the _GLOBAL object.\n");
        bu_vls_printf(gedp->ged_result_str, "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n");
        bu_vls_printf(gedp->ged_result_str, "\tsuch as your preferred units and the title of the database.\n");
        bu_vls_printf(gedp->ged_result_str, "\tUse the \"-f\" option, if you really want to do this.\n");
        continue;
        }

        is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

        /* don't worry about phony objects */
        if (is_phony)
        continue;

        _dl_eraseAllNamesFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, argv[i], 0, gedp->freesolid);

        if (db_delete(gedp->ged_wdbp->dbip, dp) != 0 || db_dirdelete(gedp->ged_wdbp->dbip, dp) != 0) {
        /* Abort kill processing on first error */
        bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[i]);
        return GED_ERROR;
        }
    }
    }
#endif

    return GED_OK;
}

/* Usage:  labelvert solid(s) */
int
labelvert(struct ged * gedp, char* obj)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bn_vlblock*vbp;
    struct directory *dp;
    mat_t mat;
    fastf_t scale;
    struct solid *s;

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;      /* divide by # chars/screen */

    /* Find uses of this solid in the solid table */
    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
            next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

            FOR_ALL_SOLIDS(s, &gdlp->dl_headSolid) {
            if (db_full_path_search(&s->s_fullpath, dp)) {
                rt_label_vlist_verts(vbp, &s->s_vlist, mat, scale, base2local);
            }
        }

        gdlp = next_gdlp;
    }

#if 0
    cvt_vlblock_to_solids(vbp, "_LABELVERT_", 0);
#endif

    bn_vlblock_free(vbp);

#if 0
    update_views = 1;
#endif
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
