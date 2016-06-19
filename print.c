/*
 * print.c - print functions
 * Gong Cheng, 10/4/2015
 * CS631 midterm project - ls(1)
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <bsd/stdlib.h>
#include <bsd/string.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "ls.h"
#include "print.h"

#define WITH_LINK	1
#define NO_LINK		0
#define DEFAULT_BSIZE	512
#define BLOCK_BUFF_SIZE	5	/* i.e. 1023k */
#define SIZE_BUFF_SIZE	5	/* i.e. 1023k */
#define MODE_BUFF_SIZE	12	/* i.e. -rwxrwxrwx+ */
#define ROOT		0

#define MAX(a, b) (a > b ? a : b)

/* the last 1 or 2 int arguments are used for formatting */
static void print_total(int64_t);
static void print_block(int64_t, int);
static void print_user_and_group(uid_t, gid_t, int, int);
static void print_size(off_t, int);
static void print_device(dev_t, int, int);
static void print_date(struct stat *);
static int  print_name(FTSENT *, int);
static void print_link(FTSENT *);
static void print_line(OBJ *);
static void print_column(OBJ *);
static void print_colacross(OBJ *);

void
print_long(OBJ *obj)
{
	struct stat *st;
	FTSENT      *p;
	char        mode[MODE_BUFF_SIZE];

	p = obj->obj_list;

	if(!obj->obj_isroot)
		print_total(obj->obj_size_total);

	while (p != NULL) {
		if (p->fts_number == FTS_NO_PRINT) {
			p = p->fts_link;
			continue;
		}

		st = p->fts_statp;

		/* print inode */
		if (f_ino)
			printf("%*"PRIuMAX" ", obj->obj_ino_max,
				(uintmax_t)st->st_ino);

		/* print number of blocks */
		if (f_block) {	
			print_block((int64_t)st->st_blocks * DEFAULT_BSIZE, 
				    obj->obj_block_max);
		}
		
		/* print mode */
		strmode(st->st_mode, mode);
		printf("%11s ", mode);

		/* print number of links */
		printf("%*"PRIuMAX" ", obj->obj_link_max, 
			(uintmax_t)st->st_nlink);

		/* print owner and group */
		print_user_and_group(st->st_uid, st->st_gid, 
				      obj->obj_user_max, obj->obj_group_max);

		/* print size */
		if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
			print_device(st->st_rdev, obj->obj_major_max,
					obj->obj_minor_max);
		else
			print_size(st->st_size, obj->obj_size_max);

		print_date(st);
		print_name(p, WITH_LINK);

		putchar('\n');

		p = p->fts_link;
	}
}

void
print_short(OBJ *obj)
{
	if (f_totalsum && !obj->obj_isroot)
		print_total(obj->obj_size_total);

	if (f_line)
		print_line(obj);
	else if(f_column)
		print_column(obj);
	else
		print_colacross(obj);
}

static void
print_total(int64_t size)
{
	char block_buff[BLOCK_BUFF_SIZE];

	if (f_readable) {
		if (humanize_number(block_buff, BLOCK_BUFF_SIZE, 
		    size / blksize, "", 
		    HN_AUTOSCALE, HN_DECIMAL | HN_NOSPACE) == -1) {
			/* transformation failed, uses '?' to represent */
			block_buff[0] = '?';
			block_buff[1] = '\0';

			return_val = EXIT_FAILURE;
		}

			printf("total %s\n", block_buff);
	} else if (f_kb) {
		printf("total %"PRIuMAX"\n", (uintmax_t)(size / 1024));
	} else {
		printf("total %"PRIuMAX"\n", (uintmax_t)(size / blksize));
	}

}

static void
print_block(int64_t size, int block_max)
{
	char block_buff[BLOCK_BUFF_SIZE];

	if (f_readable) {
		if (humanize_number(block_buff, BLOCK_BUFF_SIZE, 
		    size / blksize, "", 
		    HN_AUTOSCALE, HN_DECIMAL | HN_NOSPACE) == -1) {
			/* transformation failed, uses '?' to represent */
			block_buff[0] = '?';
			block_buff[1] = '\0';

			return_val = EXIT_FAILURE;
		}

			printf("%*s ", block_max, block_buff);
	} else if (f_kb) {
		printf("%*"PRIuMAX" ", block_max, 
			(uintmax_t)(size / 1024));
	} else {
		printf("%*"PRIuMAX" ", block_max, 
			(uintmax_t)(size / blksize));
	}
}

