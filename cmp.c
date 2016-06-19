/*
 * cmp.c - Compare functions for fts_open
 * Gong Cheng, 10/15/2015
 * CS631 midterm project - ls(1)
 */

#include <sys/stat.h>

#include <fts.h>
#include <string.h>

#include "cmp.h"
#include "ls.h"

int
cmp_lexico(const FTSENT *p, const FTSENT *q)
{
	return strcmp(p->fts_name, q->fts_name);
}

int
cmp_time(const FTSENT *p, const FTSENT *q)
{
	if (f_atime) {
		if (p->fts_statp->st_atime > q->fts_statp->st_atime)
			return -1;
		else if (p->fts_statp->st_atime < q->fts_statp->st_atime)
			return 1;
	} else if (f_ctime) {
		if (p->fts_statp->st_ctime > q->fts_statp->st_ctime)
			return -1;
		else if (p->fts_statp->st_ctime < q->fts_statp->st_ctime)
			return 1;
	} else {
		if (p->fts_statp->st_mtime > q->fts_statp->st_mtime)
			return -1;
		else if (p->fts_statp->st_mtime < q->fts_statp->st_mtime)
			return 1;
	}

	/* if time is the same, compare the names */
	return cmp_lexico(p, q);
	
}

int
cmp_size(const FTSENT *p, const FTSENT *q)
{
	if (p->fts_statp->st_size > q->fts_statp->st_size)
		return 1;
	else if (p->fts_statp->st_size < q->fts_statp->st_size)
		return -1;

	/* if sizes are the same, compare the names */
	return cmp_lexico(p, q);
}
