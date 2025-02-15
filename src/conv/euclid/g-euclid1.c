/*                     G - E U C L I D 1 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 *
 */
/** @file g-euclid1.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a Euclid
 * "decoded" facetted model by calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include "bio.h"

/* interface headers */
#include "bu/parallel.h"
#include "bu/getopt.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

static char usage[] = "[-v] [-s alarm_seconds] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-P #_of_CPUs] brlcad_db.g object(s)\n";

static int NMG_debug;		/* saved arg of -X, for longjmp handling */
static int verbose;
static int ncpu = 1;		/* Number of processors */
static int face_count;		/* Count of faces output for a region id */
static int alarm_secs;		/* Number of seconds to allow for conversion, 0 means no limit */
static struct db_i *dbip;
static struct rt_tess_tol ttol;
static struct bn_tol tol;
static struct model *the_model;

static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;

struct facets
{
    struct loopuse *lu;
    struct loopuse *outer_loop;
    fastf_t diag_len;
    int facet_type;
};

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}

void
fastf_print(FILE *fp_out, size_t length, fastf_t f)
{
    char buffer[128];
    char *ptr;
    size_t i;
    size_t buf_len;

    sprintf(&buffer[1], "%f", f);
    buffer[0] = ' ';

    buf_len = strlen(buffer);
    if (buf_len <= length) {
	for (i = 0; i < length; i++) {
	    if (i < buf_len)
		fputc(buffer[i], fp_out);
	    else
		fputc(' ', fp_out);
	}

	return;
    }

    ptr = strchr(buffer, '.');
    if ((size_t)(ptr - buffer) > length) {
	bu_exit(1, "ERROR: Value (%f) too large for format length (%zu)\n", f, length);
    }

    for (i = 0; i < length; i++)
	fputc(buffer[i], fp_out);
}


/* only used with SIGALRM */
void
handler(int UNUSED(code))
{
    bu_exit(EXIT_FAILURE, "ALARM boolean evaluation aborted\n");
}


static void
Write_euclid_face(const struct loopuse *lu, const int facet_type, const int regionid, const int face_number, FILE *fp_out)
{
    struct faceuse *fu;
    struct edgeuse *eu;
    plane_t plane;
    int vertex_count = 0;

    NMG_CK_LOOPUSE(lu);

    if (verbose)
	bu_log("Write_euclid_face: lu=%p, facet_type=%d, regionid=%d, face_number=%d\n",
	       (void *)lu, facet_type, regionid, face_number);

    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return;

    if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
	return;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
	vertex_count++;

    fprintf(fp_out, "%10d%3d     0.    1%5d", regionid, facet_type, vertex_count);

    vertex_count = 0;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	struct vertex *v;
	int i;

	NMG_CK_EDGEUSE(eu);
	v = eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
/* fprintf(fp_out, "%10d%8f%8f%8f", ++vertex_count, V3ARGS(v->vg_p->coord)); */
	vertex_count++;
	fprintf(fp_out, "%10d", vertex_count);

	for (i=X; i<=Z; i++)
	    fastf_print(fp_out, 8, v->vg_p->coord[i]);
    }

    fu = lu->up.fu_p;
    NMG_CK_FACEUSE(fu);
    NMG_GET_FU_PLANE(plane, fu);
    fprintf(fp_out, "%10d%15.5f%15.5f%15.5f%15.5f", face_number, V4ARGS(plane));
}


