/*
 * softmagic - interpret variable magic from MAGIC
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include "system.h"
#include "file.h"
#include "debug.h"

FILE_RCSID("@(#)Id: softmagic.c,v 1.51 2002/07/03 18:26:38 christos Exp ")

/*@access fmagic @*/

/*@-bounds@*/
static int32_t
fmagicSPrint(const fmagic fm, struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
    union VALUETYPE * p = &fm->val;
    uint32_t v;
    int32_t t = 0;

    switch (m->type) {
    case FILE_BYTE:
	v = signextend(m, p->b);
	fmagicPrintf(fm, m->desc, (unsigned char) v);
/*@-sizeoftype@*/
	t = m->offset + sizeof(char);
/*@=sizeoftype@*/
	break;

    case FILE_SHORT:
    case FILE_BESHORT:
    case FILE_LESHORT:
	v = signextend(m, p->h);
	fmagicPrintf(fm, m->desc, (unsigned short) v);
/*@-sizeoftype@*/
	t = m->offset + sizeof(short);
/*@=sizeoftype@*/
	break;

    case FILE_LONG:
    case FILE_BELONG:
    case FILE_LELONG:
	v = signextend(m, p->l);
	fmagicPrintf(fm, m->desc, (uint32_t) v);
/*@-sizeoftype@*/
	t = m->offset + sizeof(int32_t);
/*@=sizeoftype@*/
  	break;

    case FILE_STRING:
    case FILE_PSTRING:
	if (m->reln == '=') {
	    fmagicPrintf(fm, m->desc, m->value.s);
	    t = m->offset + strlen(m->value.s);
	} else {
	    if (*m->value.s == '\0') {
		char *cp = strchr(p->s,'\n');
		if (cp != NULL)
		    *cp = '\0';
	    }
	    fmagicPrintf(fm, m->desc, p->s);
	    t = m->offset + strlen(p->s);
	}
	break;

    case FILE_DATE:
    case FILE_BEDATE:
    case FILE_LEDATE:
	fmagicPrintf(fm, m->desc, fmttime(p->l, 1));
/*@-sizeoftype@*/
	t = m->offset + sizeof(time_t);
/*@=sizeoftype@*/
	break;

    case FILE_LDATE:
    case FILE_BELDATE:
    case FILE_LELDATE:
	fmagicPrintf(fm, m->desc, fmttime(p->l, 0));
/*@-sizeoftype@*/
	t = m->offset + sizeof(time_t);
/*@=sizeoftype@*/
	break;

    case FILE_REGEX:
  	fmagicPrintf(fm, m->desc, p->s);
	t = m->offset + strlen(p->s);
	break;

    default:
	error(EXIT_FAILURE, 0, "invalid m->type (%d) in fmagicSPrint().\n", m->type);
	/*@notreached@*/ break;
    }
    return(t);
}
/*@=bounds@*/

/*
 * Convert the byte order of the data we are looking at
 * While we're here, let's apply the mask operation
 * (unless you have a better idea)
 */
