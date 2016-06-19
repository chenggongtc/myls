ls: ls.c ls.h print.c print.h cmp.c cmp.h
	cc -Wall -o ls ls.c print.c cmp.c -lbsd
