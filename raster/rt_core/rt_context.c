/*
 *
 * WKTRaster - Raster Types for PostGIS
 * http://trac.osgeo.org/postgis/wiki/WKTRaster
 *
 * Copyright (C) 2011-2013 Regents of the University of California
 *   <bkpark@ucdavis.edu>
 * Copyright (C) 2010-2011 Jorge Arevalo <jorge.arevalo@deimos-space.com>
 * Copyright (C) 2010-2011 David Zwarg <dzwarg@azavea.com>
 * Copyright (C) 2009-2011 Pierre Racine <pierre.racine@sbf.ulaval.ca>
 * Copyright (C) 2009-2011 Mateusz Loskot <mateusz@loskot.net>
 * Copyright (C) 2008-2009 Sandro Santilli <strk@kbt.io>
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

#include <stdarg.h> /* for va_list, va_start etc */

#include "librtcore.h"
#include "librtcore_internal.h"

/******************************************************************************
* rt_context
******************************************************************************/

/*
 * Default allocators
 *
 * We include some default allocators that use malloc/free/realloc
 * along with stdout/stderr since this is the most common use case
 *
 */
void *
default_rt_allocator(size_t size)
{
    void *mem = malloc(size);
    return mem;
}

void *
default_rt_reallocator(void *mem, size_t size)
{
    void *ret = realloc(mem, size);
    return ret;
}

void
default_rt_deallocator(void *mem)
{
    free(mem);
}

void
default_rt_error_handler(const char *fmt, va_list ap) {

    static const char *label = "ERROR: ";
    char newfmt[1024] = {0};
    snprintf(newfmt, 1024, "%s%s\n", label, fmt);
    newfmt[1023] = '\0';

    vprintf(newfmt, ap);

    va_end(ap);
}

void
default_rt_warning_handler(const char *fmt, va_list ap) {

    static const char *label = "WARNING: ";
    char newfmt[1024] = {0};
    snprintf(newfmt, 1024, "%s%s\n", label, fmt);
    newfmt[1023] = '\0';

    vprintf(newfmt, ap);

    va_end(ap);
}

void
default_rt_info_handler(const char *fmt, va_list ap) {

    static const char *label = "INFO: ";
    char newfmt[1024] = {0};
    snprintf(newfmt, 1024, "%s%s\n", label, fmt);
    newfmt[1023] = '\0';

    vprintf(newfmt, ap);

    va_end(ap);
}

char * default_rt_options(const char* varname) {
	return NULL;
}


/**
 * Struct definition here
 */
struct rt_context_t {
    rt_allocator alloc;
    rt_reallocator realloc;
    rt_deallocator dealloc;
    rt_message_handler err;
    rt_message_handler warn;
    rt_message_handler info;
    rt_options options;
};

/* Static variable, to be used for all rt_core functions */
static struct rt_context_t ctx_t = {
    .alloc = default_rt_allocator,
    .realloc = default_rt_reallocator,
    .dealloc = default_rt_deallocator,
    .err = default_rt_error_handler,
    .warn = default_rt_warning_handler,
    .info = default_rt_info_handler,
    .options = default_rt_options
};


/**
 * Useful in raster core testing and in the (future)
 * loader, when we need to use raster core functions but we don't have
 * PostgreSQL backend behind. We must take care of memory by ourselves in those
 * situations
 */
void
rt_install_default_allocators(void)
{
    ctx_t.alloc = default_rt_allocator;
    ctx_t.realloc = default_rt_reallocator;
    ctx_t.dealloc = default_rt_deallocator;
    ctx_t.err = default_rt_error_handler;
    ctx_t.info = default_rt_info_handler;
    ctx_t.warn = default_rt_warning_handler;
    ctx_t.options = default_rt_options;
}


/**
 * This function is called when the PostgreSQL backend is
 * taking care of the memory and we want to use palloc family
 */
void
rt_set_handlers(rt_allocator allocator, rt_reallocator reallocator,
    rt_deallocator deallocator, rt_message_handler error_handler,
    rt_message_handler info_handler, rt_message_handler warning_handler)
{
    rt_set_handlers_options(allocator, reallocator, deallocator,
    	error_handler, info_handler, warning_handler,
        default_rt_options);
}

void
rt_set_handlers_options(rt_allocator allocator, rt_reallocator reallocator,
    rt_deallocator deallocator, rt_message_handler error_handler,
    rt_message_handler info_handler, rt_message_handler warning_handler,
    rt_options options_handler)
{
    ctx_t.alloc   = allocator;
    ctx_t.realloc = reallocator;
    ctx_t.dealloc = deallocator;

    ctx_t.err  = error_handler;
    ctx_t.info = info_handler;
    ctx_t.warn = warning_handler;

    ctx_t.options = options_handler;
}

/**
 * Raster core memory management functions.
 *
 * They use the functions defined by the caller.
 */
void *
rtalloc(size_t size) {
    void * mem = ctx_t.alloc(size);
    RASTER_DEBUGF(5, "rtalloc called: %d@%p", size, mem);
    return mem;
}


void *
rtrealloc(void * mem, size_t size) {
    void * result = ctx_t.realloc(mem, size);
    RASTER_DEBUGF(5, "rtrealloc called: %d@%p", size, result);
    return result;
}

void
rtdealloc(void * mem) {
    ctx_t.dealloc(mem);
    RASTER_DEBUG(5, "rtdealloc called");
}

/**
 * Raster core error and info handlers
 *
 * Since variadic functions cannot pass their parameters directly, we need
 * wrappers for these functions to convert the arguments into a va_list
 * structure.
 */
void
rterror(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);

    /* Call the supplied function */
    (*ctx_t.err)(fmt, ap);

    va_end(ap);
}

void
rtinfo(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);

    /* Call the supplied function */
    (*ctx_t.info)(fmt, ap);

    va_end(ap);
}


void
rtwarn(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);

    /* Call the supplied function */
    (*ctx_t.warn)(fmt, ap);

    va_end(ap);
}

char *
rtoptions(const char *varname) {
	if (!ctx_t.options)
		return NULL;
	return ctx_t.options(varname);
}

char *
rtstrdup(const char *str) {
	size_t sz;
	char* dup;
	if (!str) return NULL;
	sz = strlen(str) + 1;
	dup = rtalloc(sz);
	memcpy(dup, str, sz);
	return dup;
}


const char*
rtcore_version()
{
    static char* rt_ver = LIBLWGEOM_VERSION;

    return rt_ver;
}