/*@-bounds@*/
static int
fmagicSConvert(fmagic fm, struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
    union VALUETYPE * p = &fm->val;

    switch (m->type) {
    case FILE_BYTE:
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->b &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->b |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->b ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->b += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->b -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->b *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->b /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->b %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->b = ~p->b;
	return 1;
    case FILE_SHORT:
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->h &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->h |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->h ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->h += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->h -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->h *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->h /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->h %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->h = ~p->h;
	return 1;
    case FILE_LONG:
    case FILE_DATE:
    case FILE_LDATE:
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->l &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->l |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->l ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->l += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->l -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->l *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->l /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->l %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->l = ~p->l;
	return 1;
    case FILE_STRING:
	{
	    int n;

	    /* Null terminate and eat *trailing* return */
	    p->s[sizeof(p->s) - 1] = '\0';
	    n = strlen(p->s) - 1;
	    if (p->s[n] == '\n')
		p->s[n] = '\0';
	    return 1;
	}
    case FILE_PSTRING:
	{
	    char *ptr1 = p->s, *ptr2 = ptr1 + 1;
	    int n = *p->s;
	    if (n >= sizeof(p->s))
		n = sizeof(p->s) - 1;
	    while (n--)
		*ptr1++ = *ptr2++;
	    *ptr1 = '\0';
	    n = strlen(p->s) - 1;
	    if (p->s[n] == '\n')
		p->s[n] = '\0';
	    return 1;
	}
    case FILE_BESHORT:
	p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->h &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->h |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->h ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->h += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->h -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->h *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->h /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->h %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->h = ~p->h;
	return 1;
    case FILE_BELONG:
    case FILE_BEDATE:
    case FILE_BELDATE:
	p->l = (int32_t)
	    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->l &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->l |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->l ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->l += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->l -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->l *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->l /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->l %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->l = ~p->l;
	return 1;
    case FILE_LESHORT:
	p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->h &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->h |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->h ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->h += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->h -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->h *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->h /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->h %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->h = ~p->h;
	return 1;
    case FILE_LELONG:
    case FILE_LEDATE:
    case FILE_LELDATE:
	p->l = (int32_t)
	    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
	if (m->mask)
	    switch (m->mask_op&0x7F) {
	    case FILE_OPAND:
		p->l &= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPOR:
		p->l |= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPXOR:
		p->l ^= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPADD:
		p->l += m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMINUS:
		p->l -= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMULTIPLY:
		p->l *= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPDIVIDE:
		p->l /= m->mask;
		/*@innerbreak@*/ break;
	    case FILE_OPMODULO:
		p->l %= m->mask;
		/*@innerbreak@*/ break;
	    }
	if (m->mask_op & FILE_OPINVERSE)
	    p->l = ~p->l;
	return 1;
    case FILE_REGEX:
	return 1;
    default:
	error(EXIT_FAILURE, 0, "invalid type %d in fmagicSConvert().\n", m->type);
	/*@notreached@*/
	return 0;
    }
}
/*@=bounds@*/


static void
fmagicSDebug(int32_t offset, char *str, int len)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    (void) fprintf(stderr, "fmagicSGet @%d: ", offset);
    showstr(stderr, (char *) str, len);
    (void) fputc('\n', stderr);
    (void) fputc('\n', stderr);
}

/*@-bounds@*/
static int
fmagicSGet(fmagic fm, struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
    unsigned char * buf = fm->buf;
    int nb = fm->nb;
    union VALUETYPE * p = &fm->val;
    int32_t offset = m->offset;

/*@-branchstate@*/
    if (m->type == FILE_REGEX) {
	/*
	* offset is interpreted as last line to search,
	* (starting at 1), not as bytes-from start-of-file
	*/
	char *last = NULL;
/*@-temptrans@*/
	p->buf = buf;
/*@=temptrans@*/
	for (; offset && (buf = strchr(buf, '\n')) != NULL; offset--, buf++)
	    last = buf;
	if (last != NULL)
	    *last = '\0';
    } else if (offset + sizeof(*p) <= nb)
	memcpy(p, buf + offset, sizeof(*p));
    else {
	/*
	 * the usefulness of padding with zeroes eludes me, it
	 * might even cause problems
	 */
	int32_t have = nb - offset;
	memset(p, 0, sizeof(*p));
	if (have > 0)
	    memcpy(p, buf + offset, have);
    }
/*@=branchstate@*/

    if (fm->flags & FMAGIC_FLAGS_DEBUG) {
	fmagicSDebug(offset, (char *) p, sizeof(*p));
	mdump(m);
    }

    if (m->flag & INDIR) {
	switch (m->in_type) {
	case FILE_BYTE:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = p->b & m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = p->b | m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = p->b ^ m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = p->b + m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = p->b - m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = p->b * m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = p->b / m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = p->b % m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_BESHORT:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) &
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) |
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) ^
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) +
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) -
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) *
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) /
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = (short)((p->hs[0]<<8) | (p->hs[1])) %
				 m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_LESHORT:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) &
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) |
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) ^
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) +
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) -
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) *
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) /
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = (short)((p->hs[1]<<8) | (p->hs[0])) %
				 m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_SHORT:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = p->h & m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = p->h | m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = p->h ^ m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = p->h + m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = p->h - m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = p->h * m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = p->h / m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = p->h % m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_BELONG:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) &
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) |
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) ^
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) +
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) -
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) *
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) /
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = (int32_t)(	(p->hl[0]<<24) | (p->hl[1]<<16) |
					(p->hl[2]<< 8) | (p->hl[3])) %
				 m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_LELONG:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) &
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) |
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) ^
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) +
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) -
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) *
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					 (p->hl[1]<< 8) | (p->hl[0])) /
				 m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = (int32_t)(	(p->hl[3]<<24) | (p->hl[2]<<16) |
					(p->hl[1]<< 8) | (p->hl[0])) %
				 m->in_offset;
		    /*@innerbreak@*/ break;
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	case FILE_LONG:
	    if (m->in_offset)
		switch (m->in_op&0x7F) {
		case FILE_OPAND:
		    offset = p->l & m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPOR:
		    offset = p->l | m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPXOR:
		    offset = p->l ^ m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPADD:
		    offset = p->l + m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMINUS:
		    offset = p->l - m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMULTIPLY:
		    offset = p->l * m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPDIVIDE:
		    offset = p->l / m->in_offset;
		    /*@innerbreak@*/ break;
		case FILE_OPMODULO:
		    offset = p->l % m->in_offset;
		    /*@innerbreak@*/ break;
	    /*	case TOOMANYSWITCHBLOCKS:
	     *	    ugh = p->eye % m->strain;
	     *	    rub;
	     *	case BEER:
	     *	    off = p->tab & m->in_gest;
	     *	    sleep;
	     */
		}
	    if (m->in_op & FILE_OPINVERSE)
		offset = ~offset;
	    break;
	}