static void
print_user_and_group(uid_t uid, gid_t gid, int usr_max, int grp_max)
{
	struct passwd *usr;
	struct group  *grp;

	if (f_numid) {
		printf("%*"PRIuMAX" %*"PRIuMAX" ", usr_max, 
			(uintmax_t)uid, grp_max, (uintmax_t)gid);
	} else {
		if ((usr = getpwuid(uid)) != NULL)
			printf("%*s ", usr_max, usr->pw_name);
		else {
			if (errno != 0)
				return_val = EXIT_FAILURE;

			printf("%*"PRIuMAX" ", usr_max, (uintmax_t)uid);
		}
		if ((grp = getgrgid(gid)) != NULL)
			printf("%*s ", grp_max, grp->gr_name);
		else {
			if (errno != 0)
				return_val = EXIT_FAILURE;

			printf("%*"PRIuMAX" ", usr_max, (uintmax_t)gid);
		}
	}
}

static void
print_size(off_t size, int size_max)
{
	char size_buff[SIZE_BUFF_SIZE];

	if (f_readable) {
		if (humanize_number(size_buff, SIZE_BUFF_SIZE, 
		    (int64_t)size, "", 
		    HN_AUTOSCALE, HN_DECIMAL | HN_NOSPACE) == -1) {
			/* transformation failed, uses '?' to represent */
			size_buff[0] = '?';
			size_buff[1] = '\0';

			return_val = EXIT_FAILURE;
		}
			printf("%*s ", size_max, size_buff);
	} else if (f_kb) {
		printf("%*"PRIuMAX" ", size_max, 
			(uintmax_t)size / 1024);
	} else {
		printf("%*"PRIuMAX" ", size_max, 
			(uintmax_t)size);
	}
	
}

static void
print_device(dev_t devid, int major_max, int minor_max)
{
	int major_num, minor_num;

	major_num = major(devid);
	minor_num = minor(devid);

	printf("%*d, %*d ", major_max, major_num, minor_max, minor_num);
}

static void
print_date(struct stat *st)
{
	char *complete_time;
	char show_time[13];

	if (f_atime)
		complete_time = ctime(&st->st_atime);
	else if(f_ctime)
		complete_time = ctime(&st->st_ctime);
	else
		complete_time = ctime(&st->st_mtime);

	/* transformation failed, uses '??????????' to represent */
	if (complete_time == NULL) {
	             /* MON DD HH:MM */
		printf("???????????? ");

		return_val = EXIT_FAILURE;
		return ;
	}

	if (strncpy(show_time, complete_time + 4, 12) == NULL) {
	             /* MON DD HH:MM */
		printf("???????????? ");

		return_val = EXIT_FAILURE;
		return ;
	}

	show_time[12]='\0';
	printf("%s ", show_time);
	
}

/* 
 * Return the length of the name, the return value is used for short
 * format(both column and column across)
 */
static int
print_name(FTSENT *p, int prt_link)
{
	int    i;
	int    count;
	char   *name;
	mode_t mode;

	name = p->fts_name;

	if (name == NULL) {
		putchar('?');
		return_val = EXIT_FAILURE;
		return 1;
	}

	count = strlen(name);

	mode = p->fts_statp->st_mode;

	if (f_nonprint) {
		for (i = 0; name[i] != '\0'; i++) {
			if (isprint(name[i]))
				putchar(name[i]);
			else
				putchar('?');
		}
	}
	else
		printf("%s", name);

	if (f_char) {
		switch (mode & S_IFMT) {
		case S_IFDIR:
			putchar('/');
			count++;
			break;
		case S_IFLNK:
			putchar('@');
			count++;
			break;
#ifdef S_IFWHT
		case S_IFWHT:
			putchar('%');
			count++;
			break;
#endif
		case S_IFSOCK:
			putchar('=');
			count++;
			break;
		case S_IFIFO:
			putchar('|');
			count++;
			break;
		default:
			if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
				putchar('*');
				count++;
			}
			break;
		}
	}

	/* print linked-to file */
	if (prt_link == WITH_LINK && S_ISLNK(mode)) 
		print_link(p);

	return count;
}

