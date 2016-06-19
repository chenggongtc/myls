/*
 * cmp.h - header file for cmp.c
 * Gong Cheng, 10/1/2015
 * CS631 midterm project - ls(1)
 */

#ifndef CG_CMP_H
#define CG_CMP_H

int cmp_lexico(const FTSENT *, const FTSENT *);
int cmp_time(const FTSENT *, const FTSENT *);
int cmp_size(const FTSENT *, const FTSENT *);

#endif
