/*
 * ls.c	- main program for `ls(1)` command
 * ls	- list directory contents
 * Gong Cheng, 10/15/2015
 * CS631 midterm project - ls(1)
 *
 * Usage: ./ls [-AaCcdFfhiklnqRrSstuwx1] [file ...]
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <bsd/stdlib.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cmp.h"
#include "ls.h"
#include "print.h"

#define SINGLE_ARG	1
#define MULTI_ARG	0
#define ROOT		0
#define CHILD		1
#define DEFAULT_BSIZE	512
#define MAX(a, b)	(a > b ? a : b)

#define COUNT_LENTH(x)			\
temp_count = 0;				\
temp_num = x;				\
do {					\
	temp_num /= 10;			\
	temp_count++;			\
} while (temp_num > 0)

static void	traverse(int, char * const *, int);
static void	print_file(int, FTSENT *);
static int	fts_cmp(const FTSENT **, const FTSENT **);
static void	usage();

blksize_t blksize;	/* 512 by default, or same as BLOCKSIZE */
int       term_width;	/* the width of the terminal */
int       return_val;	/* return value for main */

/* flags */
int f_atime;		/* use atime instead of mtime */
int f_block;		/* show number of blocks */
int f_char;		/* special charecter after pathname */
int f_ctime;		/* use ctime instead of mtime */
int f_column;		/* list output by columns */
int f_colacross;	/* sort output across columns */
int f_dir;		/* show directory entries instead of content */
int f_ino;		/* show inode number */
int f_kb;		/* show size in kilobytes */
int f_listall;		/* list all except '.' and '..'  */
int f_listdot;		/* list '.' and '..' */
int f_line;		/* list one entry per line */
int f_long;		/* long format */
int f_nonprint;		/* show non-printable characters as ? */
int f_nosort;		/* do not sort output */
int f_numid;		/* show numerical user and group ID */
int f_readable;		/* show bytes in readable format */
int f_recursive;	/* list subdirectories recurisvely */
int f_reverse;		/* reverse the order of output */
int f_sortmtime;	/* sort by mtime */
int f_sortsize;		/* sort by size */
int f_totalsum;		/* show total sum for all file sizes */

/*
 * The program displays the operands with the associated information
 * specified by the options.  If the type of a operand is directory, the
 * program displays the files contained within it, otherwise the program
 * displays the file itself.
 */
