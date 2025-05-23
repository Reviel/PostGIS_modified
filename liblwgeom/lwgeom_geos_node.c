/**********************************************************************
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.net
 *
 * PostGIS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * PostGIS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PostGIS.  If not, see <http://www.gnu.org/licenses/>.
 *
 **********************************************************************
 *
 * Copyright (C) 2011 Sandro Santilli <strk@kbt.io>
 *
 **********************************************************************/


#include "lwgeom_geos.h"
#include "liblwgeom_internal.h"

#include <string.h>
#include <assert.h>

static int
lwgeom_ngeoms(const LWGEOM* n)
{
	const LWCOLLECTION* c = lwgeom_as_lwcollection(n);
	if ( c ) return c->ngeoms;
	else return 1;
}

static const LWGEOM*
lwgeom_subgeom(const LWGEOM* g, int n)
{
	const LWCOLLECTION* c = lwgeom_as_lwcollection(g);
	if ( c ) return lwcollection_getsubgeom((LWCOLLECTION*)c, n);
	else return g;
}


static void
lwgeom_collect_endpoints(const LWGEOM* lwg, LWMPOINT* col)
{
	int i, n;
	LWLINE* l;

	switch (lwg->type)
	{
		case MULTILINETYPE:
			for ( i = 0,
			        n = lwgeom_ngeoms(lwg);
			      i < n; ++i )
			{
				lwgeom_collect_endpoints(
					lwgeom_subgeom(lwg, i),
					col);
			}
			break;
		case LINETYPE:
			l = (LWLINE*)lwg;
			col = lwmpoint_add_lwpoint(col,
				lwline_get_lwpoint(l, 0));
			col = lwmpoint_add_lwpoint(col,
				lwline_get_lwpoint(l, l->points->npoints-1));
			break;
		default:
			lwerror("lwgeom_collect_endpoints: invalid type %s",
				lwtype_name(lwg->type));
			break;
	}
}

static LWMPOINT*
lwgeom_extract_endpoints(const LWGEOM* lwg)
{
	LWMPOINT* col = lwmpoint_construct_empty(SRID_UNKNOWN,
	                              FLAGS_GET_Z(lwg->flags),
	                              FLAGS_GET_M(lwg->flags));
	lwgeom_collect_endpoints(lwg, col);

	return col;
}

/* Assumes initGEOS was called already */
/* May return LWPOINT or LWMPOINT */
static LWGEOM*
lwgeom_extract_unique_endpoints(const LWGEOM* lwg)
{
	LWGEOM* ret;
	GEOSGeometry *gepu;
	LWMPOINT *epall = lwgeom_extract_endpoints(lwg);
	GEOSGeometry *gepall = LWGEOM2GEOS((LWGEOM*)epall, 1);
	lwmpoint_free(epall);
	if ( ! gepall ) {
		lwerror("LWGEOM2GEOS: %s", lwgeom_geos_errmsg);
		return NULL;
	}

	/* UnaryUnion to remove duplicates */
	/* TODO: do it all within pgis using indices */
	gepu = GEOSUnaryUnion_r(lwcontext()->geos_ctx, gepall);
	if ( ! gepu ) {
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, gepall);
		lwerror("GEOSUnaryUnion: %s", lwgeom_geos_errmsg);
		return NULL;
	}
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, gepall);

	ret = GEOS2LWGEOM(gepu, FLAGS_GET_Z(lwg->flags));
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, gepu);
	if ( ! ret ) {
		lwerror("Error during GEOS2LWGEOM");
		return NULL;
	}

	return ret;
}

