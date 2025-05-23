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
 * Copyright 2009-2010 Sandro Santilli <strk@kbt.io>
 *
 **********************************************************************/

#include "../postgis_config.h"
/*#define POSTGIS_DEBUG_LEVEL 4*/

#include "liblwgeom.h"
#include "lwgeom_geos.h"
#include "liblwgeom_internal.h"
#include "lwgeom_log.h"
#include "optionlist.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* #define PARANOIA_LEVEL 2 */
#undef LWGEOM_PROFILE_MAKEVALID

#if POSTGIS_GEOS_VERSION < 30800
/*
 * Return Nth vertex in GEOSGeometry as a POINT.
 * May return NULL if the geometry has NO vertex.
 */
GEOSGeometry* LWGEOM_GEOS_getPointN(const GEOSGeometry*, uint32_t);
GEOSGeometry*
LWGEOM_GEOS_getPointN(const GEOSGeometry* g_in, uint32_t n)
{
	uint32_t dims = 0;
	const GEOSCoordSequence* seq_in;
	GEOSCoordSeq seq_out;
	double val;
	uint32_t sz = 0;
	int gn;
	GEOSGeometry* ret;

	switch (GEOSGeomTypeId_r(lwcontext()->geos_ctx, g_in))
	{
	case GEOS_MULTIPOINT:
	case GEOS_MULTILINESTRING:
	case GEOS_MULTIPOLYGON:
	case GEOS_GEOMETRYCOLLECTION:
	{
		for (gn = 0; gn < GEOSGetNumGeometries_r(lwcontext()->geos_ctx, g_in); ++gn)
		{
			const GEOSGeometry* g = GEOSGetGeometryN_r(lwcontext()->geos_ctx, g_in, gn);
			ret = LWGEOM_GEOS_getPointN(g, n);
			if (ret) return ret;
		}
		break;
	}

	case GEOS_POLYGON:
	{
		ret = LWGEOM_GEOS_getPointN(GEOSGetExteriorRing_r(lwcontext()->geos_ctx, g_in), n);
		if (ret) return ret;
		for (gn = 0; gn < GEOSGetNumInteriorRings_r(lwcontext()->geos_ctx, g_in); ++gn)
		{
			const GEOSGeometry* g = GEOSGetInteriorRingN_r(lwcontext()->geos_ctx, g_in, gn);
			ret = LWGEOM_GEOS_getPointN(g, n);
			if (ret) return ret;
		}
		break;
	}

	case GEOS_POINT:
	case GEOS_LINESTRING:
	case GEOS_LINEARRING:
		break;
	}

	seq_in = GEOSGeom_getCoordSeq_r(lwcontext()->geos_ctx, g_in);
	if (!seq_in) return NULL;
	if (!GEOSCoordSeq_getSize_r(lwcontext()->geos_ctx, seq_in, &sz)) return NULL;
	if (!sz) return NULL;

	if (!GEOSCoordSeq_getDimensions_r(lwcontext()->geos_ctx, seq_in, &dims)) return NULL;

	seq_out = GEOSCoordSeq_create_r(lwcontext()->geos_ctx, 1, dims);
	if (!seq_out) return NULL;

	if (!GEOSCoordSeq_getX_r(lwcontext()->geos_ctx, seq_in, n, &val)) return NULL;
	if (!GEOSCoordSeq_setX_r(lwcontext()->geos_ctx, seq_out, n, val)) return NULL;
	if (!GEOSCoordSeq_getY_r(lwcontext()->geos_ctx, seq_in, n, &val)) return NULL;
	if (!GEOSCoordSeq_setY_r(lwcontext()->geos_ctx, seq_out, n, val)) return NULL;
	if (dims > 2)
	{
		if (!GEOSCoordSeq_getZ_r(lwcontext()->geos_ctx, seq_in, n, &val)) return NULL;
		if (!GEOSCoordSeq_setZ_r(lwcontext()->geos_ctx, seq_out, n, val)) return NULL;
	}

	return GEOSGeom_createPoint_r(lwcontext()->geos_ctx, seq_out);
}
#endif

LWGEOM* lwcollection_make_geos_friendly(LWCOLLECTION* g);
LWGEOM* lwline_make_geos_friendly(LWLINE* line);
LWGEOM* lwpoly_make_geos_friendly(LWPOLY* poly);
POINTARRAY* ring_make_geos_friendly(POINTARRAY* ring);