static void
print_link(FTSENT *p)
{
	int  len;
	char src_path[PATH_MAX], dst_path[PATH_MAX];

	if (p->fts_level == ROOT) {
		if (snprintf(src_path, PATH_MAX - 1, "%s", p->fts_name) 
		    == -1) {
			warnx("%s: failed to copy name", p->fts_name);
			return_val = EXIT_FAILURE;

			return;
		}
	} else {
		if (snprintf(src_path, PATH_MAX - 1, "%s/%s", p->fts_path, 
			 p->fts_name) == -1) {
			warnx("%s: failed to copy name", p->fts_name);
			return_val = EXIT_FAILURE;

			return;
		}
	}

	if ((len = readlink(src_path, dst_path, PATH_MAX - 1)) == -1) {
		warn("%s: can not link", src_path);

		return_val = EXIT_FAILURE;
		return;
	}

	dst_path[len] = '\0';
	printf(" -> ");

	printf("%s", dst_path);
}

static void
print_line(OBJ *obj)
{
	struct stat *st;
	FTSENT      *p;

	p = obj->obj_list;

	while (p != NULL) {
		if (p->fts_number == FTS_NO_PRINT) {
			p = p->fts_link;
			continue;
		}

		st = p->fts_statp;

		/* print inode */
		if (f_ino)
			printf("%*"PRIuMAX" ", obj->obj_ino_max,
				(uintmax_t)st->st_ino);

		/* print number of blocks */
		if (f_block) {	
			print_block(st->st_blocks * 512, obj->obj_block_max);
		}

		print_name(p, NO_LINK);

		putchar('\n');
		p = p->fts_link;
	}
}

static void
print_column(OBJ *obj)
{
	struct stat *st;
	FTSENT      *p;
	FTSENT      **arr;	/* array of the files */

	int count_file;		/* count total number of file */
	int count_pre;		/* count the width of ino and block */
	int col_num;		/* how many columns to print */
	int *col_width_max;	/* max width for each column */
	int row_num;		/* number of rows */
	int last_row;		/* how rows in the last column */
	int i, j, t;

	count_pre = 0;

	if (f_ino)
		count_pre += obj->obj_ino_max + 1;
	if (f_block)
		count_pre += obj->obj_block_max + 1;
	if (f_char)
		count_pre++;

	count_file = 0;
	for (p = obj->obj_list; p != NULL; p = p->fts_link)
		if (p->fts_number != FTS_NO_PRINT)
			count_file++;

	/* first guess at column number, suppose all the file names are 1 */
	col_num = term_width / (count_pre + 1 + 2);

	/* at least one column */
	col_num = MAX(col_num, 1);

	if (count_file == 0) {
		return;
	}

	/* one column, use line print */
	if (count_file == 1 || col_num == 1) {
		print_line(obj);
		return;
	}

	col_width_max = (int *)malloc(col_num * sizeof(int));
	arr = (FTSENT **)malloc(count_file * sizeof(FTSENT *));

	if (col_width_max == NULL || arr == NULL) {
		free(col_width_max);
		free(arr);
		warn("failed to malloc");
		return;
	}

	/* copy files to array */
	for (p = obj->obj_list, i = 0; p!= NULL; p = p->fts_link) {
		if (p->fts_number == FTS_NO_PRINT)
			continue;
		arr[i++] = p;
	}

	/* how many rows needed to be printed */
	if (count_file % col_num == 0)
		row_num = count_file / col_num;
	else
		row_num = count_file / col_num + 1;

	row_num = MAX(row_num, 1);

	/*
	 * Try to find the best printing format, and the max width for each
	 * column.
	 */
	while (row_num < count_file) {
		/* max length of the column */
		int max_len;

		if (count_file % row_num == 0) {
			col_num = count_file / row_num;
			last_row = row_num;
		} else {
			col_num = count_file / row_num + 1;
			last_row = count_file % row_num;
		}

		for (i = 0; i < col_num; i++)
			col_width_max[i] = 0;

		for (i = 0, t = 0; i < col_num; i++) {
			int f;
			int tmp_count;
			f = (i == col_num - 1) ? last_row : row_num;
			for (j = 0; j < f; j++) {
				tmp_count = count_pre + 
					    strlen(arr[t++]->fts_name) + 2;

				if (f_char) 
					tmp_count++;

				col_width_max[i] = MAX(col_width_max[i], 
							tmp_count); 
			}
		}

		max_len = 0;

		for (i = 0; i < col_num; i++)
			max_len += col_width_max[i];

		/*
		 * If the total length of each row is less than the width
		 * of the terminal, then we stop.
		 */
		if (max_len <= term_width)
			break;

		row_num++;
	}

	/* one column, use line print */
	if (row_num == count_file) {
		free(arr);
		free(col_width_max);
		print_line(obj);
		return;
	}

	for (i = 0; i < row_num; i++) {
		int f;
		f = (i < last_row) ? col_num : (col_num - 1);
		for (j = 0; j < f; j++) {
			st = arr[i + j * row_num]->fts_statp;

			/* print inode */
			if (f_ino)
				printf("%*"PRIuMAX" ", obj->obj_ino_max,
					(uintmax_t)st->st_ino);
	
			/* print number of blocks */
			if (f_block) {	
				print_block(st->st_blocks * DEFAULT_BSIZE, 
					    obj->obj_block_max);
			}
	
			
			t = count_pre + print_name(arr[i + j * row_num], 0);

			while (t < col_width_max[j]) {
				putchar(' ');
				t++;
			}
		}
		putchar('\n');
	}

	free(arr);
	free(col_width_max);
}

