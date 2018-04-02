/*
 *                     RCS stream editor
 */
#ifndef lint
static char rcsid[]= "$Id: rcsedit.c,v 3.9 88/02/18 11:56:51 bostic Exp $ Purdue CS";
#endif
/**********************************************************************************
 *                       edits the input file according to a
 *                       script from stdin, generated by diff -n
 *                       performs keyword expansion
 **********************************************************************************
 *
 * Copyright (C) 1982 by Walter F. Tichy
 *                       Purdue University
 *                       Computer Science Department
 *                       West Lafayette, IN 47907
 *
 * All rights reserved. No part of this software may be sold or distributed
 * in any form or by any means without the prior written permission of the
 * author.
 * Report problems and direct all inquiries to Tichy@purdue (ARPA net).
 */


/* $Log:	rcsedit.c,v $
 * Revision 3.9  88/02/18  11:56:51  bostic
 * replaced with version 4
 * 
 * Revision 4.5  87/12/18  11:38:46  narten
 * Changes from the 43. version. Don't know the significance of the
 * first change involving "rewind". Also, additional "lint" cleanup.
 * (Guy Harris)
 * 
 * Revision 4.4  87/10/18  10:32:21  narten
 * Updating version numbers. Changes relative to version 1.1 actually
 * relative to 4.1
 * 
 * Revision 1.4  87/09/24  13:59:29  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.3  87/09/15  16:39:39  shepler
 * added an initializatin of the variables editline and linecorr
 * this will be done each time a file is processed.
 * (there was an obscure bug where if co was used to retrieve multiple files
 *  it would dump)
 * fix attributed to  Roy Morris @FileNet Corp ...!felix!roy
 * 
 * Revision 1.2  87/03/27  14:22:17  jenkins
 * Port to suns
 * 
 * Revision 1.1  84/01/23  14:50:20  kcs
 * Initial revision
 * 
 * Revision 4.1  83/05/12  13:10:30  wft
 * Added new markers Id and RCSfile; added locker to Header and Id.
 * Overhauled expandline completely() (problem with $01234567890123456789@).
 * Moved trymatch() and marker table to rcskeys.c.
 * 
 * Revision 3.7  83/05/12  13:04:39  wft
 * Added retry to expandline to resume after failed match which ended in $.
 * Fixed truncation problem for $19chars followed by@@.
 * Log no longer expands full path of RCS file.
 * 
 * Revision 3.6  83/05/11  16:06:30  wft
 * added retry to expandline to resume after failed match which ended in $.
 * Fixed truncation problem for $19chars followed by@@.
 * 
 * Revision 3.5  82/12/04  13:20:56  wft
 * Added expansion of keyword Locker.
 *
 * Revision 3.4  82/12/03  12:26:54  wft
 * Added line number correction in case editing does not start at the
 * beginning of the file.
 * Changed keyword expansion to always print a space before closing KDELIM;
 * Expansion for Header shortened.
 *
 * Revision 3.3  82/11/14  14:49:30  wft
 * removed Suffix from keyword expansion. Replaced fclose with ffclose.
 * keyreplace() gets log message from delta, not from curlogmsg.
 * fixed expression overflow in while(c=putc(GETC....
 * checked nil printing.
 *
 * Revision 3.2  82/10/18  21:13:39  wft
 * I added checks for write errors during the co process, and renamed
 * expandstring() to xpandstring().
 *
 * Revision 3.1  82/10/13  15:52:55  wft
 * changed type of result of getc() from char to int.
 * made keyword expansion loop in expandline() portable to machines
 * without sign-extension.
 */


#include "rcsbase.h"


extern FILE * fopen();
extern char * mktempfile();
extern char * bindex();
extern FILE * finptr, * frewrite;
extern int rewriteflag;
extern int nextc;
extern char * RCSfilename, * workfilename;
extern char * bindex();
extern char * getfullRCSname();
extern enum markers trymatch();


FILE  * fcopy,  * fedit; /* result and edit file descriptors                */
char  *resultfile = nil; /* result file name                                */
char  * editfile  = nil; /* edit   file name                                */
int editline;  /*line counter in fedit; starts with 1, is always #lines+1   */
int linecorr;  /*contains #adds - #deletes in each edit run.                */
               /*used to correct editline in case file is not rewound after */
               /* applying one delta                                        */