static void
ptarray_strip_nan_coords_in_place(POINTARRAY *pa)
{
	uint32_t i, j = 0;
	POINT4D *p, *np;
	int ndims = FLAGS_NDIMS(pa->flags);
	for ( i = 0; i < pa->npoints; i++ )
	{
		int isnan = 0;
		p = (POINT4D *)(getPoint_internal(pa, i));
		if ( isnan(p->x) || isnan(p->y) ) isnan = 1;
		else if (ndims > 2 && isnan(p->z) ) isnan = 1;
		else if (ndims > 3 && isnan(p->m) ) isnan = 1;
		if ( isnan ) continue;

		np = (POINT4D *)(getPoint_internal(pa, j++));
		if ( np != p ) {
			np->x = p->x;
			np->y = p->y;
			if (ndims > 2)
				np->z = p->z;
			if (ndims > 3)
				np->m = p->m;
		}
	}
	pa->npoints = j;
}



/*
 * Ensure the geometry is "structurally" valid
 * (enough for GEOS to accept it)
 * May return the input untouched (if already valid).
 * May return geometries of lower dimension (on collapses)
 */
static LWGEOM*
lwgeom_make_geos_friendly(LWGEOM* geom)
{
	LWDEBUGF(2, "lwgeom_make_geos_friendly enter (type %d)", geom->type);
	switch (geom->type)
	{
	case POINTTYPE:
		ptarray_strip_nan_coords_in_place(((LWPOINT*)geom)->point);
		return geom;

	case LINETYPE:
		/* lines need at least 2 points */
		return lwline_make_geos_friendly((LWLINE*)geom);
		break;

	case POLYGONTYPE:
		/* polygons need all rings closed and with npoints > 3 */
		return lwpoly_make_geos_friendly((LWPOLY*)geom);
		break;

	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
	case MULTIPOINTTYPE:
		return lwcollection_make_geos_friendly((LWCOLLECTION*)geom);
		break;

	case CIRCSTRINGTYPE:
	case COMPOUNDTYPE:
	case CURVEPOLYTYPE:
	case MULTISURFACETYPE:
	case MULTICURVETYPE:
	default:
		lwerror("lwgeom_make_geos_friendly: unsupported input geometry type: %s (%d)",
			lwtype_name(geom->type),
			geom->type);
		break;
	}
	return 0;
}

/*
 * Close the point array, if not already closed in 2d.
 * Returns the input if already closed in 2d, or a newly
 * constructed POINTARRAY.
 * TODO: move in ptarray.c
 */
static POINTARRAY*
ptarray_close2d(POINTARRAY* ring)
{
	POINTARRAY* newring;

	/* close the ring if not already closed (2d only) */
	if (!ptarray_is_closed_2d(ring))
	{
		/* close it up */
		newring = ptarray_addPoint(ring, getPoint_internal(ring, 0), FLAGS_NDIMS(ring->flags), ring->npoints);
		ring = newring;
	}
	return ring;
}

/* May return the same input or a new one (never zero) */
POINTARRAY*
ring_make_geos_friendly(POINTARRAY* ring)
{
	POINTARRAY* closedring;
	POINTARRAY* ring_in = ring;

	ptarray_strip_nan_coords_in_place(ring_in);

	/* close the ring if not already closed (2d only) */
	closedring = ptarray_close2d(ring);
	if (closedring != ring) ring = closedring;

	/* return 0 for collapsed ring (after closeup) */

	while (ring->npoints < 4)
	{
		POINTARRAY* oring = ring;
		LWDEBUGF(4, "ring has %d points, adding another", ring->npoints);
		/* let's add another... */
		ring = ptarray_addPoint(ring, getPoint_internal(ring, 0), FLAGS_NDIMS(ring->flags), ring->npoints);
		if (oring != ring_in) ptarray_free(oring);
	}

	return ring;
}

/* Make sure all rings are closed and have > 3 points.
 * May return the input untouched.
 */
LWGEOM*
lwpoly_make_geos_friendly(LWPOLY* poly)
{
	LWGEOM* ret;
	POINTARRAY** new_rings;
	uint32_t i;

	/* If the polygon has no rings there's nothing to do */
	if (!poly->nrings) return (LWGEOM*)poly;

	/* Allocate enough pointers for all rings */
	new_rings = lwalloc(sizeof(POINTARRAY*) * poly->nrings);

	/* All rings must be closed and have > 3 points */
	for (i = 0; i < poly->nrings; i++)
	{
		POINTARRAY* ring_in = poly->rings[i];
		POINTARRAY* ring_out = ring_make_geos_friendly(ring_in);

		if (ring_in != ring_out)
		{
			LWDEBUGF(
			    3, "lwpoly_make_geos_friendly: ring %d cleaned, now has %d points", i, ring_out->npoints);
			ptarray_free(ring_in);
		}
		else
			LWDEBUGF(3, "lwpoly_make_geos_friendly: ring %d untouched", i);

		assert(ring_out);
		new_rings[i] = ring_out;
	}

	lwfree(poly->rings);
	poly->rings = new_rings;
	ret = (LWGEOM*)poly;

	return ret;
}