/*@-compmempass@*/
	if (buf == NULL || offset + sizeof(*p) > nb)
	    return 0;
/*@=compmempass@*/

	memcpy(p, buf + offset, sizeof(*p));

	if (fm->flags & FMAGIC_FLAGS_DEBUG) {
	    fmagicSDebug(offset, (char *) p, sizeof(*p));
	    mdump(m);
	}
    }
/*@-compmempass@*/
    if (!fmagicSConvert(fm, m))
	return 0;
    return 1;
/*@=compmempass@*/
}
/*@=bounds@*/

/*@-bounds@*/
static int
fmagicSCheck(const fmagic fm, struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    union VALUETYPE * p = &fm->val;
    uint32_t l = m->value.l;
    uint32_t v = 0;
    int matched;

    if ( (m->value.s[0] == 'x') && (m->value.s[1] == '\0') ) {
	fprintf(stderr, "BOINK");
	return 1;
    }

    switch (m->type) {
    case FILE_BYTE:
	v = p->b;
	break;

    case FILE_SHORT:
    case FILE_BESHORT:
    case FILE_LESHORT:
	v = p->h;
	break;

    case FILE_LONG:
    case FILE_BELONG:
    case FILE_LELONG:
    case FILE_DATE:
    case FILE_BEDATE:
    case FILE_LEDATE:
    case FILE_LDATE:
    case FILE_BELDATE:
    case FILE_LELDATE:
	v = p->l;
	break;

    case FILE_STRING:
    case FILE_PSTRING:
    {
	/*
	 * What we want here is:
	 * v = strncmp(m->value.s, p->s, m->vallen);
	 * but ignoring any nulls.  bcmp doesn't give -/+/0
	 * and isn't universally available anyway.
	 */
	unsigned char *a = (unsigned char*)m->value.s;
	unsigned char *b = (unsigned char*)p->s;
	int len = m->vallen;
	l = 0;
	v = 0;
	if (0L == m->mask) { /* normal string: do it fast */
	    while (--len >= 0)
		if ((v = *b++ - *a++) != '\0')
		    /*@loopbreak@*/ break; 
	} else { /* combine the others */
	    while (--len >= 0) {
		if ((m->mask & STRING_IGNORE_LOWERCASE) && islower(*a)) {
		    if ((v = tolower(*b++) - *a++) != '\0')
			/*@loopbreak@*/ break;
		} else
		if ((m->mask & STRING_COMPACT_BLANK) && isspace(*a)) { 
		    a++;
		    if (isspace(*b++)) {
			while (isspace(*b))
			    b++;
		    } else {
			v = 1;
			/*@loopbreak@*/ break;
		    }
		} else
		if (isspace(*a) && (m->mask & STRING_COMPACT_OPTIONAL_BLANK)) {
		    a++;
		    while (isspace(*b))
			b++;
		} else {
		    if ((v = *b++ - *a++) != '\0')
			/*@loopbreak@*/ break;
		}
	    }
	}
	break;
    }
    case FILE_REGEX:
    {
	int rc;
	regex_t rx;
	char errmsg[512];

	rc = regcomp(&rx, m->value.s, REG_EXTENDED|REG_NOSUB);
	if (rc) {
	    (void) regerror(rc, &rx, errmsg, sizeof(errmsg));
	    error(EXIT_FAILURE, 0, "regex error %d, (%s)\n", rc, errmsg);
	    /*@notreached@*/
	} else {
	    rc = regexec(&rx, p->buf, 0, NULL, 0);
	    return !rc;
	}
    }
	/*@notreached@*/ break;
    default:
	error(EXIT_FAILURE, 0, "invalid type %d in fmagicSCheck().\n", m->type);
	/*@notreached@*/
	return 0;
    }

    if(m->type != FILE_STRING && m->type != FILE_PSTRING)
	v = signextend(m, v);

    switch (m->reln) {
    case 'x':
	if (fm->flags & FMAGIC_FLAGS_DEBUG)
	    (void) fprintf(stderr, "%u == *any* = 1\n", v);
	matched = 1;
	break;

    case '!':
	matched = v != l;
	if (fm->flags & FMAGIC_FLAGS_DEBUG)
	    (void) fprintf(stderr, "%u != %u = %d\n",
			       v, l, matched);
	break;

    case '=':
	matched = v == l;
	if (fm->flags & FMAGIC_FLAGS_DEBUG)
	    (void) fprintf(stderr, "%u == %u = %d\n",
			       v, l, matched);
	break;

    case '>':
	if (m->flag & UNSIGNED) {
	    matched = v > l;
	    if (fm->flags & FMAGIC_FLAGS_DEBUG)
		(void) fprintf(stderr, "%u > %u = %d\n", v, l, matched);
	}
	else {
	    matched = (int32_t) v > (int32_t) l;
	    if (fm->flags & FMAGIC_FLAGS_DEBUG)
		(void) fprintf(stderr, "%d > %d = %d\n", v, l, matched);
	}
	break;

    case '<':
	if (m->flag & UNSIGNED) {
	    matched = v < l;
	    if (fm->flags & FMAGIC_FLAGS_DEBUG)
		(void) fprintf(stderr, "%u < %u = %d\n", v, l, matched);
	}
	else {
	    matched = (int32_t) v < (int32_t) l;
	    if (fm->flags & FMAGIC_FLAGS_DEBUG)
		(void) fprintf(stderr, "%d < %d = %d\n", v, l, matched);
	}
	break;

    case '&':
	matched = (v & l) == l;
	if (fm->flags & FMAGIC_FLAGS_DEBUG)
	    (void) fprintf(stderr, "((%x & %x) == %x) = %d\n", v, l, l, matched);
	break;

    case '^':
	matched = (v & l) != l;
	if (fm->flags & FMAGIC_FLAGS_DEBUG)
	    (void) fprintf(stderr, "((%x & %x) != %x) = %d\n", v, l, l, matched);
	break;

    default:
	matched = 0;
	error(EXIT_FAILURE, 0, "fmagicSCheck: can't happen: invalid relation %d.\n", m->reln);
	/*@notreached@*/ break;
    }

    return matched;
}
/*@=bounds@*/