initeditfiles(dir)
char * dir;
/* Function: Initializes resultfile and editfile with temporary filenames
 * in directory dir. Opens resultfile for reading and writing, with fcopy
 * as file descriptor. fedit is set to nil.
 */
{
	editline = linecorr = 0;    /* make sure we start from the beginning*/
        resultfile=mktempfile(dir,TMPFILE1);
        editfile  =mktempfile(dir,TMPFILE2);
        fedit=nil;
        if ((fcopy=fopen(resultfile,"w+"))==NULL) {
                faterror("Can't open working file %s",resultfile);
        }
}


swapeditfiles(tostdout)
/* Function: swaps resultfile and editfile, assigns fedit=fcopy,
 * rewinds fedit for reading, and opens resultfile for reading and
 * writing, using fcopy. If tostdout, fcopy is set to stdout.
 */
{       char * tmpptr;
        if(ferror(fcopy))
                faterror("write failed on %s -- file system full?",resultfile);
        fedit=fcopy;
        rewind(fedit);
        editline = 1; linecorr=0;
        tmpptr=editfile; editfile=resultfile; resultfile=tmpptr;
        if (tostdout)
                fcopy=stdout;
        elsif ((fcopy=fopen(resultfile,"w+"))==NULL) {
                faterror("Can't open working file %s",resultfile);
        }
}


finishedit(delta)
struct hshentry * delta;
/* copy the rest of the edit file and close it (if it exists).
 * if delta!=nil, perform keyword substitution at the same time.
 */
{
        register int c;
        if (fedit!=nil) {
                if (delta!=nil) {
                        while (expandline(fedit,fcopy,delta,false,false)) editline++;
                } else {
                        while((c=getc(fedit))!=EOF) {
                                VOID putc(c,fcopy);
                                if (c=='\n') editline++;
                        }
                }
                ffclose(fedit);
        }
}


copylines(line,delta)
register int line; struct hshentry * delta;
/* Function: copies input lines editline..line-1 from fedit to fcopy.
 * If delta != nil, keyword expansion is done simultaneously.
 * editline is updated. Rewinds a file only if necessary.
 */
{

        if (editline>line) {
                /* swap files */
                finishedit((struct hshentry *)nil); swapeditfiles(false);
                /* assumes edit only during last pass, from the beginning*/
        }
        while (editline<line) {
                /*copy another line*/
                if (delta)
                        VOID expandline(fedit,fcopy,delta,false,false);
                else
                        while (putc(getc(fedit),fcopy)!='\n');
                editline++;
        }
}



xpandstring(delta)
struct hshentry * delta;
/* Function: Reads a string terminated by SDELIM from finptr and writes it
 * to fcopy. Double SDELIM is replaced with single SDELIM.
 * Keyword expansion is performed with data from delta.
 * If rewriteflag==true, the string is also copied unchanged to frewrite.
 * editline is updated.
 */
{
        editline=1;
        while (expandline(finptr,fcopy,delta,true,rewriteflag)) editline++;
        nextc='\n';
}


copystring()
/* Function: copies a string terminated with a single SDELIM from finptr to
 * fcopy, replacing all double SDELIM with a single SDELIM.
 * If rewriteflag==true, the string also copied unchanged to frewrite.
 * editline is set to (number of lines copied)+1.
 * Assumption: next character read is first string character.
 */
{       register c, write;
        write=rewriteflag;
        editline=1;
        while ((c=GETC(finptr,frewrite,write)) != EOF) {
                if ((c==SDELIM)&&((c=GETC(finptr,frewrite,write)) != SDELIM)){
                        /* end of string */
                        nextc = c;
                        return;
                }
                VOID putc(c,fcopy);
                if (c=='\n') editline++;
        }
        nextc = c;
        serror("Unterminated string");
        return;
}




