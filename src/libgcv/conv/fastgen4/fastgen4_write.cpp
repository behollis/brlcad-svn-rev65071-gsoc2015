/*              F A S T G E N 4 _ W R I T E . C P P
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
/** @file fastgen4_write.cpp
 *
 * FASTGEN4 export plugin for libgcv.
 *
 */


#include "common.h"

#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

#include "../../plugin.h"


namespace
{


template <typename T> HIDDEN inline void
autofreeptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoFreePtr");
}


template <typename T, void free_fn(T *) = autofreeptr_wrap_bu_free>
struct AutoFreePtr {
    AutoFreePtr(T *vptr) : ptr(vptr) {}

    ~AutoFreePtr()
    {
	if (ptr) free_fn(ptr);
    }


    T * const ptr;


private:
    AutoFreePtr(const AutoFreePtr &source);
    AutoFreePtr &operator=(const AutoFreePtr &source);
};


class FastgenWriter
{
public:
    FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_comment(const std::string &value);


private:
    friend class Section;

    class Record;

    static const std::size_t MAX_GROUP_ID = 49;
    static const std::size_t MAX_SECTION_ID = 999;

    std::size_t m_next_section_id[MAX_GROUP_ID + 1];
    bool m_record_open;
    std::ofstream m_ostream, m_colors_ostream;
};


class FastgenWriter::Record
{
public:
    Record(FastgenWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &operator<<(fastf_t value);
    Record &non_zero(fastf_t value);
    Record &text(const std::string &value);


private:
    static const std::size_t FIELD_WIDTH = 8;
    static const std::size_t RECORD_WIDTH = 10;

    std::size_t m_width;
    FastgenWriter &m_writer;
};


FastgenWriter::Record::Record(FastgenWriter &writer) :
    m_width(0),
    m_writer(writer)
{
    if (m_writer.m_record_open)
	bu_bomb("logic error: record open");

    m_writer.m_record_open = true;
}


FastgenWriter::Record::~Record()
{
    if (m_width) m_writer.m_ostream.put('\n');

    m_writer.m_record_open = false;
}


template <typename T> FastgenWriter::Record &
FastgenWriter::Record::operator<<(const T &value)
{
    if (++m_width > RECORD_WIDTH)
	bu_bomb("logic error: invalid record width");

    std::ostringstream sstream;

    if (!(sstream << value))
	throw std::invalid_argument("failed to convert value");

    const std::string str_val = sstream.str();

    if (str_val.size() > FIELD_WIDTH)
	throw std::invalid_argument("length exceeds field width");

    m_writer.m_ostream << std::left << std::setw(FIELD_WIDTH) << str_val;
    return *this;
}


FastgenWriter::Record &
FastgenWriter::Record::operator<<(fastf_t value)
{
    std::ostringstream sstream;
    sstream << std::fixed << std::showpoint << value;
    return operator<<(sstream.str().substr(0, FIELD_WIDTH));
}


// ensure that the truncated value != 0.0
FastgenWriter::Record &
FastgenWriter::Record::non_zero(fastf_t value)
{
    std::ostringstream sstream;
    sstream << std::fixed << std::showpoint << value;

    std::string result = sstream.str().substr(0, FIELD_WIDTH);

    if (result.find_first_not_of("-0.") == std::string::npos) {
	result = "0.0";
	result.append(FIELD_WIDTH - result.size() - 1, '0');
	result.push_back('1');
    }

    return operator<<(result);
}


FastgenWriter::Record &
FastgenWriter::Record::text(const std::string &value)
{
    m_width = RECORD_WIDTH;
    m_writer.m_ostream << value;
    return *this;
}


FastgenWriter::FastgenWriter(const std::string &path) :
    m_record_open(false),
    m_ostream(path.c_str(), std::ofstream::out),
    m_colors_ostream((path + ".colors").c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    m_colors_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    for (std::size_t i = 0; i <= MAX_GROUP_ID; ++i)
	m_next_section_id[i] = 1;
}


FastgenWriter::~FastgenWriter()
{
    Record(*this) << "ENDDATA";
}


void
FastgenWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(" ").text(value);
}


class Section
{
public:
    Section(FastgenWriter &writer, std::string name, bool volume_mode,
	    const unsigned char *color = NULL);

    std::size_t add_grid_point(fastf_t x, fastf_t y, fastf_t z);

