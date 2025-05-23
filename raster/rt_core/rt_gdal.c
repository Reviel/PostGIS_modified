/*
 *
 * WKTRaster - Raster Types for PostGIS
 * http://trac.osgeo.org/postgis/wiki/WKTRaster
 *
 * Copyright (C) 2021 Paul Ramsey <pramsey@cleverelephant.ca>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "../../postgis_config.h"
/* #define POSTGIS_DEBUG_LEVEL 4 */

#include "librtcore.h"
#include "librtcore_internal.h"
#include "stringbuffer.h"

/******************************************************************************
* rt_raster_gdal_contour()
******************************************************************************/

typedef struct
{
	struct {
		GDALDatasetH ds;
		GDALDriverH drv;
		int destroy_drv;
	} src;

	struct {
		OGRSFDriverH drv;
		OGRDataSourceH ds;
		OGRLayerH lyr;
		int srid;
		OGRwkbGeometryType gtype;
	} dst;

} _rti_contour_arg;


/* ---------------------------------------------------------------- */
/*  GDAL progress callback for interrupt handling */
/* ---------------------------------------------------------------- */

int rt_util_gdal_progress_func(
	double dfComplete,
	const char *pszMessage,
	void *pProgressArg)
{
	(void)dfComplete;
	(void)pszMessage;

	if (lwgeom_interrupt_state())
	{
		// rtwarn("%s interrupted at %g", (const char*)pProgressArg, dfComplete);
		lwgeom_cancel_interrupt();
		return FALSE;
	}
	else
		return TRUE;
}

static void
_rti_contour_arg_init(_rti_contour_arg* arg)
{
	memset(arg, 0, sizeof(_rti_contour_arg));
	return;
}


static int
_rti_contour_arg_destroy(_rti_contour_arg* arg)
{
	if(arg->src.ds != NULL)
		GDALClose(arg->src.ds);

	if (arg->src.drv != NULL && arg->src.destroy_drv) {
		GDALDeregisterDriver(arg->src.drv);
		GDALDestroyDriver(arg->src.drv);
	}

	if (arg->dst.ds != NULL)
		OGR_DS_Destroy(arg->dst.ds);

	return FALSE;
}



/**
 * Return palloc'ed list of contours.
 * @param src_raster : raster to generate contour from
 * @param src_band : band to use as input
 * @param src_srid : srid of raster
 * @param src_srs : Coordinate reference system string for raster
 * @param options : CSList of OPTION=VALUE strings for the
 *   contour routine, see https://gdal.org/api/gdal_alg.html?highlight=contour#_CPPv419GDALContourGenerate15GDALRasterBandHddiPdidPvii16GDALProgressFuncPv
 * @param ncontours : Output parameter for length of contour list
 * @param contours : Output palloc'ed list of contours, caller to free
 */