editstring(delta)
struct hshentry * delta;
/* Function: reads an edit script from finptr and applies it to
 * file fedit; the result is written to fcopy.
 * If delta!=nil, keyword expansion is performed simultaneously.
 * If frewrite==true, the edit script is also copied verbatim to frewrite.
 * Assumes that all these files are open.
 * If running out of lines in fedit, fedit and fcopy are swapped.
 * resultfile and editfile are the names of the files that go with fcopy
 * and fedit, respectively.
 * Assumes the next input character from finptr is the first character of
 * the edit script. Resets nextc on exit.
 */
{
        int ed; /* editor command */
        register int c;
        register int write, i;
        int line, length;

        editline += linecorr; linecorr=0; /*correct line number*/
        write=rewriteflag;
        for (;;) {
                /* read next command and decode */
                /* assume next non-white character is command name*/
                while((ed=GETC(finptr,frewrite,write))=='\n'||
                        ed==' ' || ed=='\t');
                if (ed==SDELIM) break;
                /* now attempt to read numbers. */
                /* fscanf causes trouble because of the required echoing */
                while ((c=GETC(finptr,frewrite,write))==' ');  /*skip spaces*/
                if (!('0'<=c && c<='9')) {
                        faterror("missing line number in edit script");
                        break;
                }
                line= c -'0';
                while ('0'<=(c=GETC(finptr,frewrite,write)) && c<='9') {
                        line = line*10 + c-'0';
                }
                while (c==' ') c=GETC(finptr,frewrite,write);
                if (!('0'<=c && c<='9')) {
                        faterror("incorrect range in edit script");
                        break;
                }
                length= c -'0';
                while ('0'<=(c=GETC(finptr,frewrite,write)) && c<='9') {
                        length = length*10 + c-'0';
                }
                while(c!='\n'&&c!=EOF) c=GETC(finptr,frewrite,write); /* skip to end of line */

                switch (ed) {
                case 'd':
                        copylines(line,delta);
                        /* skip over unwanted lines */
                        for (i=length;i>0;i--) {
                                /*skip next line*/
                                while ((c=getc(fedit))!='\n');
					if (c==EOF)
						faterror("EOF during edit");
                                editline++;
                        }
                        linecorr -= length;
                        break;
                case 'a':
                        copylines(line+1,delta); /*copy only; no delete*/
                        for (i=length;i>0;i--) {
                                /*copy next line from script*/
                                if (delta!=nil)
                                       VOID expandline(finptr,fcopy,delta,true,write);
                                else {
                                       c = GETC(finptr,frewrite,write);
                                       while (putc(c,fcopy)!='\n'){
                                               if ((c==SDELIM)&&((c=GETC(finptr,frewrite,write))!=SDELIM)){
                                                       serror("Missing string delimiter in edit script");
                                                       VOID putc(c,fcopy);
                                               }
                                               c = GETC(finptr,frewrite,write);
                                       }
                                }
                        }
                        linecorr += length;
                        break;
                default:
                        faterror("unknown command in edit script: %c", ed);
                        break;
                }
        }
        nextc=GETC(finptr,frewrite,write);
}



/* The rest if for keyword expansion */



