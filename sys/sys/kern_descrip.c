/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kern_descrip.c	1.6 (2.11BSD) 1999/9/13
 */

#include "param.h"
#include "user.h"
#include "proc.h"
#include "file.h"
#include "systm.h"
#include "inode.h"
#include "ioctl.h"
#include "stat.h"
#include "conf.h"
#ifdef INET
#include "socket.h"
#include "socketvar.h"
#endif
#include <syslog.h>

/*
 * Descriptor management.
 */

/*
 * Systemstruct file *wfp; calls on descriptors.
 */
getdtablesize()
{

	u.u_r.r_val1 = NOFILE;
}

dup()
{
	register struct a {
		int	i;
	} *uap = (struct a *) u.u_ap;
	register struct file *fp;
	register int j;

	if (uap->i &~ 077) { uap->i &= 077; dup2(); return; }	/* XXX */

	GETF(fp, uap->i);
	j = ufalloc(0);
	if (j < 0)
		return;
	dupit(j, fp, u.u_pofile[uap->i] &~ UF_EXCLOSE);
}

dup2()
{
	register struct a {
		int	i, j;
	} *uap = (struct a *) u.u_ap;
	register struct file *fp;

	GETF(fp, uap->i);
	if (uap->j < 0 || uap->j >= NOFILE) {
		u.u_error = EBADF;
		return;
	}
	u.u_r.r_val1 = uap->j;
	if (uap->i == uap->j)
		return;
	if (u.u_ofile[uap->j])
		/*
		 * dup2 must succeed even if the close has an error.
		 */
		(void) closef(u.u_ofile[uap->j]);
	dupit(uap->j, fp, u.u_pofile[uap->i] &~ UF_EXCLOSE);
}

dupit(fd, fp, flags)
	register int fd;
	register struct file *fp;
	int flags;
{

	u.u_ofile[fd] = fp;
	u.u_pofile[fd] = flags;
	fp->f_count++;
	if (fd > u.u_lastfile)
		u.u_lastfile = fd;
}

/*
 * The file control system call.
 */
fcntl()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		int	cmd;
		int	arg;
	} *uap;
	register i;
	register char *pop;

	uap = (struct a *)u.u_ap;
	if ((fp = getf(uap->fdes)) == NULL)
		return;
	pop = &u.u_pofile[uap->fdes];
	switch(uap->cmd) {
	case F_DUPFD:
		i = uap->arg;
		if (i < 0 || i >= NOFILE) {
			u.u_error = EINVAL;
			return;
		}
		if ((i = ufalloc(i)) < 0)
			return;
		dupit(i, fp, *pop &~ UF_EXCLOSE);
		break;

	case F_GETFD:
		u.u_r.r_val1 = *pop & 1;
		break;

	case F_SETFD:
		*pop = (*pop &~ 1) | (uap->arg & 1);
		break;

	case F_GETFL:
		u.u_r.r_val1 = OFLAGS(fp->f_flag);
		break;

	case F_SETFL:
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= (FFLAGS(uap->arg)) & FCNTLFLAGS;
		u.u_error = fset(fp, FNONBLOCK, fp->f_flag & FNONBLOCK);
		if (u.u_error)
			break;
		u.u_error = fset(fp, FASYNC, fp->f_flag & FASYNC);
		if (u.u_error)
			(void) fset(fp, FNONBLOCK, 0);
		break;

	case F_GETOWN:
		u.u_error = fgetown(fp, &u.u_r.r_val1);
		break;

	case F_SETOWN:
		u.u_error = fsetown(fp, uap->arg);
		break;

	default:
		u.u_error = EINVAL;
	}
}

fset(fp, bit, value)
register struct file *fp;
	int bit, value;
{

	if (value)
		fp->f_flag |= bit;
	else
		fp->f_flag &= ~bit;
	return (fioctl(fp, (u_int)(bit == FNONBLOCK ? FIONBIO : FIOASYNC),
			(caddr_t)&value));
}