/* Need NO or >1 points. Duplicate first if only one. */
LWGEOM*
lwline_make_geos_friendly(LWLINE* line)
{
	LWGEOM* ret;

	ptarray_strip_nan_coords_in_place(line->points);

	if (line->points->npoints == 1) /* 0 is fine, 2 is fine */
	{
#if 1
		/* Duplicate point */
		line->points = ptarray_addPoint(line->points,
						getPoint_internal(line->points, 0),
						FLAGS_NDIMS(line->points->flags),
						line->points->npoints);
		ret = (LWGEOM*)line;
#else
		/* Turn into a point */
		ret = (LWGEOM*)lwpoint_construct(line->srid, 0, line->points);
#endif
		return ret;
	}
	else
	{
		return (LWGEOM*)line;
		/* return lwline_clone(line); */
	}
}

LWGEOM*
lwcollection_make_geos_friendly(LWCOLLECTION* g)
{
	LWGEOM** new_geoms;
	uint32_t i, new_ngeoms = 0;
	LWCOLLECTION* ret;

	if ( ! g->ngeoms ) {
		LWDEBUG(3, "lwcollection_make_geos_friendly: returning input untouched");
		return lwcollection_as_lwgeom(g);
	}

	/* enough space for all components */
	new_geoms = lwalloc(sizeof(LWGEOM*) * g->ngeoms);

	ret = lwalloc(sizeof(LWCOLLECTION));
	memcpy(ret, g, sizeof(LWCOLLECTION));
	ret->maxgeoms = g->ngeoms;

	for (i = 0; i < g->ngeoms; i++)
	{
		LWGEOM* newg = lwgeom_make_geos_friendly(g->geoms[i]);
		if (!newg) continue;
		if ( newg != g->geoms[i] ) {
			new_geoms[new_ngeoms++] = newg;
		} else {
			new_geoms[new_ngeoms++] = lwgeom_clone(newg);
		}
	}

	ret->bbox = NULL; /* recompute later... */

	ret->ngeoms = new_ngeoms;
	if (new_ngeoms)
		ret->geoms = new_geoms;
	else
	{
		lwfree(new_geoms);
		ret->geoms = NULL;
		ret->maxgeoms = 0;
	}

	return (LWGEOM*)ret;
}

#if POSTGIS_GEOS_VERSION < 30800

/*
 * Fully node given linework
 */
static GEOSGeometry*
LWGEOM_GEOS_nodeLines(const GEOSGeometry* lines)
{
	/* GEOS3.7 GEOSNode fails on regression tests */
	/* GEOS3.7 GEOSUnaryUnion fails on regression tests */

	/* union of first point with geometry */
	GEOSGeometry *unioned, *point;
	point = LWGEOM_GEOS_getPointN(lines, 0);
	if (!point) return NULL;
	unioned = GEOSUnion_r(lwcontext()->geos_ctx, lines, point);
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, point);
	return unioned;
}

/*
 * We expect initGEOS being called already.
 * Will return NULL on error (expect error handler being called by then)
 */
