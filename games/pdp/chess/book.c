#include "old.h"

bookm()
{
	int i, buf[2];

	if(!bookp) return(0);
	lseek(bookf, (long)(unsigned)bookp, 0);

	i = 0;
loop:
	read(bookf, buf, 4);
	*buf = booki(*buf);
	if(*buf >= 0) {
		if(!i)
			i = *buf;
		goto loop;
	}
	if(abmove = i)
		return(1);
	return(0);
}

makmov(m)
{
	int buf[2];

	out1(m);
	mantom? bmove(m): wmove(m);
	increm();
	if(!bookp) return;
	lseek(bookf, (long)(unsigned)bookp, 0);

loop:
	read(bookf, buf, 4);
	*buf = booki(*buf);
	if(m == *buf || *buf == 0) {
		bookp = buf[1] & ~1;
		goto l1;
	}
	if(*buf < 0) {
		bookp = 0;
		goto l1;
	}
	goto loop;

l1:
	if(!bookp) {
		putchar('\n');
		return;
	}
}

booki(m)
{
	union {
		int l;
		char m[2];
	} i, j;

	j.l = m;

	i.m[0] = j.m[1];
	i.m[1] = j.m[0];

	return(i.l);
}