/* Routine to write an nmgregion in the Euclid "decoded" format */
static void
Write_euclid_region(struct nmgregion *r, struct db_tree_state *tsp, FILE *fp_out)
{
    struct shell *s;
    struct facets *faces = NULL;
    int i, j;

    NMG_CK_REGION(r);

    if (verbose)
	bu_log("Write_euclid_region: r=%p\n", (void *)r);

    face_count = 0;

    /* if bounds haven't been calculated, do it now */
    if (r->ra_p == NULL)
	nmg_region_a(r, &tol);

    /* Check if region extents are beyond the limitations of the format */
    for (i = X; i < ELEMENTS_PER_POINT; i++) {
	if (r->ra_p->min_pt[i] < (-999999.0)) {
	    bu_log("g-euclid: Coordinates too large (%g) for Euclid format\n", r->ra_p->min_pt[i]);
	    return;
	}
	if (r->ra_p->max_pt[i] > 9999999.0) {
	    bu_log("g-euclid: Coordinates too large (%g) for Euclid format\n", r->ra_p->max_pt[i]);
	    return;
	}
    }

    /* write out each face in the region */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    int no_of_loops = 0;
	    int no_of_holes = 0;

	    if (fu->orientation != OT_SAME)
		continue;

	    /* count the loops in this face */
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		no_of_loops++;
	    }

	    if (!no_of_loops)
		continue;

	    faces = (struct facets *)bu_calloc(no_of_loops, sizeof(struct facets), "g-euclid: faces");

	    i = 0;
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		faces[i].lu = lu;
		if (lu->orientation == OT_OPPOSITE)
		    faces[i].facet_type = 1; /* this is a hole */
		else
		    faces[i].facet_type = (-1); /* TBD */

		faces[i].outer_loop = NULL;
		i++;
	    }

	    /* determine type of face
	     * 0 -> simple facet (no holes)
	     * 1 -> a hole
	     * 2 -> a facet that will have holes
	     */

	    for (i = 0; i < no_of_loops; i++) {
		if (faces[i].facet_type == 1)
		    no_of_holes++;
	    }

	    if (!no_of_holes) {
		/* no holes, so each loop is a simple face (type 0) */
		for (i = 0; i < no_of_loops; i++)
		    faces[i].facet_type = 0;
	    } else if (no_of_loops == no_of_holes + 1) {
		struct loopuse *outer_lu = NULL;

		/* only one outer loop, so find it */
		for (i = 0; i < no_of_loops; i++) {
		    if (faces[i].facet_type == (-1)) {
			outer_lu = faces[i].lu;
			faces[i].facet_type = 2;
			break;
		    }
		}

		/* every hole must have this same outer_loop */
		for (i = 0; i < no_of_loops; i++) {
		    if (faces[i].facet_type == 1)
			faces[i].outer_loop = outer_lu;
		}
	    } else {
		int loop1, loop2;
		int outer_loop_count;

		/* must determine which holes go with which outer loops */
		for (loop1 = 0; loop1 < no_of_loops; loop1++) {
		    if (faces[loop1].facet_type != 1)
			continue;

		    /* loop1 is a hole look for loops containing loop1 */
		    outer_loop_count = 0;
		    for (loop2 = 0; loop2 < no_of_loops; loop2++) {
			int nmg_class;

			if (faces[loop2].facet_type == 1)
			    continue;

			nmg_class = nmg_classify_lu_lu(faces[loop1].lu,
						       faces[loop2].lu, &tol);

			if (nmg_class != NMG_CLASS_AinB)
			    continue;

			/* loop1 is inside loop2, possible outer loop */
			faces[loop2].facet_type = (-2);
			outer_loop_count++;
		    }

		    if (outer_loop_count > 1) {
			/* must choose outer loop from a list of candidates
			 * if any of these candidates contain one of the
			 * other candidates, the outer one can be eliminated
			 * as a possible choice */
			for (loop2 = 0; loop2 < no_of_loops; loop2++) {
			    if (faces[loop2].facet_type != (-2))
				continue;

			    for (i = 0; i < no_of_loops; i++) {
				if (faces[i].facet_type != (-2))
				    continue;

				if (nmg_classify_lu_lu(faces[i].lu,
						       faces[loop2].lu, &tol)) {
				    if (faces[i].facet_type != (-2))
					continue;

				    faces[loop2].facet_type = (-1);
				    outer_loop_count--;
				}
			    }
			}
		    }

		    if (outer_loop_count != 1) {
			bu_log("Failed to find outer loop for hole in component %d\n", tsp->ts_regionid);
			goto outt;
		    }

		    for (i = 0; i < no_of_loops; i++) {
			if (faces[i].facet_type == (-2)) {
			    faces[i].facet_type = 2;
			    faces[loop1].outer_loop = faces[i].lu;
			}
		    }
		}

		/* Check */
		for (i = 0; i < no_of_loops; i++) {
		    if (faces[i].facet_type < 0) {
			/* all holes have been placed
			 * so these must be simple faces
			 */
			faces[i].facet_type = 0;
		    }

		    if (faces[i].facet_type == 1 && faces[i].outer_loop == NULL) {
			bu_log("Failed to find outer loop for hole in component %d\n", tsp->ts_regionid);
			goto outt;
		    }
		}
	    }
	    /* output faces with holes first */
	    for (i = 0; i < no_of_loops; i++) {
		struct loopuse *outer_loop;

		if (faces[i].facet_type != 2)
		    continue;

		outer_loop = faces[i].lu;
		Write_euclid_face(outer_loop, 2, tsp->ts_regionid, ++face_count, fp_out);

		/* output holes for this face */
		for (j = 0; j < no_of_loops; j++) {
		    if (j == i)
			continue;

		    if (faces[j].outer_loop == outer_loop)
			Write_euclid_face(faces[j].lu, 1, tsp->ts_regionid, ++face_count, fp_out);
		}
	    }
	    /* output simple faces */
	    for (i = 0; i < no_of_loops; i++) {
		if (faces[i].facet_type != 0)
		    continue;
		Write_euclid_face(faces[i].lu, 0, tsp->ts_regionid, ++face_count, fp_out);
	    }

	    bu_free((char *)faces, "g-euclid: faces");
	    faces = (struct facets*)NULL;
	}
    }

    regions_written++;