static GEOSGeometry*
LWGEOM_GEOS_makeValidPolygon(const GEOSGeometry* gin)
{
	GEOSGeom gout;
	GEOSGeom geos_bound;
	GEOSGeom geos_cut_edges, geos_area, collapse_points;
	GEOSGeometry* vgeoms[3]; /* One for area, one for cut-edges */
	unsigned int nvgeoms = 0;

	assert(GEOSGeomTypeId_r(lwcontext()->geos_ctx, gin) == GEOS_POLYGON || GEOSGeomTypeId_r(lwcontext()->geos_ctx, gin) == GEOS_MULTIPOLYGON);

	geos_bound = GEOSBoundary_r(lwcontext()->geos_ctx, gin);
	if (!geos_bound) return NULL;

	/* Use noded boundaries as initial "cut" edges */

	geos_cut_edges = LWGEOM_GEOS_nodeLines(geos_bound);
	if (!geos_cut_edges)
	{
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_bound);
		lwnotice("LWGEOM_GEOS_nodeLines(): %s", lwgeom_geos_errmsg);
		return NULL;
	}

	/* NOTE: the noding process may drop lines collapsing to points.
	 *       We want to retrieve any of those */
	{
		GEOSGeometry* pi;
		GEOSGeometry* po;

		pi = GEOSGeom_extractUniquePoints_r(lwcontext()->geos_ctx, geos_bound);
		if (!pi)
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_bound);
			lwnotice("GEOSGeom_extractUniquePoints(): %s", lwgeom_geos_errmsg);
			return NULL;
		}

		po = GEOSGeom_extractUniquePoints_r(lwcontext()->geos_ctx, geos_cut_edges);
		if (!po)
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_bound);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, pi);
			lwnotice("GEOSGeom_extractUniquePoints(): %s", lwgeom_geos_errmsg);
			return NULL;
		}

		collapse_points = GEOSDifference_r(lwcontext()->geos_ctx, pi, po);
		if (!collapse_points)
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_bound);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, pi);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, po);
			lwnotice("GEOSDifference(): %s", lwgeom_geos_errmsg);
			return NULL;
		}

		GEOSGeom_destroy_r(lwcontext()->geos_ctx, pi);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, po);
	}
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_bound);

	/* And use an empty geometry as initial "area" */
	geos_area = GEOSGeom_createEmptyPolygon_r(lwcontext()->geos_ctx);
	if (!geos_area)
	{
		lwnotice("GEOSGeom_createEmptyPolygon(): %s", lwgeom_geos_errmsg);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);
		return NULL;
	}

	/*
	 * See if an area can be build with the remaining edges
	 * and if it can, symdifference with the original area.
	 * Iterate this until no more polygons can be created
	 * with left-over edges.
	 */
	while (GEOSGetNumGeometries_r(lwcontext()->geos_ctx, geos_cut_edges))
	{
		GEOSGeometry* new_area = 0;
		GEOSGeometry* new_area_bound = 0;
		GEOSGeometry* symdif = 0;
		GEOSGeometry* new_cut_edges = 0;

#ifdef LWGEOM_PROFILE_MAKEVALID
		lwnotice("ST_MakeValid: building area from %d edges", GEOSGetNumGeometries_r(lwcontext()->geos_ctx, geos_cut_edges));
#endif

		/*
		 * ASSUMPTION: cut_edges should already be fully noded
		 */

		new_area = LWGEOM_GEOS_buildArea(geos_cut_edges);
		if (!new_area) /* must be an exception */
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);
			lwnotice("LWGEOM_GEOS_buildArea() threw an error: %s", lwgeom_geos_errmsg);
			return NULL;
		}

		if (GEOSisEmpty_r(lwcontext()->geos_ctx, new_area))
		{
			/* no more rings can be build with thes edges */
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area);
			break;
		}

		/*
		 * We succeeded in building a ring!
		 * Save the new ring boundaries first (to compute
		 * further cut edges later)
		 */
		new_area_bound = GEOSBoundary_r(lwcontext()->geos_ctx, new_area);
		if (!new_area_bound)
		{
			/* We did check for empty area already so this must be some other error */
			lwnotice("GEOSBoundary('%s') threw an error: %s",
				 lwgeom_to_ewkt(GEOS2LWGEOM(new_area, 0)),
				 lwgeom_geos_errmsg);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);
			return NULL;
		}

		/*
		 * Now symdif new and old area
		 */
		symdif = GEOSSymDifference_r(lwcontext()->geos_ctx, geos_area, new_area);
		if (!symdif) /* must be an exception */
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area_bound);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);
			lwnotice("GEOSSymDifference() threw an error: %s", lwgeom_geos_errmsg);
			return NULL;
		}

		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area);
		geos_area = symdif;
		symdif = 0;

		/*
		 * Now let's re-set geos_cut_edges with what's left
		 * from the original boundary.
		 * ASSUMPTION: only the previous cut-edges can be
		 *             left, so we don't need to reconsider
		 *             the whole original boundaries
		 *
		 * NOTE: this is an expensive operation.
		 *
		 */

		new_cut_edges = GEOSDifference_r(lwcontext()->geos_ctx, geos_cut_edges, new_area_bound);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, new_area_bound);
		if (!new_cut_edges) /* an exception ? */
		{
			/* cleanup and throw */
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);
			lwerror("GEOSDifference() threw an error: %s", lwgeom_geos_errmsg);
			return NULL;
		}
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);
		geos_cut_edges = new_cut_edges;
	}

	if (!GEOSisEmpty_r(lwcontext()->geos_ctx, geos_area))
		vgeoms[nvgeoms++] = geos_area;
	else
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_area);

	if (!GEOSisEmpty_r(lwcontext()->geos_ctx, geos_cut_edges))
		vgeoms[nvgeoms++] = geos_cut_edges;
	else
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, geos_cut_edges);

	if (!GEOSisEmpty_r(lwcontext()->geos_ctx, collapse_points))
		vgeoms[nvgeoms++] = collapse_points;
	else
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, collapse_points);

	if (1 == nvgeoms)
	{
		/* Return cut edges */
		gout = vgeoms[0];
	}
	else
	{
		/* Collect areas and lines (if any line) */
		gout = GEOSGeom_createCollection_r(lwcontext()->geos_ctx, GEOS_GEOMETRYCOLLECTION, vgeoms, nvgeoms);
		if (!gout) /* an exception again */
		{
			/* cleanup and throw */
			lwerror("GEOSGeom_createCollection() threw an error: %s", lwgeom_geos_errmsg);
			/* TODO: cleanup! */
			return NULL;
		}
	}

	return gout;
}