fgetown(fp, valuep)
	register struct file *fp;
	register int *valuep;
{
	register int error;

#ifdef INET
	if (fp->f_type == DTYPE_SOCKET) {
		*valuep = mfsd(&fp->f_socket->so_pgrp);
		return (0);
	}
#endif
	error = fioctl(fp, (u_int)TIOCGPGRP, (caddr_t)valuep);
	*valuep = -*valuep;
	return (error);
}

fsetown(fp, value)
	register struct file *fp;
	int value;
{

#ifdef INET
	if (fp->f_type == DTYPE_SOCKET) {
		mtsd(&fp->f_socket->so_pgrp, value);
		return (0);
	}
#endif
	if (value > 0) {
		register struct proc *p = pfind(value);
		if (p == 0)
			return (ESRCH);
		value = p->p_pgrp;
	} else
		value = -value;
	return (fioctl(fp, (u_int)TIOCSPGRP, (caddr_t)&value));
}

extern	struct	fileops	*Fops[];

fioctl(fp, cmd, value)
register struct file *fp;
	u_int cmd;
	caddr_t value;
{

	return ((*Fops[fp->f_type]->fo_ioctl)(fp, cmd, value));
}

close()
{
	register struct a {
		int	i;
	} *uap = (struct a *)u.u_ap;
	register struct file *fp;

	GETF(fp, uap->i);
	u.u_ofile[uap->i] = NULL;
	while (u.u_lastfile >= 0 && u.u_ofile[u.u_lastfile] == NULL)
		u.u_lastfile--;
	u.u_error = closef(fp);
	/* WHAT IF u.u_error ? */
}

fstat()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		struct	stat *sb;
	} *uap;
	struct stat ub;

	uap = (struct a *)u.u_ap;
	if ((fp = getf(uap->fdes)) == NULL)
		return;
	switch (fp->f_type) {

	case DTYPE_PIPE:
	case DTYPE_INODE:
		u.u_error = ino_stat((struct inode *)fp->f_data, &ub);
		if (fp->f_type == DTYPE_PIPE)
			ub.st_size -= fp->f_offset;
		break;

#ifdef INET
	case DTYPE_SOCKET:
		u.u_error = SOO_STAT(fp->f_socket, &ub);
		break;
#endif
	default:
		u.u_error = EINVAL;
		break;
	}
	if (u.u_error == 0)
		u.u_error = copyout((caddr_t)&ub, (caddr_t)uap->sb,
		    sizeof (ub));
}

/* copied, for supervisory networking, to sys_net.c */
/*
 * Allocate a user file descriptor.
 */
ufalloc(i)
	register int i;
{

	for (; i < NOFILE; i++)
		if (u.u_ofile[i] == NULL) {
			u.u_r.r_val1 = i;
			u.u_pofile[i] = 0;
			if (i > u.u_lastfile)
				u.u_lastfile = i;
			return (i);
		}
	u.u_error = EMFILE;
	return (-1);
}

struct	file *lastf;
/*
 * Allocate a user file descriptor
 * and a file structure.
 * Initialize the descriptor
 * to point at the file structure.
 */
struct file *
falloc()
{
	register struct file *fp;
	register i;

	i = ufalloc(0);
	if (i < 0)
		return (NULL);
	if (lastf == 0)
		lastf = file;
	for (fp = lastf; fp < fileNFILE; fp++)
		if (fp->f_count == 0)
			goto slot;
	for (fp = file; fp < lastf; fp++)
		if (fp->f_count == 0)
			goto slot;
	tablefull("file");
	u.u_error = ENFILE;
	return (NULL);
slot:
	u.u_ofile[i] = fp;
	fp->f_count = 1;
	fp->f_data = 0;
	fp->f_offset = 0;
	lastf = fp + 1;
	return (fp);
}

/*
 * Convert a user supplied file descriptor into a pointer
 * to a file structure.  Only task is to check range of the descriptor.
 * Critical paths should use the GETF macro unless code size is a 
 * consideration.
 */