    void add_sphere(std::size_t g1, fastf_t thickness, fastf_t radius);

    void add_cone(std::size_t g1, std::size_t g2, fastf_t ro1, fastf_t ro2,
		  fastf_t ri1, fastf_t ri2);

    void add_line(std::size_t g1, std::size_t g2, fastf_t thickness,
		  fastf_t radius);

    void add_triangle(std::size_t g1, std::size_t g2, std::size_t g3,
		      fastf_t thickness, bool grid_centered = false);

    void add_hexahedron(const std::size_t *g);


private:
    static const fastf_t INCHES_PER_MM;
    static const std::size_t MAX_NAME_SIZE = 25;
    static const std::size_t MAX_GRID_POINTS = 50000;

    std::size_t m_next_grid_id;
    std::size_t m_next_element_id;
    bool m_volume_mode;
    FastgenWriter &m_writer;
};


const fastf_t Section::INCHES_PER_MM = 1.0 / 25.4;


Section::Section(FastgenWriter &writer, std::string name, bool volume_mode,
		 const unsigned char *color) :
    m_next_grid_id(1),
    m_next_element_id(1),
    m_volume_mode(volume_mode),
    m_writer(writer)
{
    std::size_t group_id = 0;

    while (m_writer.m_next_section_id[group_id] > FastgenWriter::MAX_SECTION_ID)
	if (++group_id > FastgenWriter::MAX_GROUP_ID)
	    throw std::range_error("group_id exceeds limit");

    std::size_t &next_section_id = m_writer.m_next_section_id[group_id];

    if (name.size() > MAX_NAME_SIZE) {
	m_writer.write_comment(name);
	name = "..." + name.substr(name.size() - MAX_NAME_SIZE + 3);
    }

    {
	FastgenWriter::Record record(m_writer);
	record << "$NAME" << group_id << next_section_id;
	record << "" << "" << "" << "";
	record.text(name);
    }

    FastgenWriter::Record(m_writer) << "SECTION" << group_id << next_section_id <<
				    (m_volume_mode ? 2 : 1);

    if (color) {
	writer.m_colors_ostream << next_section_id << ' '
				<< next_section_id << ' '
				<< static_cast<unsigned>(color[0]) << ' '
				<< static_cast<unsigned>(color[1]) << ' '
				<< static_cast<unsigned>(color[2]) << '\n';
    }

    ++next_section_id;
}


std::size_t
Section::add_grid_point(fastf_t x, fastf_t y, fastf_t z)
{
    x *= INCHES_PER_MM;
    y *= INCHES_PER_MM;
    z *= INCHES_PER_MM;

    if (m_next_grid_id > MAX_GRID_POINTS)
	throw std::length_error("maximum GRID records");

    if (m_next_element_id != 1)
	bu_bomb("logic error: add_grid_point() called after adding elements");

    FastgenWriter::Record record(m_writer);
    record << "GRID" << m_next_grid_id << "";
    record << x << y << z;
    return m_next_grid_id++;
}


void
Section::add_sphere(std::size_t g1, fastf_t thickness, fastf_t radius)
{
    thickness *= INCHES_PER_MM;
    radius *= INCHES_PER_MM;

    if (!g1 || g1 >= m_next_grid_id || thickness <= 0.0 || radius <= 0.0)
	throw std::invalid_argument("invalid value");

    FastgenWriter::Record record(m_writer);
    record << "CSPHERE" << m_next_element_id++ << 0 << g1 << "" << "" << "";
    record.non_zero(thickness).non_zero(radius);
}