static GEOSGeometry*
LWGEOM_GEOS_makeValidLine(const GEOSGeometry* gin)
{
	GEOSGeometry* noded;
	noded = LWGEOM_GEOS_nodeLines(gin);
	return noded;
}

static GEOSGeometry*
LWGEOM_GEOS_makeValidMultiLine(const GEOSGeometry* gin)
{
	GEOSGeometry** lines;
	GEOSGeometry** points;
	GEOSGeometry* mline_out = 0;
	GEOSGeometry* mpoint_out = 0;
	GEOSGeometry* gout = 0;
	uint32_t nlines = 0, nlines_alloc;
	uint32_t npoints = 0;
	uint32_t ngeoms = 0, nsubgeoms;
	uint32_t i, j;

	ngeoms = GEOSGetNumGeometries_r(lwcontext()->geos_ctx, gin);

	nlines_alloc = ngeoms;
	lines = lwalloc(sizeof(GEOSGeometry*) * nlines_alloc);
	points = lwalloc(sizeof(GEOSGeometry*) * ngeoms);

	for (i = 0; i < ngeoms; ++i)
	{
		const GEOSGeometry* g = GEOSGetGeometryN_r(lwcontext()->geos_ctx, gin, i);
		GEOSGeometry* vg;
		vg = LWGEOM_GEOS_makeValidLine(g);
		/* Drop any invalid or empty geometry */
		if (!vg)
			continue;
		if (GEOSisEmpty_r(lwcontext()->geos_ctx, vg))
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, vg);
			continue;
		}

		if (GEOSGeomTypeId_r(lwcontext()->geos_ctx, vg) == GEOS_POINT)
			points[npoints++] = vg;
		else if (GEOSGeomTypeId_r(lwcontext()->geos_ctx, vg) == GEOS_LINESTRING)
			lines[nlines++] = vg;
		else if (GEOSGeomTypeId_r(lwcontext()->geos_ctx, vg) == GEOS_MULTILINESTRING)
		{
			nsubgeoms = GEOSGetNumGeometries_r(lwcontext()->geos_ctx, vg);
			nlines_alloc += nsubgeoms;
			lines = lwrealloc(lines, sizeof(GEOSGeometry*) * nlines_alloc);
			for (j = 0; j < nsubgeoms; ++j)
			{
				const GEOSGeometry* gc = GEOSGetGeometryN_r(lwcontext()->geos_ctx, vg, j);
				/* NOTE: ownership of the cloned geoms will be
				 *       taken by final collection */
				lines[nlines++] = GEOSGeom_clone_r(lwcontext()->geos_ctx, gc);
			}
		}
		else
		{
			/* NOTE: return from GEOSGeomType will leak
			 * but we really don't expect this to happen */
			lwerror("unexpected geom type returned by LWGEOM_GEOS_makeValid: %s", GEOSGeomType_r(lwcontext()->geos_ctx, vg));
		}
	}

	if (npoints)
	{
		if (npoints > 1)
			mpoint_out = GEOSGeom_createCollection_r(lwcontext()->geos_ctx, GEOS_MULTIPOINT, points, npoints);
		else
			mpoint_out = points[0];
	}

	if (nlines)
	{
		if (nlines > 1)
			mline_out = GEOSGeom_createCollection_r(lwcontext()->geos_ctx, GEOS_MULTILINESTRING, lines, nlines);
		else
			mline_out = lines[0];
	}

	lwfree(lines);

	if (mline_out && mpoint_out)
	{
		points[0] = mline_out;
		points[1] = mpoint_out;
		gout = GEOSGeom_createCollection_r(lwcontext()->geos_ctx, GEOS_GEOMETRYCOLLECTION, points, 2);
	}
	else if (mline_out)
		gout = mline_out;

	else if (mpoint_out)
		gout = mpoint_out;

	lwfree(points);

	return gout;
}