struct file *
getf(f)
	register int f;
{
	register struct file *fp;

	if ((unsigned)f >= NOFILE || (fp = u.u_ofile[f]) == NULL) {
		u.u_error = EBADF;
		return (NULL);
	}
	return (fp);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 */
closef(fp)
	register struct file *fp;
{
	int	error;

	if (fp == NULL)
		return(0);
	if (fp->f_count > 1) {
		fp->f_count--;
		return(0);
	}

	if	((fp->f_flag & (FSHLOCK|FEXLOCK)) && fp->f_type == DTYPE_INODE)
		ino_unlock(fp, FSHLOCK|FEXLOCK);

	error = (*Fops[fp->f_type]->fo_close)(fp);
	fp->f_count = 0;
	return(error);
}

/*
 * Apply an advisory lock on a file descriptor.
 */
flock()
{
	register struct a {
		int	fd;
		int	how;
	} *uap = (struct a *)u.u_ap;
	register struct file *fp;
	int error;

	if ((fp = getf(uap->fd)) == NULL)
		return;
	if (fp->f_type != DTYPE_INODE) {
		u.u_error = EOPNOTSUPP;
		return;
	}
	if (uap->how & LOCK_UN) {
		ino_unlock(fp, FSHLOCK | FEXLOCK);
		return;
	}
	if ((uap->how & (LOCK_SH | LOCK_EX)) == 0)
		return;					/* error? */
	if (uap->how & LOCK_EX)
		uap->how &= ~LOCK_SH;
	/* avoid work... */
	if ((fp->f_flag & FEXLOCK) && (uap->how & LOCK_EX) ||
	    (fp->f_flag & FSHLOCK) && (uap->how & LOCK_SH))
		return;
	error = ino_lock(fp, uap->how);
	return(u.u_error = error);
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 */
/* ARGSUSED */
fdopen(dev, mode, type)
	dev_t dev;
	int mode, type;
	{

	/*
	 * XXX Kludge: set u.u_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error 
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open will
	 * simply report the error.
	 */
	u.u_dupfd = minor(dev);
	return(ENODEV);
	}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
dupfdopen(indx, dfd, mode, error)
	register int indx, dfd;
	int mode;
	int error;
	{
	#ifdef __riscv
		register struct file *wfp;
	#else
		register register struct file *wfp;
	#endif
	struct file *fp;
	
	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject.  Note, check for new == old is necessary as
	 * falloc could allocate an already closed to-be-dup'd descriptor
	 * as the new descriptor.
	 */
	fp = u.u_ofile[indx];
	if	(dfd >= NOFILE || (wfp = u.u_ofile[dfd]) == NULL || fp == wfp)
		return(EBADF);

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor
	 * (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and
	 * store it in (indx).  (dfd) is effectively closed by
	 * this operation.
	 *
	 * NOTE: ENXIO only comes out of the 'portal fs' code of 4.4 - since
	 * 2.11BSD does not implement the portal fs the code is ifdef'd out
	 * and a short message output.
	 *
	 * Any other error code is just returned.
	 */
	switch	(error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag)
			return(EACCES);
		u.u_ofile[indx] = wfp;
		u.u_pofile[indx] = u.u_pofile[dfd];
		wfp->f_count++;
		if	(indx > u.u_lastfile)
			u.u_lastfile = indx;
		return(0);
#ifdef	haveportalfs
	case ENXIO:
		/*
		 * Steal away the file pointer from dfd, and stuff it into indx.
		 */
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofileflags[dfd] = 0;
		/*
		 * Complete the clean up of the filedesc structure by
		 * recomputing the various hints.
		 */
		if (indx > fdp->fd_lastfile)
			fdp->fd_lastfile = indx;
		else
			while (fdp->fd_lastfile > 0 &&
			       fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
				fdp->fd_lastfile--;
			if (dfd < fdp->fd_freefile)
				fdp->fd_freefile = dfd;
		return (0);
#else
		log(LOG_NOTICE, "dupfdopen");
		/* FALLTHROUGH */
#endif
	default:
		return(error);
	}
	/* NOTREACHED */
}