int
main(int argc, char **argv) {
	struct winsize w;
	int    opt;
	int    options;
	char   *curr[2];
	char   *blk;

	term_width = 80;
	options = FTS_PHYSICAL;
	return_val = EXIT_SUCCESS;

	/* curr = "." */
	curr[0] = ".";
	curr[1] = NULL;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	if (isatty(STDOUT_FILENO) == 1) {
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
			term_width = w.ws_col;

		/* default options for output to a terminal */
		f_column = f_nonprint = f_totalsum = 1;
	} else {
		if (errno == EBADF) {
			err(EXIT_FAILURE, "Output error");
		}

		/* default option for output to a file */
		f_line = 1;
	}

	/* set -A for root */
	if (getuid() == ROOT)
		f_listall = 1;

	/* get BLOCKSIZE, or 512 by default */
	if ((blk = getenv("BLOCKSIZE")) == NULL)
		blksize = 512;
	else {
		blksize = atoi(blk);

		/* prevent BLOCKSIZE from being set to an non-positive number */
		blksize = blksize <= 0 ? DEFAULT_BSIZE : blksize;
	}

	while ((opt = getopt(argc, argv, "AaCcdFfhiklnqRrSstuwx1")) != -1) {
		switch(opt) {
		case 'A':
			f_listall = 1;
			break;
		case 'a':
			f_listall = f_listdot = 1;
			break;
		case 'C':
			f_column = 1;
			f_numid = f_colacross = f_line = f_long = 0;
			break;
		case 'c':
			f_ctime = 1;
			f_atime = 0;
			break;
		case 'd':
			f_dir = 1;
			break;
		case 'F':
			f_char = 1;
			break;
		case 'f':
			f_nosort = 1;
			break;
		case 'h':
			f_readable = 1;
			f_kb = 0;
			break;
		case 'i':
			f_ino = 1;
			break;
		case 'k':
			f_kb = 1;
			f_readable = 0;
			break;
		case 'l':
			f_long = 1;
			f_column = f_colacross = f_line = f_numid = 0;
			break;
		case 'n':
			f_numid = f_long = 1;
			f_column = f_colacross = f_line = 0;
			break;
		case 'q':
			f_nonprint = 1;
			break;
		case 'R':
			f_recursive = 1;
			break;
		case 'r':
			f_reverse = 1;
			break;
		case 'S':
			f_sortsize = 1;
			break;
		case 's':
			f_block = 1;
			break;
		case 't':
			f_sortmtime = 1;
			break;
		case 'u':
			f_atime = 1;
			f_ctime = 0;
			break;
		case 'w':
			f_nonprint = 0;
			break;
		case 'x':
			f_colacross = 1;
			f_column = f_numid = f_long = f_line = 0;
			break;
		case '1':
			f_line = 1;
			f_column = f_colacross = f_numid = f_long = 0;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

/*	if (!f_long && !f_ino && !f_block && !f_atime && !f_ctime
		 && !f_sortmtime && !f_sortsize && !f_char)
		options |= FTS_NOSTAT;
*/
	if (f_listdot)
		options |= FTS_SEEDOT;

	if (argc == 0) {
		/* if no operand is specified, then list current directory */
		traverse(SINGLE_ARG, curr, options);
	} else {
		traverse(argc == 1 ? SINGLE_ARG : MULTI_ARG, argv, options);
	}

	return return_val;
}

/* 
 * Traverse the file hierarchy by specified options.  single_argc is used
 * for formatting output. 
 */
static void
traverse(int single_argc, char * const *argv, int options)
{
	FTS    *handle;
	FTSENT *file, *child;

	if ((handle = fts_open(argv, options, f_nosort ? NULL : fts_cmp))
	    == NULL) {
		err(EXIT_FAILURE, "Failed to traverse");	
	}

	if ((child = fts_children(handle, 0)) == NULL) {
		if (errno != 0)
			err(EXIT_FAILURE, "Failed to search the operands");
	}

	print_file(ROOT, child);

	if (f_dir)
		goto end;

	while ((file = fts_read(handle)) != NULL) {
		if (file->fts_info == FTS_ERR || file->fts_info == FTS_NS) {
			warnx("%s: file error: %s", file->fts_name,
				strerror(file->fts_errno));

			return_val = EXIT_FAILURE;
		} else if (file->fts_info == FTS_DC) {
			warnx("%s: directory error: %s", file->fts_name,
				strerror(file->fts_errno));

			return_val = EXIT_FAILURE;
		} else if (file->fts_info == FTS_D) {
			/*
			 * Skip hidden directories if -a or -A not specified,
			 * except the hidden directories are the operands.
			 */
			if (!f_listall && file->fts_level != ROOT &&
			    file->fts_name[0] == '.') {
				if (fts_set(handle, file, FTS_SKIP) == -1) {
					warn("%s: failed to skip", 
					     file->fts_name);
					return_val = EXIT_FAILURE;
				}
				continue;
			}

			if (single_argc != SINGLE_ARG)
				printf("\n%s:\n",file->fts_path);
			else
				single_argc = 0;

			if ((child = fts_children(handle, 0)) == NULL) {
				if (errno != 0) {
					warn("%s: failed to search",
						file->fts_name);

					return_val = EXIT_FAILURE;
					continue;
				}
			}

			print_file(CHILD, child);

			/* stop searching subdirectories when -R not set */
			if (!f_recursive)
				if (fts_set(handle, file, FTS_SKIP) == -1) {
					warn("%s: failed to skip", 
					     file->fts_name);
					return_val = EXIT_FAILURE;
				}
		}

	}

	/* fts_read return NULL and errno is set */
	if (errno != 0) {
		warn("FTS read error");
		return_val = EXIT_FAILURE;
	}

end:	(void)fts_close(handle);

}

static int
fts_cmp(const FTSENT **p, const FTSENT **q)
{
	int            ret;
	unsigned short info_p, info_q;

	info_p = (*p)->fts_info;
	info_q = (*q)->fts_info;

	/* error file comes first */
	if (info_p == FTS_ERR)
		return  -1;
	else if (info_q == FTS_ERR)
		return 1;
	else if (info_p == FTS_NS || info_q == FTS_NS) {
		if (info_q != FTS_NS)
			return -1;
		else if (info_p != FTS_NS)
			return 1;

		ret = cmp_lexico(*p, *q);

		/* reverse the order if -r specified */
		if (!f_reverse)
			return ret;
		else
			return -ret;
	}

	/* sort by different options */
	if (f_sortmtime)
		ret = cmp_time(*p, *q);
	else if (f_sortsize)
		ret = cmp_size(*p, *q);
	else
		ret = cmp_lexico(*p, *q);

	if (!f_reverse)
		return ret;
	else
		return -ret;
}	

static void
print_file(int parent, FTSENT *child)
{
	OBJ    obj;
	FTSENT *list;

	struct stat   *st;
	struct passwd *pwd;
	struct group  *grp;

	int64_t size_total;
	int64_t temp_num;
	int     temp_count;	/* count length */

	/* max length for each field when output */
	int     block_max;
	int     group_max;
	int     ino_max;
	int     link_max;
	int     major_max;
	int     minor_max;
	int     size_max;
	int     user_max;

	obj.obj_isroot = 0;
	size_total = 0;
	block_max = group_max = ino_max = link_max = major_max = minor_max = 
	size_max = user_max = 1;
	
	/*
	 * Preprocess the files' information before output.  The fts_number
	 * field is used as the flag to determine whether to output the file
	 * or not.
	 *
	 * fts_number is used for determining if the file should be output
	 * or not.
	 */
	for (list = child; list != NULL; list = list->fts_link) {
		if (list->fts_info == FTS_ERR || list->fts_info == FTS_NS) {
			warnx("%s: file error: %s", list->fts_name,
				strerror(list->fts_errno));

			return_val = EXIT_FAILURE;
			list->fts_number = FTS_NO_PRINT;
			continue;
		}  else if (list->fts_info == FTS_DC || 
			    list->fts_info == FTS_DNR) {
			warnx("%s: directory error: %s", list->fts_name,
				strerror(list->fts_errno));

			return_val = EXIT_FAILURE;
			list->fts_number = FTS_NO_PRINT;
			continue;
		} else if (list->fts_info == FTS_DP) {
			list->fts_number = FTS_NO_PRINT;
			continue;
		}

		if (parent == ROOT) {
			obj.obj_isroot = 1;
			if (!f_dir && list->fts_info == FTS_D) {
				list->fts_number = FTS_NO_PRINT;
				continue;
			}
		} else {
			obj.obj_isroot = 0;
			if (!f_listall && list->fts_name[0] == '.') {
				list->fts_number = FTS_NO_PRINT;
				continue;
			}
		}

		if ((st = list->fts_statp) == NULL) {
			warn("%s: cannot stat", list->fts_name);

			list->fts_number = FTS_NO_PRINT;
			return_val = EXIT_FAILURE;
			continue;
		}

		size_total += st->st_blocks * DEFAULT_BSIZE;

		/* count max width of block and size */
		if (f_readable) {
			block_max = size_max = 5;	/* i.e. 1023K */
		} else if (f_kb) {
			COUNT_LENTH(st->st_blocks * DEFAULT_BSIZE / 1024);
			block_max = MAX(block_max, temp_count);

			COUNT_LENTH(st->st_size / 1024);
			size_max = MAX(size_max, temp_count);
		} else {
			COUNT_LENTH(st->st_blocks * DEFAULT_BSIZE / blksize);
			block_max = MAX(block_max, temp_count);

			COUNT_LENTH(st->st_size);
			size_max = MAX(size_max, temp_count);
		}

		/* count max width of ino */
		COUNT_LENTH(st->st_ino);
		ino_max = MAX(ino_max, temp_count);

		/* count max width of links */
		COUNT_LENTH(st->st_nlink);
		link_max = MAX(link_max, temp_count);

		/* count max width of major and minor device */
		if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
			COUNT_LENTH(major(st->st_rdev));
			major_max = MAX(major_max, temp_count);

			COUNT_LENTH(minor(st->st_rdev));
			minor_max = MAX(minor_max, temp_count);
		}

		/* count max width of user and group */
		if (f_numid) {
			COUNT_LENTH(st->st_uid);
			user_max = MAX(user_max, temp_count);

			COUNT_LENTH(st->st_gid);
			group_max = MAX(group_max, temp_count);
		} else {
			if ((pwd = getpwuid(st->st_uid)) == NULL) {
				if (errno != 0)
					return_val = EXIT_FAILURE;

				COUNT_LENTH(st->st_uid);
			} else
				temp_count = strlen(pwd->pw_name);

			user_max = MAX(user_max, temp_count);

			if ((grp = getgrgid(st->st_gid)) == NULL) {
				if (errno != 0)
					return_val = EXIT_FAILURE;

			COUNT_LENTH(st->st_gid);
			} else
				temp_count = strlen(grp->gr_name);

			group_max = MAX(group_max, temp_count);
		}

	}

	/* keep the size width the same */
	if (size_max < major_max + minor_max + 2)
		size_max = major_max + minor_max + 2;
	else
		major_max = size_max - 2 - minor_max;

	obj.obj_list = child;
	obj.obj_size_total = size_total;
	obj.obj_block_max = block_max;
	obj.obj_group_max = group_max;
	obj.obj_ino_max = ino_max;
	obj.obj_link_max = link_max;
	obj.obj_major_max = major_max;
	obj.obj_minor_max = minor_max;
	obj.obj_size_max = size_max;
	obj.obj_user_max = user_max;

	if(f_long)
		print_long(&obj);
	else 
		print_short(&obj);
}

static void
usage()
{
	fprintf(stderr, "Usage: ./%s [-AaCcdFfhiklnqRrSstuwx1] [file ...]\n",
		getprogname());
	exit(EXIT_FAILURE);
}