/*
 * We expect initGEOS being called already.
 * Will return NULL on error (expect error handler being called by then)
 */
static GEOSGeometry*
LWGEOM_GEOS_makeValidCollection(const GEOSGeometry* gin)
{
	int nvgeoms;
	GEOSGeometry** vgeoms;
	GEOSGeom gout;
	int i;

	nvgeoms = GEOSGetNumGeometries_r(lwcontext()->geos_ctx, gin);
	if (nvgeoms == -1)
	{
		lwerror("GEOSGetNumGeometries: %s", lwgeom_geos_errmsg);
		return 0;
	}

	vgeoms = lwalloc(sizeof(GEOSGeometry*) * nvgeoms);
	if (!vgeoms)
	{
		lwerror("LWGEOM_GEOS_makeValidCollection: out of memory");
		return 0;
	}

	for (i = 0; i < nvgeoms; ++i)
	{
		vgeoms[i] = LWGEOM_GEOS_makeValid(GEOSGetGeometryN_r(lwcontext()->geos_ctx, gin, i));
		if (!vgeoms[i])
		{
			int j;
			for (j = 0; j < (i - 1); j++)
				GEOSGeom_destroy_r(lwcontext()->geos_ctx, vgeoms[j]);
			lwfree(vgeoms);
			/* we expect lwerror being called already by makeValid */
			return NULL;
		}
	}

	/* Collect areas and lines (if any line) */
	gout = GEOSGeom_createCollection_r(lwcontext()->geos_ctx, GEOS_GEOMETRYCOLLECTION, vgeoms, nvgeoms);
	if (!gout) /* an exception again */
	{
		/* cleanup and throw */
		for (i = 0; i < nvgeoms; ++i)
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, vgeoms[i]);
		lwfree(vgeoms);
		lwerror("GEOSGeom_createCollection() threw an error: %s", lwgeom_geos_errmsg);
		return NULL;
	}
	lwfree(vgeoms);

	return gout;
}