/*
 * Go through the whole list, stopping if you find a match.  Process all
 * the continuations of that match before returning.
 *
 * We support multi-level continuations:
 *
 *	At any time when processing a successful top-level match, there is a
 *	current continuation level; it represents the level of the last
 *	successfully matched continuation.
 *
 *	Continuations above that level are skipped as, if we see one, it
 *	means that the continuation that controls them - i.e, the
 *	lower-level continuation preceding them - failed to match.
 *
 *	Continuations below that level are processed as, if we see one,
 *	it means we've finished processing or skipping higher-level
 *	continuations under the control of a successful or unsuccessful
 *	lower-level continuation, and are now seeing the next lower-level
 *	continuation and should process it.  The current continuation
 *	level reverts to the level of the one we're seeing.
 *
 *	Continuations at the current level are processed as, if we see
 *	one, there's no lower-level continuation that may have failed.
 *
 *	If a continuation matches, we bump the current continuation level
 *	so that higher-level continuations are processed.
 */
/*@-bounds@*/
static int
fmagicSMatch(const fmagic fm)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
    struct magic * m;
    uint32_t nmagic = fm->ml->nmagic;
    int cont_level = 0;
    int need_separator = 0;
    /*@only@*/
    static int32_t * tmpoff = NULL;
    static int tmpdelta = 64;
    static size_t tmplen = 0;
    int32_t oldoff = 0;
    int firstline = 1; /* a flag to print X\n  X\n- X */
    int ret = 0; /* if a match is found it is set to 1*/
    int i;

    for (i = 0; i < nmagic; i++) {
	m = &fm->ml->magic[i];
	/* if main entry matches, print it... */
	if (!fmagicSGet(fm, m) || !fmagicSCheck(fm, m)) {
	    /* main entry didn't match, flush its continuations */
	    while ((m+1)->cont_level != 0 && ++i < nmagic)
		m++;
	    continue;
	}

	if (! firstline) { /* we found another match */
	    /* put a newline and '-' to do some simple formatting */
	    fmagicPrintf(fm, "\n- ");
	}

	if ((cont_level+1) >= tmplen) {
	    tmplen += tmpdelta;
	    tmpoff = xrealloc(tmpoff, tmplen * sizeof(*tmpoff));
	}
	tmpoff[cont_level] = fmagicSPrint(fm, m);
	cont_level++;

	/*
	 * If we printed something, we'll need to print
	 * a blank before we print something else.
	 */
	if (m->desc[0])
	    need_separator = 1;

	/* and any continuations that match */
	while ((m+1)->cont_level != 0 && ++i < nmagic) {
	    m++;
	    if (cont_level < m->cont_level)
		/*@innercontinue@*/ continue;
	    if (cont_level > m->cont_level) {
		/* We're at the end of the level "cont_level" continuations. */
		cont_level = m->cont_level;
	    }
	    if (m->flag & OFFADD) {
		oldoff = m->offset;
		m->offset += tmpoff[cont_level-1];
	    }
	    if (fmagicSGet(fm, m) && fmagicSCheck(fm, m)) {
		/*
		 * This continuation matched.
		 * Print its message, with a blank before it if the previous
		 * item printed and this item isn't empty.
		 */
		/* space if previous printed */
		if (need_separator
		   && (m->nospflag == 0) && (m->desc[0] != '\0'))
		{
		    fmagicPrintf(fm, " ");
		    need_separator = 0;
		}
		if ((cont_level+1) >= tmplen) {
		    tmplen += tmpdelta;
		    tmpoff = xrealloc(tmpoff, tmplen * sizeof(*tmpoff));
		}
		tmpoff[cont_level] = fmagicSPrint(fm, m);
		cont_level++;
		if (m->desc[0])
		    need_separator = 1;
	    }
	    if (m->flag & OFFADD)
		m->offset = oldoff;
	}
	firstline = 0;
	ret = 1;
	if (!(fm->flags & FMAGIC_FLAGS_CONTINUE)) /* don't keep searching */
	    return 1;
    }
    return ret;	/* This is hit if -k is set or there is no match */
}
/*@=bounds@*/

/*
 * fmagicS - lookup one file in database 
 * (already read from MAGIC by apprentice.c).
 * Passed the name and FILE * of one file to be typed.
 */
int
fmagicS(fmagic fm)
{
/*@-branchstate@*/
    if (fm->mlist != NULL)
    for (fm->ml = fm->mlist->next; fm->ml != fm->mlist; fm->ml = fm->ml->next) {
/*@-compmempass@*/
	if (fmagicSMatch(fm))
	    return 1;
/*@=compmempass@*/
    }
/*@=branchstate@*/

/*@-compmempass@*/
    return 0;
/*@=compmempass@*/
}
