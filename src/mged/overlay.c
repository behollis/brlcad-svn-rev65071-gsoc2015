/*                       O V E R L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/overlay.c
 *
 */

#include "common.h"

#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "raytrace.h"
#include "bn/vlist.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* Usage:  overlay file.plot3 [name] */
int
cmd_overlay(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;
    int ac;
    const char *av[5];
    struct bu_vls char_size = BU_VLS_INIT_ZERO;

    if (gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    if (argc == 1) {
	Tcl_DStringAppend(&ds, "file.plot3 [name]", -1);
	Tcl_DStringResult(interp, &ds);
	return TCL_OK;
    }

    ac = argc + 1;

    bu_vls_printf(&char_size, "%lf", view_state->vs_gvp->gv_scale * 0.01);
    av[0] = argv[0];		/* command name */
    av[1] = argv[1];		/* plotfile name */
    av[2] = bu_vls_addr(&char_size);
    if (argc == 3) {
	av[3] = argv[2];	/* name */
	av[4] = (char *)0;
    } else
	av[3] = (char *)0;

    ret = ged_overlay(gedp, ac, (const char **)av);
    Tcl_DStringAppend(&ds, bu_vls_addr(gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    update_views = 1;

    bu_vls_free(&char_size);
    return ret;
}

void
insert_index_label( struct bu_list* v_list, struct vertex* vert )
{
    /* Copies data from struct vertex and
     * insert into bu_list of index labels.
     */

    struct vtxlabel* v_info =
            (struct vtxlabel*)bu_malloc(sizeof(struct vtxlabel), "vtxlabel");
    v_info->coord[0] = vert->vg_p->coord[0];
    v_info->coord[1] = vert->vg_p->coord[1];
    v_info->coord[2] = vert->vg_p->coord[2];
    v_info->index = vert->index;

    BU_LIST_INSERT( v_list, &v_info->l);
}

void
free_index_label_list( struct bu_list* v_list )
{
    struct vtxlabel* curr_vl;
    while (BU_LIST_WHILE(curr_vl, vtxlabel, v_list)) {
        BU_LIST_DEQUEUE(&(curr_vl->l));
        bu_free(curr_vl, "vtxlabel");
    }
}

void
get_vertex_list( const struct model* m, struct bu_list* v_list )
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
    struct vtxlabel *curr_v;
    int found = 0;

    NMG_CK_MODEL(m);

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

                        /* check vertexuse */
                        NMG_CK_VERTEXUSE(vu);
                        v = vu->v_p;
                        NMG_CK_VERTEX(v);

                        /* check for duplicate vertex struct */
                        for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                            if (curr_v->index == v->index) {
                                found = 1;
                                break;
                            }
                        }

                        if ( !found ) {
                            insert_index_label( v_list, v );
                        }

                        found = 0;
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

                        /* check vertexuse */
                        NMG_CK_VERTEXUSE(vu);
                        v = vu->v_p;
                        NMG_CK_VERTEX(v);

                        /* check for duplicate vertex struct */
                        for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                            if (curr_v->index == v->index) {
                                found = 1;
                                break;
                            }
                        }

                        if ( !found ) {
                            insert_index_label( v_list, v );
                        }

                        found = 0;
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
                    NMG_CK_VERTEX(v);

                    /* check for duplicate vertex struct */
                    for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                        if (curr_v->index == v->index) {
                            found = 1;
                            break;
                        }
                    }

                    if ( !found ) {
                        insert_index_label( v_list, v );
                    }

                    found = 0;
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

                    /* check vertexuse */
                    NMG_CK_VERTEXUSE(vu);
                    v = vu->v_p;
                    NMG_CK_VERTEX(v);

                    /* check for duplicate vertex struct */
                    for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                        if (curr_v->index == v->index) {
                            found = 1;
                            break;
                        }
                    }

                    if ( !found ) {
                        insert_index_label( v_list, v );
                    }

                    found = 0;
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

                /* check vertexuse */
                NMG_CK_VERTEXUSE(vu);
                v = vu->v_p;
                NMG_CK_VERTEX(v);

                /* check for duplicate vertex struct */
                for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                    if (curr_v->index == v->index) {
                        found = 1;
                        break;
                    }
                }

                if ( !found ) {
                    insert_index_label( v_list, v );
                    found = 0;
                }
            }

            /* Lone vertex in shell */
            vu = s->vu_p;

            if (vu) {
                /* check vertexuse */
                NMG_CK_VERTEXUSE(vu);
                v = vu->v_p;
                NMG_CK_VERTEX(v);

                /* check for duplicate vertex struct */
                for (BU_LIST_FOR(curr_v, vtxlabel, v_list)) {
                    if (curr_v->index == v->index) {
                        found = 1;
                        break;
                    }
                }

                if ( !found ) {
                    insert_index_label( v_list, v );
                    found = 0;
                }
            }
        }
    }
}