/* exported */
extern LWGEOM* lwgeom_node(const LWGEOM* lwgeom_in);
LWGEOM*
lwgeom_node(const LWGEOM* lwgeom_in)
{
	GEOSGeometry *g1, *gn, *gm;
	LWGEOM *ep, *lines;
	LWCOLLECTION *col, *tc;
	int pn, ln, np, nl;

	if ( lwgeom_dimension(lwgeom_in) != 1 ) {
		lwerror("Noding geometries of dimension != 1 is unsupported");
		return NULL;
	}

	//initGEOS(lwgeom_geos_error, lwgeom_geos_error);
	GEOSContext_setNoticeHandler_r(lwcontext()->geos_ctx, lwnotice);
	GEOSContext_setErrorHandler_r(lwcontext()->geos_ctx, lwgeom_geos_error);

	g1 = LWGEOM2GEOS(lwgeom_in, 1);
	if ( ! g1 ) {
		lwerror("LWGEOM2GEOS: %s", lwgeom_geos_errmsg);
		return NULL;
	}

	ep = lwgeom_extract_unique_endpoints(lwgeom_in);
	if ( ! ep ) {
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, g1);
		lwerror("Error extracting unique endpoints from input");
		return NULL;
	}

	gn = GEOSNode_r(lwcontext()->geos_ctx, g1);
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, g1);
	if ( ! gn ) {
		lwgeom_free(ep);
		lwerror("GEOSNode: %s", lwgeom_geos_errmsg);
		return NULL;
	}

	gm = GEOSLineMerge_r(lwcontext()->geos_ctx, gn);
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, gn);
	if ( ! gm ) {
		lwgeom_free(ep);
		lwerror("GEOSLineMerge: %s", lwgeom_geos_errmsg);
		return NULL;
	}

	lines = GEOS2LWGEOM(gm, FLAGS_GET_Z(lwgeom_in->flags));
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, gm);
	if ( ! lines ) {
		lwgeom_free(ep);
		lwerror("Error during GEOS2LWGEOM");
		return NULL;
	}

	/*
	 * Reintroduce endpoints from input, using split-line-by-point.
	 * Note that by now we can be sure that each point splits at
	 * most _one_ segment as any point shared by multiple segments
	 * would already be a node. Also we can be sure that any of
	 * the segments endpoints won't split any other segment.
	 * We can use the above 2 assertions to early exit the loop.
	 */

	col = lwcollection_construct_empty(MULTILINETYPE, lwgeom_in->srid,
	                              FLAGS_GET_Z(lwgeom_in->flags),
	                              FLAGS_GET_M(lwgeom_in->flags));

	np = lwgeom_ngeoms(ep);
	for (pn=0; pn<np; ++pn) { /* for each point */

		const LWPOINT* p = (LWPOINT*)lwgeom_subgeom(ep, pn);

		nl = lwgeom_ngeoms(lines);
		for (ln=0; ln<nl; ++ln) { /* for each line */

			const LWLINE* l = (LWLINE*)lwgeom_subgeom(lines, ln);

			int s = lwline_split_by_point_to(l, p, (LWMLINE*)col);

			if ( ! s ) continue; /* not on this line */

			if ( s == 1 ) {
				/* found on this line, but not splitting it */
				break;
			}

			/* splits this line */

			/* replace this line with the two splits */
			if ( lwgeom_is_collection(lines) ) {
				tc = (LWCOLLECTION*)lines;
				lwcollection_reserve(tc, nl + 1);
				while (nl > ln+1) {
					tc->geoms[nl] = tc->geoms[nl-1];
					--nl;
				}
				lwgeom_free(tc->geoms[ln]);
				tc->geoms[ln]   = col->geoms[0];
				tc->geoms[ln+1] = col->geoms[1];
				tc->ngeoms++;
			} else {
				lwgeom_free(lines);
				/* transfer ownership rather than cloning */
				lines = (LWGEOM*)lwcollection_clone_deep(col);
				assert(col->ngeoms == 2);
				lwgeom_free(col->geoms[0]);
				lwgeom_free(col->geoms[1]);
			}

			/* reset the vector */
			assert(col->ngeoms == 2);
			col->ngeoms = 0;

			break;
		}

	}

	lwgeom_free(ep);
	lwcollection_free(col);

	lwgeom_set_srid(lines, lwgeom_in->srid);
	return (LWGEOM*)lines;
}