outt:
    if (faces)
	bu_free((char *)faces, "g-euclid: faces");
    return;
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);

    } else {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	RTG.NMG_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree, &rt_uniresource);/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	bu_free(name, "db_path_to_string");
	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    } BU_UNSETJUMP;/* Relinquish the protection */

    return ret_tree;
}


/*
 * Called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    FILE *fp_out;
    struct nmgregion *r;
    struct bu_list vhead;
    struct directory *dir;
    union tree *ret_tree;

    if (verbose)
	bu_log("do_region_end: regionid = %d\n", tsp->ts_regionid);

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried > 0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    dir = DB_FULL_PATH_CUR_DIR(pathp);
    if ((fp_out = fopen(dir->d_namep, "wb")) == NULL) {
	perror("g-euclid");
	bu_exit(1, "ERROR: Cannot open file %s\n", dir->d_namep);
    }

    bu_log("\n\nProcessing region %s:\n", dir->d_namep);

    regions_tried++;

    if (verbose)
	bu_log("\tEvaluating region\n");

    ret_tree = process_boolean(curtree, tsp, pathp);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    regions_converted++;
    if (r != (struct nmgregion *)NULL) {
	struct shell *s;
	int empty_region = 0;
	int empty_model = 0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s)) {
		if (nmg_ks(s)) {
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if (!empty_region) {
	    empty_model = nmg_kill_zero_length_edgeuses(*tsp->ts_m);
	}

	/* Write the region to the EUCLID file */
	if (!empty_region && !empty_model)
	    Write_euclid_region(r, tsp, fp_out);

	bu_log("Wrote region %s\n", dir->d_namep);

	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC)
	    nmg_km(*tsp->ts_m);
	else
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();

	rt_vlist_cleanup();
    }

    /*
     * Dispose of original tree, so that all associated dynamic
     * memory is released now, not at the end of all regions.
     * A return of TREE_NULL from this routine signals an error,
     * so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    /* close any output file */
    if (fp_out)
	fclose(fp_out);

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


int
main(int argc, char **argv)
{
    int c;
    double percent;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    the_model = (struct model *)NULL;
    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_m = &the_model;
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;

    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:n:r:s:vx:P:X:h?")) != -1) {
	switch (c) {
	    case 's':
		alarm_secs = atoi(bu_optarg);
		break;
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi(bu_optarg);
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&RTG.debug);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&RTG.NMG_debug);
		NMG_debug = RTG.NMG_debug;
		break;
	    default:
		print_usage(argv[0]);
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	print_usage(argv[0]);
    }

    /* Open BRL-CAD database */
    if ((dbip = db_open(argv[bu_optind], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open geometry database file %s\n", argv[bu_optind]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "db_dirbuild failed\n");
    }
    bu_optind++;

    /* Walk indicated tree(s).  Each region will be output separately */

    tree_state = rt_initial_tree_state;	/* struct copy */
    the_model = nmg_mm();
    tree_state.ts_m = &the_model;
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;

    (void)db_walk_tree(dbip, argc-bu_optind, (const char **)(&argv[bu_optind]),
		       ncpu,
		       &tree_state,
		       0,
		       do_region_end,
		       nmg_booltree_leaf_tess,
		       (void *)NULL);	/* in librt/nmg_bool.c */

    nmg_km(the_model);

    percent = 0;
    if (regions_tried > 0)
	percent = ((double)regions_converted * 100) / regions_tried;
    printf("Tried %d regions, %d converted successfully.  %g%%\n",
	   regions_tried, regions_converted, percent);
    percent = 0;
    if (regions_tried > 0)
	percent = ((double)regions_written * 100) / regions_tried;
    printf("                  %d written successfully. %g%%\n",
	   regions_written, percent);

    /* Release dynamic storage */
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
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