int rt_raster_gdal_contour(
	/* input parameters */
	rt_raster src_raster,
	int src_band,
	int src_srid,
	const char* src_srs,
	double contour_interval,
	double contour_base,
	int fixed_level_count,
	double *fixed_levels,
	int polygonize,
	/* output parameters */
	size_t *ncontours,
	struct rt_contour_t **contours
	)
{
	CPLErr cplerr;
	OGRErr ogrerr;
	GDALRasterBandH hBand;
	int nfeatures = 0, i = 0;
	OGRFeatureH hFeat;

	/* For building out options list */
	stringbuffer_t sb;
	char **papszOptList = NULL;
	const char* elev_field = polygonize ? "ELEV_FIELD_MIN" : "ELEV_FIELD";

	_rti_contour_arg arg;
	_rti_contour_arg_init(&arg);

	/* Load raster into GDAL memory */
	arg.src.ds = rt_raster_to_gdal_mem(src_raster, src_srs, NULL, NULL, 0, &(arg.src.drv), &(arg.src.destroy_drv));
	/* Extract the desired band */
	hBand = GDALGetRasterBand(arg.src.ds, src_band);

	RASTER_DEBUG(3, "creating OGR MEM vector");

	/* Set up the OGR destination data store */
	arg.dst.srid = src_srid;
	arg.dst.drv = OGRGetDriverByName("Memory");
	if (!arg.dst.drv)
		return _rti_contour_arg_destroy(&arg);

	arg.dst.ds = OGR_Dr_CreateDataSource(arg.dst.drv, "contour_ds", NULL);
	if (!arg.dst.ds)
		return _rti_contour_arg_destroy(&arg);

	/* Polygonize is 2.4+ only */
#if POSTGIS_GDAL_VERSION >= 24
	arg.dst.gtype = polygonize ? wkbPolygon : wkbLineString;
#else
	arg.dst.gtype = wkbLineString;
#endif

	/* Layer has geometry, elevation, id */
	arg.dst.lyr = OGR_DS_CreateLayer(arg.dst.ds, "contours", NULL, arg.dst.gtype, NULL);
	if (!arg.dst.lyr)
		return _rti_contour_arg_destroy(&arg);

	/* ID field */
	OGRFieldDefnH hFldId = OGR_Fld_Create("id", OFTInteger);
	ogrerr = OGR_L_CreateField(arg.dst.lyr, hFldId, TRUE);
	OGR_Fld_Destroy(hFldId);
	if (ogrerr != OGRERR_NONE)
		return _rti_contour_arg_destroy(&arg);

	/* ELEVATION field */
	OGRFieldDefnH hFldElevation = OGR_Fld_Create("elevation", OFTReal);
	ogrerr = OGR_L_CreateField(arg.dst.lyr, hFldElevation, TRUE);
	OGR_Fld_Destroy(hFldElevation);
	if (ogrerr != OGRERR_NONE)
		return _rti_contour_arg_destroy(&arg);

	int use_no_data = 0;
	double no_data_value = GDALGetRasterNoDataValue(hBand, &use_no_data);

	// LEVEL_INTERVAL=f
	//   The elevation interval between contours generated.
	// LEVEL_BASE=f
	//   The "base" relative to which contour intervals are applied. This is normally zero, but could be different. To generate 10m contours at 5, 15, 25, ... the LEVEL_BASE would be 5.
	// LEVEL_EXP_BASE=f
	//   If greater than 0, contour levels are generated on an exponential scale. Levels will then be generated by LEVEL_EXP_BASE^k where k is a positive integer.
	// FIXED_LEVELS=f[,f]*
	//   The list of fixed contour levels at which contours should be generated. This option has precedence on LEVEL_INTERVAL
	// NODATA=f
	//   The value to use as a "nodata" value. That is, a pixel value which should be ignored in generating contours as if the value of the pixel were not known.
	// ID_FIELD=d
	//   This will be used as a field index to indicate where a unique id should be written for each feature (contour) written.
	// ELEV_FIELD=d
	//   This will be used as a field index to indicate where the elevation value of the contour should be written. Only used in line contouring mode.
	// ELEV_FIELD_MIN=d
	//   This will be used as a field index to indicate where the minimum elevation value of the polygon contour should be written. Only used in polygonal contouring mode.
	// ELEV_FIELD_MAX=d
	//   This will be used as a field index to indicate where the maximum elevation value of the polygon contour should be written. Only used in polygonal contouring mode.
	// POLYGONIZE=YES|NO

	/* Options strings list */
	stringbuffer_init(&sb);

	if (use_no_data)
		stringbuffer_aprintf(&sb, "NODATA=%g ", no_data_value);

	if (fixed_level_count > 0) {
		int i = 0;
		stringbuffer_append(&sb, "FIXED_LEVELS=");
		for (i = 0; i < fixed_level_count; i++) {
			if (i) stringbuffer_append_char(&sb, ',');
			stringbuffer_aprintf(&sb, "%g", fixed_levels[i]);
		}
		stringbuffer_append_char(&sb, ' ');
	}
	else {
		stringbuffer_aprintf(&sb, "LEVEL_INTERVAL=%g ", contour_interval);
		stringbuffer_aprintf(&sb, "LEVEL_BASE=%g ", contour_base);
	}

	stringbuffer_aprintf(&sb, "ID_FIELD=%d ", 0);
	stringbuffer_aprintf(&sb, "%s=%d ", elev_field, 1);

	stringbuffer_aprintf(&sb, "POLYGONIZE=%s ", polygonize ? "YES" : "NO");

	papszOptList = CSLTokenizeString(stringbuffer_getstring(&sb));

	// CPLSetConfigOption("OGR_GEOMETRY_ACCEPT_UNCLOSED_RING", "NO");

	/* Run the contouring routine, filling up the OGR layer */
	cplerr = GDALContourGenerateEx(
		hBand,
		arg.dst.lyr,
		papszOptList,      // Options
		(GDALProgressFunc)rt_util_gdal_progress_func,  // GDALProgressFunc pfnProgress
		(void*)"GDALContourGenerateEx" // void *pProgressArg
		);

	// /* Run the contouring routine, filling up the OGR layer */
	// cplerr = GDALContourGenerate(
	// 	hBand,
	// 	contour_interval, contour_base,
	// 	fixed_level_count, fixed_levels,
	// 	use_no_data, no_data_value,
	// 	arg.dst.lyr, 0, 1, // OGRLayer, idFieldNum, elevFieldNum
	// 	NULL,              // GDALProgressFunc pfnProgress
	// 	NULL               // void *pProgressArg
	// 	);

	CSLDestroy(papszOptList);
	if (cplerr >= CE_Failure) {
		return _rti_contour_arg_destroy(&arg); // FALSE
	}

	/* Convert the OGR layer into PostGIS geometries */
	nfeatures = OGR_L_GetFeatureCount(arg.dst.lyr, TRUE);
	if (nfeatures < 0)
		return _rti_contour_arg_destroy(&arg);

	*contours = rtalloc(sizeof(struct rt_contour_t) * nfeatures);
	OGR_L_ResetReading(arg.dst.lyr);
	while ((hFeat = OGR_L_GetNextFeature(arg.dst.lyr))) {
		size_t szWkb;
		unsigned char *bufWkb;
		struct rt_contour_t contour;
		OGRGeometryH hGeom;
		LWGEOM *geom;

		/* Somehow we're still iterating, should not happen. */
		if(i >= nfeatures) break;

		/* Store elevation/id */
		contour.id = OGR_F_GetFieldAsInteger(hFeat, 0);
		contour.elevation = OGR_F_GetFieldAsDouble(hFeat, 1);
		/* Convert OGR geometry to LWGEOM via WKB */
		if (!(hGeom = OGR_F_GetGeometryRef(hFeat))) continue;
		szWkb = OGR_G_WkbSize(hGeom);
		bufWkb = rtalloc(szWkb);
		if (OGR_G_ExportToWkb(hGeom, wkbNDR, bufWkb) != OGRERR_NONE) continue;
		/* Reclaim feature and associated geometry memory */
		OGR_F_Destroy(hFeat);
		geom = lwgeom_from_wkb(bufWkb, szWkb, LW_PARSER_CHECK_NONE);
		lwgeom_set_srid(geom, arg.dst.srid);
		contour.geom = gserialized_from_lwgeom(geom, NULL);
		lwgeom_free(geom);
		rtdealloc(bufWkb);
		(*contours)[i++] = contour;
	}

	/* Return total number of contours saved */
	*ncontours = i;

	/* Free all the non-database allocated structures */
	_rti_contour_arg_destroy(&arg);
	stringbuffer_release(&sb);

	/* Done */
	return TRUE;
}