/* Usage:  labelvert solid(s) */
int
f_labelvert(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bn_vlblock*vbp;
    mat_t mat;
    fastf_t scale;
    char opt[2] = {'c','\0'};
    int i=1;
    struct bu_list v_list;
    struct model* m;
    const char* name;

    BU_LIST_INIT( &v_list );

    CHECK_DBI_NULL;

    if (argc < 2) {
        struct bu_vls vls = BU_VLS_INIT_ZERO;

        bu_vls_printf(&vls, "help labelvert");
        Tcl_Eval(interp, bu_vls_addr(&vls));
        bu_vls_free(&vls);
        return TCL_ERROR;
    }

    if (argc > 2) {
        i = 2;
        opt[0] = argv[1][1];
        if ( !BU_STR_EQUIV( "c", opt ) && !BU_STR_EQUIV( "i", opt ) ) {
            struct bu_vls vls = BU_VLS_INIT_ZERO;

            bu_vls_printf(&vls, "help labelvert");
            Tcl_Eval(interp, bu_vls_addr(&vls));
            bu_vls_free(&vls);
            return TCL_ERROR;
        }

        if ( BU_STR_EQUIV( "i", opt ) ) {

            /* attempt to resolve and verify */
            name = argv[2];

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
        }
    }

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;		/* divide by # chars/screen */

    if ( BU_STR_EQUIV( "i", opt ) ) {
        get_vertex_list(m, &v_list);
        rt_label_vidx_verts(vbp, &v_list, mat, scale, base2local);
        free_index_label_list( &v_list );
    } else {
        for (; i<argc; i++) {
        struct solid *s;
        if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
            continue;
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
        }
    }

    cvt_vlblock_to_solids(vbp, "_LABELVERT_", 0);

    bn_vlblock_free(vbp);
    update_views = 1;
    return TCL_OK;
}

void
get_face_list( const struct model* m, struct bu_list* f_list )
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct face* curr_f;
    int found;

    NMG_CK_MODEL(m);

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

                found = 0;
                /* check for duplicate face struct */
                for (BU_LIST_FOR(curr_f, face, f_list)) {
                    if (curr_f->index == f->index) {
                        found = 1;
                        break;
                    }
                }

                if ( !found )
                    BU_LIST_INSERT( f_list, &(f->l) );

                if (f->g.magic_p) switch (*f->g.magic_p) {
                    case NMG_FACE_G_PLANE_MAGIC:
                        break;
                    case NMG_FACE_G_SNURB_MAGIC:
                        break;
                }

            }
        }
    }
}

/* Usage:  labelface solid(s) */
int
f_labelface(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bn_vlblock *vbp;
    mat_t mat;
    fastf_t scale;
    struct model* m;
    const char* name;
    struct bu_list f_list;

    BU_LIST_INIT( &f_list );

    CHECK_DBI_NULL;

    if (argc < 2) {
        struct bu_vls vls = BU_VLS_INIT_ZERO;

        bu_vls_printf(&vls, "help labelface");
        Tcl_Eval(interp, bu_vls_addr(&vls));
        bu_vls_free(&vls);
        return TCL_ERROR;
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

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;      /* divide by # chars/screen */

    for (i=1; i<argc; i++) {
        struct solid *s;
        if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
            continue;

        /* Find uses of this solid in the solid table */
        gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
        while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
            next_gdlp = BU_LIST_PNEXT(display_list, gdlp);
            FOR_ALL_SOLIDS(s, &gdlp->dl_headSolid) {
                if (db_full_path_search(&s->s_fullpath, dp)) {
                    get_face_list(m, &f_list);
                    rt_label_vlist_faces(vbp, &f_list, mat, scale, base2local);
                }
            }

            gdlp = next_gdlp;
        }
    }

    cvt_vlblock_to_solids(vbp, "_LABELFACE_", 0);

    bn_vlblock_free(vbp);
    update_views = 1;
    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