expandline(in, out, delta,delimstuffed,write)
FILE * in, * out; struct hshentry * delta;
register int delimstuffed, write;
/* Function: Reads a line from in and writes it to out.
 * If delimstuffed==true, double SDELIM is replaced with single SDELIM.
 * Keyword expansion is performed with data from delta.
 * If write==true, the string is also copied unchanged to frewrite.
 * Returns false if end-of-string or end-of-line is detected, true otherwise.
 */
{
	register c;
	register char * tp;
	char keystring[keylength+2];
	char keyval[keyvallength+2];
        enum markers matchresult;

	c=GETC(in,frewrite,write);
        for (;;) {
                if (c==EOF) {
                        if(delimstuffed) {
                                error("unterminated string");
                                nextc=c;
                        }
                        return(false);
                }

                if (c==SDELIM && delimstuffed) {
                        if ((c=GETC(in,frewrite,write))!=SDELIM) {
                                /* end of string */
                                nextc=c;
                                return false;
                        }
                }
                VOID putc(c,out);

                if (c=='\n') return true; /* end of line */

		if (c==KDELIM) {
                        /* check for keyword */
                        /* first, copy a long enough string into keystring */
			tp=keystring;
			while (((c=GETC(in,frewrite,write))!=EOF) && (tp<keystring+keylength) && (c!='\n')
			       && (c!=SDELIM) && (c!=KDELIM) && (c!=VDELIM)) {
                              VOID putc(c,out);
			      *tp++ = c;
                        }
			*tp++ = c; *tp = '\0';
			matchresult=trymatch(keystring,false);
			if (matchresult==Nomatch) continue;
			/* last c will be dealt with properly by continue*/

			/* Now we have a keyword terminated with a K/VDELIM */
			if (c==VDELIM) {
			      /* try to find closing KDELIM, and replace value */
			      tp=keyval;
			      while (((c=GETC(in,frewrite,write)) != EOF)
				     && (c!='\n') && (c!=KDELIM) && (tp<keyval+keyvallength)) {
				      *tp++ =c;
				      if (c==SDELIM && delimstuffed) { /*skip next SDELIM */
						c=GETC(in,frewrite,write);
						/* Can't be at end of string.
						/* always a \n before closing SDELIM */
				      }
			      }
			      if (c!=KDELIM) {
				    /* couldn't find closing KDELIM -- give up */
				    VOID putc(VDELIM,out); *tp='\0';
				    VOID fputs(keyval,out);
				    continue;   /* last c handled properly */
			      }
			}
			/* now put out the new keyword value */
			keyreplace(matchresult,delta,out);
                }
                c=GETC(in,frewrite,write);
        } /* end for */
}



keyreplace(marker,delta,out)
enum markers marker; struct hshentry * delta; FILE * out;
/* function: ouputs the keyword value(s) corresponding to marker.
 * Attributes are derived from delta.
 */
{
        char * date;
        register char * sp;

        date= delta->date;

        switch (marker) {
        case Author:
                VOID fprintf(out,"%c %s %c",VDELIM,delta->author,KDELIM);
                break;
        case Date:
                VOID putc(VDELIM,out);VOID putc(' ',out);
                VOID PRINTDATE(out,date);VOID putc(' ',out);
                VOID PRINTTIME(out,date);VOID putc(' ',out);VOID putc(KDELIM,out);
                break;
        case Id:
	case Header:
		VOID putc(VDELIM,out); VOID putc(' ',out);
		if (marker==Id)
			 VOID fputs(bindex(RCSfilename,'/'),out);
		else     VOID fputs(getfullRCSname(),out);
		VOID fprintf(out," %s ", delta->num);
                VOID PRINTDATE(out,date);VOID putc(' ',out);VOID PRINTTIME(out,date);
		VOID fprintf(out, " %s %s ",delta->author,delta->state);
		if (delta->lockedby!=nil)
			 VOID fprintf(out,"Locker: %s ",delta->lockedby);
		VOID putc(KDELIM,out);
                break;
        case Locker:
                VOID fprintf(out,"%c %s %c", VDELIM,
                        delta->lockedby==nil?"":delta->lockedby,KDELIM);
                break;
        case Log:
                VOID fprintf(out, "%c\t%s %c\n%sRevision %s  ",
                        VDELIM, bindex(RCSfilename,'/'), KDELIM, Comment, delta->num);
                VOID PRINTDATE(out,date);VOID fputs("  ",out);VOID PRINTTIME(out,date);
                VOID fprintf(out, "  %s\n%s",delta->author,Comment);
                /* do not include state here because it may change and is not updated*/
                sp = delta->log;
                while (*sp) if (putc(*sp++,out)=='\n') VOID fputs(Comment,out);
                /* Comment is the comment leader */
                break;
        case RCSfile:
                VOID fprintf(out,"%c %s %c",VDELIM,bindex(RCSfilename,'/'),KDELIM);
                break;
        case Revision:
                VOID fprintf(out,"%c %s %c",VDELIM,delta->num,KDELIM);
                break;
        case Source:
                VOID fprintf(out,"%c %s %c",VDELIM,getfullRCSname(),KDELIM);
                break;
        case State:
                VOID fprintf(out,"%c %s %c",VDELIM,delta->state,KDELIM);
                break;
        case Nomatch:
                VOID putc(KDELIM,out);
                break;
        }
}


