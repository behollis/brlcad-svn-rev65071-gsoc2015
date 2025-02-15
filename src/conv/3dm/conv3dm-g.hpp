/*                   C O N V 3 D M - G . H P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file conv3dm-g.hpp
 *
 * Library for conversion of Rhino models (in .3dm files) to BRL-CAD databases.
 *
 */


#ifndef CONV3DM_G_H
#define CONV3DM_G_H

#include "common.h"

#include <map>
#include <set>
#include <string>

#include "wdb.h"


namespace conv3dm
{


struct UuidCompare {
    bool operator()(const ON_UUID &left, const ON_UUID &right) const;
};


class RhinoConverter
{
public:
    RhinoConverter(const std::string &output_path,
		   bool verbose_mode = false);

    ~RhinoConverter();


    void write_model(const std::string &path, bool use_uuidnames,
		     bool random_colors);


private:
    class Color;


    class ObjectManager
    {
    public:
	ObjectManager();

	void add(bool use_uuid, const ON_UUID &uuid, const std::string &prefix,
		 const char *suffix);
	void register_member(const ON_UUID &parent_uuid, const ON_UUID &member_uuid);
	void mark_idef_member(const ON_UUID &uuid);

	bool exists(const ON_UUID &uuid) const;
	const std::string &get_name(const ON_UUID &uuid) const;
	const std::set<ON_UUID, UuidCompare> &get_members(const ON_UUID &uuid) const;
	bool is_idef_member(const ON_UUID &uuid) const;


    private:
	struct ModelObject;

	std::string unique_name(std::string prefix, const char *suffix);

	std::map<ON_UUID, ModelObject, UuidCompare> m_obj_map;
	std::map<std::string, int> m_name_count_map;
    };


    RhinoConverter(const RhinoConverter &source);
    RhinoConverter &operator=(const RhinoConverter &source);

    void clean_model();
    void map_uuid_names(bool use_uuidnames);
    void create_all_bitmaps();
    void create_all_layers();
    void create_all_idefs();
    void create_all_objects();

    void create_bitmap(const ON_Bitmap &bitmap);
    void create_layer(const ON_Layer &layer);
    void create_idef(const ON_InstanceDefinition &idef);

    void create_object(const ON_Object &object,
		       const ON_3dmObjectAttributes &object_attrs);
    void create_geom_comb(const ON_3dmObjectAttributes &geom_attrs);

    void create_iref(const ON_InstanceRef &iref,
		     const ON_3dmObjectAttributes &iref_attrs);

    void create_brep(const ON_Brep &brep,
		     const ON_3dmObjectAttributes &brep_attrs);

    void create_mesh(ON_Mesh mesh, const ON_3dmObjectAttributes &mesh_attrs);

    Color get_color(const ON_3dmObjectAttributes &obj_attrs) const;
    Color get_color(const ON_Layer &layer) const;

    std::pair<std::string, std::string>
    get_shader(int index) const;


    const bool m_verbose_mode;
    const std::string m_output_dirname;
    rt_wdb * const m_db;
    ON_TextLog m_log;
    ObjectManager m_objects;

    bool m_random_colors;
    ONX_Model m_model;
};


}


#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
