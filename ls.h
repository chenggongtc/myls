/*
 * ls.h - header file for ls.c
 * Gong Cheng, 9/28/2015
 * CS631 midterm project - ls(1)
 */


#ifndef CG_LS_H
#define CG_LS_H

#define FTS_NO_PRINT	1
#define FTS_PRINT	0

extern blksize_t blksize;
extern int       return_val;
extern int       term_width;

extern int f_atime;
extern int f_block;
extern int f_char;
extern int f_colacross;
extern int f_column;
extern int f_ctime;
extern int f_ino;
extern int f_line;
extern int f_kb;
extern int f_nonprint;
extern int f_numid;
extern int f_readable;
extern int f_totalsum;

/*
 * Passing as an argument to print functions.
 *
 * Each obj_xx_max represents the max length of the corresponding
 * attribute.
 */
typedef struct {
	FTSENT  *obj_list;	/* list of files */
	int64_t obj_size_total;	/* total size of the directory */
	int     obj_isroot;
	int     obj_block_max;
	int     obj_group_max;
	int     obj_ino_max;
	int     obj_link_max;
	int     obj_major_max;
	int     obj_minor_max;
	int     obj_size_max;
	int     obj_user_max;
} OBJ;

#endif