static void
print_colacross(OBJ *obj)
{
	struct stat *st;
	FTSENT      *p;

	int count_file;		/* count total number of file */
	int count_pre;		/* count the width of ino and block */
	int col_num;		/* how many columns to print */
	int *col_width_max;	/* max width for each column */
	int row_num;		/* how many columns in the last row */
	int i, j, m, t;
	count_pre = 0;

	if (f_ino)
		count_pre += obj->obj_ino_max + 1;
	if (f_block)
		count_pre += obj->obj_block_max + 1;
	if (f_char)
		count_pre++;

	count_file = 0;

	for (p = obj->obj_list; p != NULL; p = p->fts_link)
		if (p->fts_number != FTS_NO_PRINT) 
			count_file++;

	/* first guess at column number, suppose all the file names are 1 */
	col_num = term_width / (count_pre + 1 + 2);

	/* at least one column */
	col_num = MAX(col_num, 1);

	if (count_file == 0) {
		putchar('\n');
		return;
	}

	if (count_file == 1 || col_num == 1) {
		print_line(obj);
		return;
	}

	col_width_max = (int *)malloc(col_num * sizeof(int));

	if (col_width_max == NULL) {
		free(col_width_max);
		warn("failed to malloc");
		return;
	}

	/*
	 * Try to find the best printing format, and the max width for each
	 * column.
	 */
	while (col_num > 1) {
		/* max length of the column */
		int max_len;

		if (count_file % col_num == 0)
			row_num = count_file / col_num;
		else
			row_num = count_file / col_num + 1;

		for (i = 0; i < col_num; i++)
			col_width_max[i] = 0;

		for (i = 0, t = 0, p = obj->obj_list; i < row_num; i++) {
			int tmp_count;
			for (j = 0; j < col_num && t < count_file; 
			     j++, t++, p = p->fts_link) {
				while (p->fts_number == FTS_NO_PRINT)
					p = p->fts_link;

				tmp_count = count_pre + 
    					strlen(p->fts_name) + 2;

				if (f_char) 
					tmp_count++;

				col_width_max[j] = MAX(col_width_max[j], 
							tmp_count); 
			}
		}

		max_len = 0;

		for (i = 0; i < col_num; i++)
			max_len += col_width_max[i];

		/*
		 * If the total length of each row is less than the width
		 * of the terminal, then we stop.
		 */
		if (max_len <= term_width)
			break;
		col_num--;
	}

	if (col_num == 1) {
		free(col_width_max);
		print_line(obj);
		return;
	}

	for (i = 0, t = 0, p = obj->obj_list; i < row_num; i++) {
		for (j = 0; j < col_num && t < count_file; 
		     j++, t++, p = p->fts_link) {
			while (p->fts_number == FTS_NO_PRINT)
				p = p->fts_link;

			st = p->fts_statp;

			/* print inode */
			if (f_ino)
				printf("%*"PRIuMAX" ", obj->obj_ino_max,
					(uintmax_t)st->st_ino);
	
			/* print number of blocks */
			if (f_block) {	
				print_block(st->st_blocks * DEFAULT_BSIZE, 
					    obj->obj_block_max);
			}
	
			
			m = count_pre + print_name(p, 0);

			while (m < col_width_max[j]) {
				putchar(' ');
				m++;
			}
		}
		putchar('\n');
	}

	free(col_width_max);
}