GEOSGeometry*
LWGEOM_GEOS_makeValid(const GEOSGeometry* gin)
{
	GEOSGeometry* gout;
	char ret_char;
#if POSTGIS_DEBUG_LEVEL >= 3
	LWGEOM *geos_geom;
	char *geom_ewkt;
#endif

	/*
	 * Step 2: return what we got so far if already valid
	 */

	ret_char = GEOSisValid_r(lwcontext()->geos_ctx, gin);
	if (ret_char == 2)
	{
		/* I don't think should ever happen */
		lwerror("GEOSisValid(): %s", lwgeom_geos_errmsg);
		return NULL;
	}
	else if (ret_char)
	{
#if POSTGIS_DEBUG_LEVEL >= 3
		geos_geom = GEOS2LWGEOM(gin, 0);
		geom_ewkt = lwgeom_to_ewkt(geos_geom);
		LWDEBUGF(3, "Geometry [%s] is valid. ", geom_ewkt);
		lwgeom_free(geos_geom);
		lwfree(geom_ewkt);
#endif

		/* It's valid at this step, return what we have */
		return GEOSGeom_clone_r(lwcontext()->geos_ctx, gin);
	}

#if POSTGIS_DEBUG_LEVEL >= 3
	geos_geom = GEOS2LWGEOM(gin, 0);
	geom_ewkt = lwgeom_to_ewkt(geos_geom);
	LWDEBUGF(3,
		 "Geometry [%s] is still not valid: %s. Will try to clean up further.",
		 geom_ewkt,
		 lwgeom_geos_errmsg);
	lwgeom_free(geos_geom);
	lwfree(geom_ewkt);
#endif

	/*
	 * Step 3 : make what we got valid
	 */

	switch (GEOSGeomTypeId_r(lwcontext()->geos_ctx, gin))
	{
	case GEOS_MULTIPOINT:
	case GEOS_POINT:
		/* points are always valid, but we might have invalid ordinate values */
		lwnotice("PUNTUAL geometry resulted invalid to GEOS -- dunno how to clean that up");
		return NULL;
		break;

	case GEOS_LINESTRING:
		gout = LWGEOM_GEOS_makeValidLine(gin);
		if (!gout) /* an exception or something */
		{
			/* cleanup and throw */
			lwerror("%s", lwgeom_geos_errmsg);
			return NULL;
		}
		break; /* we've done */

	case GEOS_MULTILINESTRING:
		gout = LWGEOM_GEOS_makeValidMultiLine(gin);
		if (!gout) /* an exception or something */
		{
			/* cleanup and throw */
			lwerror("%s", lwgeom_geos_errmsg);
			return NULL;
		}
		break; /* we've done */

	case GEOS_POLYGON:
	case GEOS_MULTIPOLYGON:
	{
		gout = LWGEOM_GEOS_makeValidPolygon(gin);
		if (!gout) /* an exception or something */
		{
			/* cleanup and throw */
			lwerror("%s", lwgeom_geos_errmsg);
			return NULL;
		}
		break; /* we've done */
	}

	case GEOS_GEOMETRYCOLLECTION:
	{
		gout = LWGEOM_GEOS_makeValidCollection(gin);
		if (!gout) /* an exception or something */
		{
			/* cleanup and throw */
			lwerror("%s", lwgeom_geos_errmsg);
			return NULL;
		}
		break; /* we've done */
	}

	default:
	{
		char* typname = GEOSGeomType_r(lwcontext()->geos_ctx, gin);
		lwnotice("ST_MakeValid: doesn't support geometry type: %s", typname);
		GEOSFree_r(lwcontext()->geos_ctx, typname);
		return NULL;
		break;
	}
	}

#if PARANOIA_LEVEL > 1
	/*
	 * Now check if every point of input is also found in output, or abort by returning NULL
	 *
	 * Input geometry was lwgeom_in
	 */
	{
		int loss;
		GEOSGeometry *pi, *po, *pd;

		/* TODO: handle some errors here...
		 * Lack of exceptions is annoying indeed,
		 * I'm getting old --strk;
		 */
		pi = GEOSGeom_extractUniquePoints_r(lwcontext()->geos_ctx, gin);
		po = GEOSGeom_extractUniquePoints_r(lwcontext()->geos_ctx, gout);
		pd = GEOSDifference_r(lwcontext()->geos_ctx, pi, po); /* input points - output points */
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, pi);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, po);
		loss = pd && !GEOSisEmpty_r(lwcontext()->geos_ctx, pd);
		GEOSGeom_destroy_r(lwcontext()->geos_ctx, pd);
		if (loss)
		{
			lwnotice("%s [%d] Vertices lost in LWGEOM_GEOS_makeValid", __FILE__, __LINE__);
			/* return NULL */
		}
	}
#endif /* PARANOIA_LEVEL > 1 */

	return gout;
}
#endif

/* Exported. Uses GEOS internally */
LWGEOM*
lwgeom_make_valid(LWGEOM* lwgeom_in)
{
	return lwgeom_make_valid_params(lwgeom_in, NULL);
}

