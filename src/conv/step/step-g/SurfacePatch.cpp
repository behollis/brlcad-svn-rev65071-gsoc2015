/*                 SurfacePatch.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/SurfacePatch.cpp
 *
 * Routines to convert STEP "SurfacePatch" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundedSurface.h"

#include "SurfacePatch.h"

#define CLASSNAME "SurfacePatch"
#define ENTITYNAME "Surface_Patch"
string SurfacePatch::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SurfacePatch::Create);

static const char *Transition_code_string[] = {
    "discontinuous",
    "continuous",
    "cont_same_gradient",
    "cont_same_gradient_same_curvature",
    "unset"
};

SurfacePatch::SurfacePatch()
{
    step = NULL;
    id = 0;
    parent_surface = NULL;
    u_transition = Transition_code_unset;
    v_transition = Transition_code_unset;
    u_sense = BUnset;
    v_sense = BUnset;
}

SurfacePatch::SurfacePatch(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    parent_surface = NULL;
    u_transition = Transition_code_unset;
    v_transition = Transition_code_unset;
    u_sense = BUnset;
    v_sense = BUnset;
}

SurfacePatch::~SurfacePatch()
{
}

bool
SurfacePatch::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sw || !sse)
	return false;

    step = sw;
    id = sse->STEPfile_id;

    if (!FoundedItem::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	goto step_error;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (parent_surface == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "parent_surface");
	if (entity != NULL) {
	    parent_surface = dynamic_cast<BoundedSurface *>(Factory::CreateObject(sw, entity));
	}
	if (!entity || !parent_surface) {
	    std::cout << CLASSNAME << ":Error loading member field \"parent_surface\"." << std::endl;
	    goto step_error;
	}
    }

    u_transition = (Transition_code)step->getEnumAttribute(sse, "u_transition");
    V_MIN(u_transition, Transition_code_unset);

    v_transition = (Transition_code)step->getEnumAttribute(sse, "v_transition");
    V_MIN(v_transition, Transition_code_unset);

    u_sense = step->getBooleanAttribute(sse, "u_sense");
    v_sense = step->getBooleanAttribute(sse, "v_sense");

    sw->entity_status[id] = STEP_LOADED;

    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

void
SurfacePatch::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "parent_surface:" << std::endl;
    parent_surface->Print(level + 1);
    TAB(level + 1);
    std::cout << "u_transition:" << Transition_code_string[u_transition] << std::endl;
    TAB(level + 1);
    std::cout << "v_transition:" << Transition_code_string[v_transition] << std::endl;

    if (step) {
	TAB(level + 1);
	std::cout << "u_sense:" << step->getBooleanString(u_sense) << std::endl;
	TAB(level + 1);
	std::cout << "v_sense:" << step->getBooleanString(v_sense) << std::endl;
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    FoundedItem::Print(level + 1);
}

STEPEntity *
SurfacePatch::GetInstance(STEPWrapper *sw, int id)
{
    return new SurfacePatch(sw, id);
}

STEPEntity *
SurfacePatch::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
SurfacePatch::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
