/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)namei.h	1.3 (2.11BSD) 1997/1/18
 */

#ifndef _NAMEI_
#define	_NAMEI_

#ifdef KERNEL
#include "uio.h"
#else
#include <sys/uio.h>
#endif

/*
 * Encapsulation of namei parameters.
 * One of these is located in the u. area to
 * minimize space allocated on the kernel stack.
 */
struct nameidata {
	caddr_t	ni_dirp;		/* pathname pointer */
	short	ni_nameiop;		/* see below */
	short	ni_error;		/* error return if any */
	off_t	ni_endoff;		/* end of useful stuff in directory */
	struct	inode *ni_pdir;		/* inode of parent directory of dirp */
	struct	inode *ni_ip;		/* inode of dirp */
	enum	uio_seg	ni_segflg;	/* segment flag */
	off_t	ni_offset;		/* offset in directory */
	u_short	ni_count;		/* offset of open slot (off_t?) */
	struct	direct ni_dent;		/* current directory entry */
};

/*
 * namei operations and modifiers
 */
#define	LOOKUP		0	/* perform name lookup only */
#define	CREATE		1	/* setup for file creation */
#define	DELETE		2	/* setup for file deletion */
#define	LOCKPARENT	0x10	/* see the top of namei */
#define NOCACHE		0x20	/* name must not be left in cache */
#define FOLLOW		0x40	/* follow symbolic links */
#define	NOFOLLOW	0x0	/* don't follow symbolic links (pseudo) */

#define	NDINIT(ndp,op,flags,segflg,namep) {\
	(ndp)->ni_nameiop = op | flags; \
	(ndp)->ni_segflg = segflg; \
	(ndp)->ni_dirp = namep; \
	}

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei.
 */
struct	namecache {
	struct	namecache *nc_forw;	/* hash chain, MUST BE FIRST */
	struct	namecache *nc_back;	/* hash chain, MUST BE FIRST */
	struct	namecache *nc_nxt;	/* LRU chain */
	struct	namecache **nc_prev;	/* LRU chain */
	struct	inode *nc_ip;		/* inode the name refers to */
	ino_t	nc_ino;			/* ino of parent of name */
	dev_t	nc_dev;			/* dev of parent of name */
	dev_t	nc_idev;		/* dev of the name ref'd */
	u_short	nc_id;			/* referenced inode's id */
	char	nc_nlen;		/* length of name */
#define	NCHNAMLEN	15	/* maximum name segment length we bother with */
	char	nc_name[NCHNAMLEN];	/* segment name */
};

#ifdef __riscv
	#if	defined(KERNEL) && !defined(SUPERVISOR)
	extern struct	namecache *namecache;
	extern int	nchsize;
	#include <machine/seg.h>
	extern segm	nmidesc;
	#endif

#else
	#if	defined(KERNEL) && !defined(SUPERVISOR)
	struct	namecache *namecache;
	int	nchsize;
	#include <machine/seg.h>
	segm	nmidesc;
	#endif
#endif

/*
 * Stats on usefulness of namei caches.
 */
struct	nchstats {
	long	ncs_goodhits;		/* hits that we can reall use */
	long	ncs_badhits;		/* hits we must drop */
	long	ncs_falsehits;		/* hits with id mismatch */
	long	ncs_miss;		/* misses */
	long	ncs_long;		/* long names that ignore cache */
	long	ncs_pass2;		/* names found with passes == 2 */
	long	ncs_2passes;		/* number of times we attempt it */
};
#endif