/* Exported. Uses GEOS internally */
LWGEOM*
lwgeom_make_valid_params(LWGEOM* lwgeom_in, char* make_valid_params)
{
	int is3d;
	GEOSGeom geosgeom;
	GEOSGeometry* geosout;
	LWGEOM* lwgeom_out;
	int code = 0;

	LWDEBUG(1, "lwgeom_make_valid enter");

	is3d = FLAGS_GET_Z(lwgeom_in->flags);

	/*
	 * Step 1 : try to convert to GEOS, if impossible, clean that up first
	 *          otherwise (adding only duplicates of existing points)
	 */

	//initGEOS(lwgeom_geos_error, lwgeom_geos_error);
	GEOSContext_setNoticeHandler_r(lwcontext()->geos_ctx, lwnotice);
	GEOSContext_setErrorHandler_r(lwcontext()->geos_ctx, lwgeom_geos_error);

	lwgeom_out = lwgeom_make_geos_friendly(lwgeom_in);
	if (!lwgeom_out) lwerror("Could not make a geos friendly geometry out of input");

	LWDEBUGF(4, "Input geom %p made GEOS-valid as %p", lwgeom_in, lwgeom_out);

	geosgeom = LWGEOM2GEOS(lwgeom_out, 1);
	if ( lwgeom_in != lwgeom_out ) {
		lwgeom_free(lwgeom_out);
	}
	if (!geosgeom)
	{
		lwerror("Couldn't convert POSTGIS geom to GEOS: %s", lwgeom_geos_errmsg);
		return NULL;
	}
	else
	{
		LWDEBUG(4, "geom converted to GEOS");
	}

#if POSTGIS_GEOS_VERSION < 30800
	geosout = LWGEOM_GEOS_makeValid(geosgeom);
#elif POSTGIS_GEOS_VERSION < 31000
	geosout = GEOSMakeValid_r(lwcontext()->geos_ctx, geosgeom);
#else
	if (!make_valid_params) {
		geosout = GEOSMakeValid_r(lwcontext()->geos_ctx, geosgeom);
	}
	else {
		/*
		* Set up a parameters object for this
		* make valid operation before calling
		* it
		*/
		const char *value;
		char *param_list[OPTION_LIST_SIZE];
		char param_list_text[OPTION_LIST_SIZE];
		strncpy(param_list_text, make_valid_params, OPTION_LIST_SIZE-1);
		param_list_text[OPTION_LIST_SIZE-1] = '\0'; /* ensure null-termination */
		memset(param_list, 0, sizeof(param_list));
		code = option_list_geos_parse(param_list_text, param_list);
		if (!code)
		{
			GEOSGeom_destroy_r(lwcontext()->geos_ctx, geosgeom);
			lwerror("Option string entry '%s' lacks separator '='", param_list[0]);
		}
		GEOSMakeValidParams *params = GEOSMakeValidParams_create_r(lwcontext()->geos_ctx);
		value = option_list_search(param_list, "method");
		if (value) {
			if (strcasecmp(value, "linework") == 0) {
				GEOSMakeValidParams_setMethod_r(lwcontext()->geos_ctx, params, GEOS_MAKE_VALID_LINEWORK);
			}
			else if (strcasecmp(value, "structure") == 0) {
				GEOSMakeValidParams_setMethod_r(lwcontext()->geos_ctx, params, GEOS_MAKE_VALID_STRUCTURE);
			}
			else
			{
				GEOSMakeValidParams_destroy_r(lwcontext()->geos_ctx, params);
				GEOSGeom_destroy_r(lwcontext()->geos_ctx, geosgeom);
				lwerror("Unsupported value for 'method', '%s'. Use 'linework' or 'structure'.", value);
			}
		}
		value = option_list_search(param_list, "keepcollapsed");
		if (value) {
			if (strcasecmp(value, "true") == 0) {
				GEOSMakeValidParams_setKeepCollapsed_r(lwcontext()->geos_ctx, params, 1);
			}
			else if (strcasecmp(value, "false") == 0) {
				GEOSMakeValidParams_setKeepCollapsed_r(lwcontext()->geos_ctx, params, 0);
			}
			else
			{
				GEOSMakeValidParams_destroy_r(lwcontext()->geos_ctx, params);
				GEOSGeom_destroy_r(lwcontext()->geos_ctx, geosgeom);
				lwerror("Unsupported value for 'keepcollapsed', '%s'. Use 'true' or 'false'", value);
			}
		}
		geosout = GEOSMakeValidWithParams_r(lwcontext()->geos_ctx, geosgeom, params);
		GEOSMakeValidParams_destroy_r(lwcontext()->geos_ctx, params);
	}
#endif
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, geosgeom);
	if (!geosout) return NULL;

	lwgeom_out = GEOS2LWGEOM(geosout, is3d);
	GEOSGeom_destroy_r(lwcontext()->geos_ctx, geosout);

	if (lwgeom_is_collection(lwgeom_in) && !lwgeom_is_collection(lwgeom_out))
	{
		LWGEOM** ogeoms = lwalloc(sizeof(LWGEOM*));
		LWGEOM* ogeom;
		LWDEBUG(3, "lwgeom_make_valid: forcing multi");
		/* NOTE: this is safe because lwgeom_out is surely not lwgeom_in or
		 * otherwise we couldn't have a collection and a non-collection */
		assert(lwgeom_in != lwgeom_out);
		ogeoms[0] = lwgeom_out;
		ogeom = (LWGEOM*)lwcollection_construct(
		    MULTITYPE[lwgeom_out->type], lwgeom_out->srid, lwgeom_out->bbox, 1, ogeoms);
		lwgeom_out->bbox = NULL;
		lwgeom_out = ogeom;
	}

	lwgeom_out->srid = lwgeom_in->srid;
	return lwgeom_out;
}