void
Section::add_cone(std::size_t g1, std::size_t g2, fastf_t ro1, fastf_t ro2,
		  fastf_t ri1, fastf_t ri2)
{
    ro1 *= INCHES_PER_MM;
    ro2 *= INCHES_PER_MM;
    ri1 *= INCHES_PER_MM;
    ri2 *= INCHES_PER_MM;

    if (g1 == g2 || !g1 || !g2 || g1 >= m_next_grid_id || g2 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    if (ri1 < 0.0 || ri2 < 0.0 || ri1 >= ro1 || ri2 >= ro2)
	throw std::invalid_argument("invalid radius");

    FastgenWriter::Record(m_writer) << "CCONE2" << m_next_element_id << 0 << g1 <<
				    g2 << "" << "" << "" << ro1 << m_next_element_id;
    FastgenWriter::Record(m_writer) << m_next_element_id << ro2 << ri1 << ri2;
    ++m_next_element_id;
}


void
Section::add_line(std::size_t g1, std::size_t g2, fastf_t thickness,
		  fastf_t radius)
{
    thickness *= INCHES_PER_MM;
    radius *= INCHES_PER_MM;

    if (g1 == g2 || !g1 || !g2 || g1 >= m_next_grid_id || g2 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    if ((!m_volume_mode && thickness <= 0.0) || thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    if (radius <= 0.0)
	throw std::invalid_argument("invalid radius");

    FastgenWriter::Record record(m_writer);
    record << "CLINE" << m_next_element_id++ << 0 << g1 << g2 << "" << "";

    if (m_volume_mode)
	record << thickness;
    else
	record.non_zero(thickness);

    record << radius;
}


void
Section::add_triangle(std::size_t g1, std::size_t g2, std::size_t g3,
		      fastf_t thickness, bool grid_centered)
{
    thickness *= INCHES_PER_MM;

    if (g1 == g2 || g1 == g3 || g2 == g3)
	throw std::invalid_argument("invalid grid id");

    if (!g1 || !g2 || !g3 || g1 >= m_next_grid_id || g2 >= m_next_grid_id
	|| g3 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    FastgenWriter::Record record(m_writer);
    record << "CTRI" << m_next_element_id++ << 0 << g1 << g2 << g3;
    record.non_zero(thickness);
    record << (grid_centered ? 1 : 2);
}


void
Section::add_hexahedron(const std::size_t *g)
{
    for (int i = 0; i < 8; ++i) {
	if (!g[i] || g[i] >= m_next_grid_id)
	    throw std::invalid_argument("invalid grid id");

	for (int j = i + 1; j < 8; ++j)
	    if (g[i] == g[j])
		throw std::invalid_argument("repeated grid id");
    }

    {
	FastgenWriter::Record record1(m_writer);
	record1 << "CHEX2" << m_next_element_id << 0;

	for (int i = 0; i < 6; ++i)
	    record1 << g[i];

	record1 << m_next_element_id;
    }

    FastgenWriter::Record(m_writer) << m_next_element_id << g[6] << g[7];
    ++m_next_element_id;
}


HIDDEN void
write_bot(FastgenWriter &writer, const std::string &name,
	  const rt_bot_internal &bot, const unsigned char *color = NULL)
{
    Section section(writer, name, bot.mode == RT_BOT_SOLID, color);

    for (std::size_t i = 0; i < bot.num_vertices; ++i) {
	const fastf_t * const vertex = &bot.vertices[i * 3];
	section.add_grid_point(V3ARGS(vertex));
    }

    for (std::size_t i = 0; i < bot.num_faces; ++i) {
	fastf_t thickness = 1.0;
	bool grid_centered = false;

	if (bot.mode == RT_BOT_PLATE) {
	    // fg4 does not allow zero thickness
	    // set a very small thickness if face thickness is zero
	    if (bot.thickness)
		thickness = !ZERO(bot.thickness[i]) ? bot.thickness[i] : 2 * SMALL_FASTF;

	    if (bot.face_mode)
		grid_centered = !BU_BITTEST(bot.face_mode, i);
	}

	const int * const face = &bot.faces[i * 3];
	section.add_triangle(face[0] + 1, face[1] + 1, face[2] + 1, thickness,
			     grid_centered);
    }
}


HIDDEN bool
ell_is_sphere(const rt_ell_internal &ell)
{
    // based on rt_sph_prep()

    fastf_t magsq_a, magsq_b, magsq_c;
    vect_t Au, Bu, Cu; // A, B, C with unit length
    fastf_t f;

    // Validate that |A| > 0, |B| > 0, |C| > 0
    magsq_a = MAGSQ(ell.a);
    magsq_b = MAGSQ(ell.b);
    magsq_c = MAGSQ(ell.c);

    if (ZERO(magsq_a) || ZERO(magsq_b) || ZERO(magsq_c))
	return false;

    // Validate that |A|, |B|, and |C| are nearly equal
    if (!EQUAL(magsq_a, magsq_b) || !EQUAL(magsq_a, magsq_c))
	return false;

    // Create unit length versions of A, B, C
    f = 1.0 / sqrt(magsq_a);
    VSCALE(Au, ell.a, f);
    f = 1.0 / sqrt(magsq_b);
    VSCALE(Bu, ell.b, f);
    f = 1.0 / sqrt(magsq_c);
    VSCALE(Cu, ell.c, f);

    // Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only)
    f = VDOT(Au, Bu);

    if (!ZERO(f))
	return false;

    f = VDOT(Bu, Cu);

    if (!ZERO(f))
	return false;

    f = VDOT(Au, Cu);

    if (!ZERO(f))
	return false;

    return true;
}


// Determines whether a tgc can be represented by a CCONE2 object.
// Assumes that tgc is a valid rt_tgc_internal.
HIDDEN bool
tgc_is_ccone(const rt_tgc_internal &tgc)
{
    if (VZERO(tgc.a) || VZERO(tgc.b))
	return false;

    if (!ZERO(VDOT(tgc.h, tgc.a)) || !ZERO(VDOT(tgc.h, tgc.b)))
	return false;

    {
	vect_t a_norm, b_norm, c_norm, d_norm;
	VMOVE(a_norm, tgc.a);
	VMOVE(b_norm, tgc.b);
	VMOVE(c_norm, tgc.c);
	VMOVE(d_norm, tgc.d);
	VUNITIZE(a_norm);
	VUNITIZE(b_norm);
	VUNITIZE(c_norm);
	VUNITIZE(d_norm);

	if (!VEQUAL(a_norm, c_norm) || !VEQUAL(b_norm, d_norm))
	    return false;
    }

    return true;
}


// Determines whether the object with the given name is a member of a
// CCONE-compatible region (typically created by the fastgen4 importer).
//
// Sets `new_name` to the name of the CCONE.
// Sets `ro1`, `ro2`, `ri1`, and `ri2` to the values of
// the lower/upper outer and inner radii.
HIDDEN bool
find_ccone_cutout(const std::string &name, db_i &db, std::string &new_name,
		  fastf_t &ro1, fastf_t &ro2, fastf_t &ri1, fastf_t &ri2)
{
    db_full_path path;

    if (db_string_to_path(&path, &db, name.c_str()))
	bu_bomb("logic error: invalid path");

    AutoFreePtr<db_full_path, db_free_full_path> autofree_path(&path);

    if (path.fp_len < 2)
	return false;

    rt_db_internal comb_db_internal;

    if (rt_db_get_internal(&comb_db_internal, path.fp_names[path.fp_len - 2], &db,
			   NULL, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_comb_db_internal(
	&comb_db_internal);

    rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
				      (comb_db_internal.idb_ptr);
    RT_CK_COMB(&comb_internal);

    const directory *outer_directory, *inner_directory;
    {
	const tree::tree_node &t = comb_internal.tree->tr_b;

	if (t.tb_op != OP_SUBTRACT || !t.tb_left || !t.tb_right
	    || t.tb_left->tr_op != OP_DB_LEAF || t.tb_right->tr_op != OP_DB_LEAF)
	    return false;

	outer_directory = db_lookup(&db, t.tb_left->tr_l.tl_name, false);
	inner_directory = db_lookup(&db, t.tb_right->tr_l.tl_name, false);

	// check for nonexistent members
	if (!outer_directory || !inner_directory)
	    return false;
    }

    rt_db_internal outer_db_internal, inner_db_internal;

    if (rt_db_get_internal(&outer_db_internal, outer_directory, &db, NULL,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_outer_db_internal(
	&outer_db_internal);

    if (rt_db_get_internal(&inner_db_internal, inner_directory, &db, NULL,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_inner_db_internal(
	&inner_db_internal);

    if ((outer_db_internal.idb_minor_type != ID_TGC
	 && outer_db_internal.idb_minor_type != ID_REC)
	|| (inner_db_internal.idb_minor_type != ID_TGC
	    && inner_db_internal.idb_minor_type != ID_REC))
	return false;

    const rt_tgc_internal &outer = *static_cast<rt_tgc_internal *>
				   (outer_db_internal.idb_ptr);
    const rt_tgc_internal &inner = *static_cast<rt_tgc_internal *>
				   (inner_db_internal.idb_ptr);
    RT_TGC_CK_MAGIC(&outer);
    RT_TGC_CK_MAGIC(&inner);

    // check cone geometry
    if (!tgc_is_ccone(outer) || !tgc_is_ccone(inner))
	return false;

    if (!VEQUAL(outer.v, inner.v) || !VEQUAL(outer.h, inner.h))
	return false;

    // store results
    --path.fp_len;
    new_name = AutoFreePtr<char>(db_path_to_string(&path)).ptr;
    ro1 = MAGNITUDE(outer.a);
    ro2 = MAGNITUDE(outer.b);
    ri1 = MAGNITUDE(inner.a);
    ri2 = MAGNITUDE(inner.b);
    return true;
}


struct ConversionData {
    FastgenWriter &m_writer;
    const bn_tol &m_tol;
    const bool convert_primitives;

    // for find_ccone_cutout()
    db_i &db;
    std::set<std::string> recorded_ccones;
};


HIDDEN bool
convert_primitive(ConversionData &data, const rt_db_internal &internal,
		  const std::string &name)
{
    switch (internal.idb_type) {
	case ID_CLINE: {
	    const rt_cline_internal &cline = *static_cast<rt_cline_internal *>
					     (internal.idb_ptr);
	    RT_CLINE_CK_MAGIC(&cline);

	    Section section(data.m_writer, name, true);
	    point_t v2;
	    VADD2(v2, cline.v, cline.h);
	    section.add_grid_point(V3ARGS(cline.v));
	    section.add_grid_point(V3ARGS(v2));
	    section.add_line(1, 2, cline.thickness, cline.radius);
	    break;
	}

	case ID_ELL:
	case ID_SPH: {
	    const rt_ell_internal &ell = *static_cast<rt_ell_internal *>(internal.idb_ptr);
	    RT_ELL_CK_MAGIC(&ell);

	    if (internal.idb_type != ID_SPH && !ell_is_sphere(ell))
		return false;

	    Section section(data.m_writer, name, true);
	    section.add_grid_point(V3ARGS(ell.v));
	    section.add_sphere(1, 1.0, MAGNITUDE(ell.a));
	    break;
	}

	case ID_TGC:
	case ID_REC: {
	    const rt_tgc_internal &tgc = *static_cast<rt_tgc_internal *>(internal.idb_ptr);
	    RT_TGC_CK_MAGIC(&tgc);

	    if (internal.idb_type != ID_REC && !tgc_is_ccone(tgc))
		return false;

	    std::string new_name;
	    fastf_t ro1, ro2, ri1, ri2;

	    if (find_ccone_cutout(name, data.db, new_name, ro1, ro2, ri1, ri2)) {
		// an imported CCONE with cutout
		if (!data.recorded_ccones.insert(new_name).second)
		    break; // already written
	    } else {
		new_name = name;
		ro1 = MAGNITUDE(tgc.a);
		ro2 = MAGNITUDE(tgc.b);
		ri1 = ri2 = 0.0;
	    }

	    Section section(data.m_writer, new_name, true);
	    point_t v2;
	    VADD2(v2, tgc.v, tgc.h);
	    section.add_grid_point(V3ARGS(tgc.v));
	    section.add_grid_point(V3ARGS(v2));
	    section.add_cone(1, 2, ro1, ro2, ri1, ri2);
	    break;
	}

	case ID_ARB8: {
	    const rt_arb_internal &arb = *static_cast<rt_arb_internal *>(internal.idb_ptr);
	    RT_ARB_CK_MAGIC(&arb);
	    Section section(data.m_writer, name, true);

	    for (int i = 0; i < 8; ++i)
		section.add_grid_point(V3ARGS(arb.pt[i]));

	    const std::size_t points[] = {1, 2, 3, 4, 5, 6, 7, 8};
	    section.add_hexahedron(points);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal.idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);
	    write_bot(data.m_writer, name, bot);
	    break;
	}

	default:
	    return false;
    }

    return true;
}


HIDDEN void
write_nmg_region(nmgregion *nmg_region, const db_full_path *path,
		 int UNUSED(region_id), int UNUSED(material_id), float *color, void *client_data)
{
    NMG_CK_REGION(nmg_region);
    NMG_CK_MODEL(nmg_region->m_p);
    RT_CK_FULL_PATH(path);

    ConversionData &data = *static_cast<ConversionData *>(client_data);

    AutoFreePtr<char> name(db_path_to_string(path));

    shell *vshell;

    for (BU_LIST_FOR(vshell, shell, &nmg_region->s_hd)) {
	NMG_CK_SHELL(vshell);

	rt_bot_internal *bot = nmg_bot(vshell, &data.m_tol);

	// fill in an rt_db_internal with our new bot so we can free it
	rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = bot;

	try {
	    unsigned char char_color[3];
	    char_color[0] = static_cast<unsigned char>(color[0] * 255.0 + 0.5);
	    char_color[1] = static_cast<unsigned char>(color[1] * 255.0 + 0.5);
	    char_color[2] = static_cast<unsigned char>(color[2] * 255.0 + 0.5);
	    write_bot(data.m_writer, name.ptr, *bot, char_color);
	} catch (const std::runtime_error &e) {
	    bu_log("FAILURE: write_bot() failed on object '%s': %s\n", name.ptr, e.what());
	} catch (const std::logic_error &e) {
	    bu_log("FAILURE: write_bot() failed on object '%s': %s\n", name.ptr, e.what());
	}

	internal.idb_meth->ft_ifree(&internal);
    }
}


HIDDEN tree *
convert_leaf(db_tree_state *tree_state, const db_full_path *path,
	     rt_db_internal *internal, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_DB_INTERNAL(internal);

    ConversionData &data = *static_cast<ConversionData *>(client_data);

    if (internal->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	return NULL;

    AutoFreePtr<char> name(db_path_to_string(path));
    bool converted = false;

    try {
	if (data.convert_primitives)
	    converted = convert_primitive(data, *internal, name.ptr);
    } catch (const std::runtime_error &e) {
	bu_log("FAILURE: convert_primitive() failed on object '%s': %s\n", name.ptr,
	       e.what());
    } catch (const std::logic_error &e) {
	bu_log("FAILURE: convert_primitive() failed on object '%s': %s\n", name.ptr,
	       e.what());
    }

    if (!converted)
	return nmg_booltree_leaf_tess(tree_state, path, internal, client_data);

    tree *result;
    RT_GET_TREE(result, tree_state->ts_resp);
    result->tr_op = OP_NOP;
    return result;
}


HIDDEN tree *
convert_region(db_tree_state *tree_state, const db_full_path *path,
	       tree *current_tree, void *client_data)
{
    ConversionData &data = *static_cast<ConversionData *>(client_data);

    gcv_region_end_data gcv_data = {write_nmg_region, &data};
    return gcv_region_end(tree_state, path, current_tree, &gcv_data);
}


HIDDEN int
gcv_fastgen4_write(const char *path, struct db_i *dbip,
		   const struct gcv_opts *UNUSED(options))
{
    // Set to true to directly translate any primitives that can be represented by fg4.
    // Due to limitations in the fg4 format, boolean operations can not be represented.
    const bool convert_primitives = false;

    FastgenWriter writer(path);
    writer.write_comment(dbip->dbi_title);
    writer.write_comment("g -> fastgen4 conversion");

    const rt_tess_tol ttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0.0};
    const bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1 - 1e-6};
    ConversionData conv_data = {writer, tol, convert_primitives, *dbip, std::set<std::string>()};

    {
	model *vmodel;
	db_tree_state initial_tree_state = rt_initial_tree_state;
	initial_tree_state.ts_tol = &tol;
	initial_tree_state.ts_ttol = &ttol;
	initial_tree_state.ts_m = &vmodel;

	db_update_nref(dbip, &rt_uniresource);
	directory **results;
	std::size_t num_objects = db_ls(dbip, DB_LS_TOPS, NULL, &results);
	AutoFreePtr<char *> object_names(db_dpv_to_argv(results));
	bu_free(results, "tops");

	vmodel = nmg_mm();
	db_walk_tree(dbip, num_objects, const_cast<const char **>(object_names.ptr), 1,
		     &initial_tree_state, NULL, convert_region, convert_leaf, &conv_data);
	nmg_km(vmodel);
    }


    return 1;
}


static const struct gcv_converter converters[] = {
    {"fg4", NULL, gcv_fastgen4_write},
    {NULL, NULL, NULL}
};


}


extern "C" {


    struct gcv_plugin_info gcv_plugin_conv_fastgen4_write = {converters};


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
