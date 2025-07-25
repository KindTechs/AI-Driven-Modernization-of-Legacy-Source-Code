/*
 * $XFree86: xc/lib/fontconfig/fontconfig/fcprivate.h,v 1.2 2002/02/15 06:01:27 keithp Exp $
 *
 * Copyright � 2001 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FCPRIVATE_H_
#define _FCPRIVATE_H_

/*
 * I tried this with functions that took va_list* arguments
 * but portability concerns made me change these functions
 * into macros (sigh).
 */

#define FcPatternVapBuild(result, orig, va)			    \
{								    \
    FcPattern	*__p__ = (orig);				    \
    const char	*__o__;						    \
    FcValue	__v__;						    \
								    \
    if (!__p__)							    \
    {								    \
	__p__ = FcPatternCreate ();				    \
	if (!__p__)		    				    \
	    goto _FcPatternVapBuild_bail0;			    \
    }				    				    \
    for (;;)			    				    \
    {				    				    \
	__o__ = va_arg (va, const char *);			    \
	if (!__o__)		    				    \
	    break;		    				    \
	__v__.type = va_arg (va, FcType);			    \
	switch (__v__.type) {	    				    \
	case FcTypeVoid:					    \
	    goto _FcPatternVapBuild_bail1;       		    \
	case FcTypeInteger:	    				    \
	    __v__.u.i = va_arg (va, int);			    \
	    break;						    \
	case FcTypeDouble:					    \
	    __v__.u.d = va_arg (va, double);			    \
	    break;						    \
	case FcTypeString:					    \
	    __v__.u.s = va_arg (va, FcChar8 *);			    \
	    break;						    \
	case FcTypeBool:					    \
	    __v__.u.b = va_arg (va, FcBool);			    \
	    break;						    \
	case FcTypeMatrix:					    \
	    __v__.u.m = va_arg (va, FcMatrix *);		    \
	    break;						    \
	case FcTypeCharSet:					    \
	    __v__.u.c = va_arg (va, FcCharSet *);		    \
	    break;						    \
	}							    \
	if (!FcPatternAdd (__p__, __o__, __v__, FcTrue))	    \
	    goto _FcPatternVapBuild_bail1;			    \
    }								    \
    result = __p__;						    \
    goto _FcPatternVapBuild_return;				    \
								    \
_FcPatternVapBuild_bail1:					    \
    if (!orig)							    \
	FcPatternDestroy (__p__);				    \
_FcPatternVapBuild_bail0:					    \
    result = 0;							    \
								    \
_FcPatternVapBuild_return:					    \
    ;								    \
}


#define FcObjectSetVapBuild(__ret__, __first__, __va__) 		\
{									\
    FcObjectSet    *__os__;						\
    const char	    *__ob__;						\
									\
    __ret__ = 0;						    	\
    __os__ = FcObjectSetCreate ();					\
    if (!__os__)							\
	goto _FcObjectSetVapBuild_bail0;				\
    __ob__ = __first__;							\
    while (__ob__)							\
    {									\
	if (!FcObjectSetAdd (__os__, __ob__))				\
	    goto _FcObjectSetVapBuild_bail1;				\
	__ob__ = va_arg (__va__, const char *);				\
    }									\
    __ret__ = __os__;							\
									\
_FcObjectSetVapBuild_bail1:						\
    if (!__ret__ && __os__)					    	\
	FcObjectSetDestroy (__os__);					\
_FcObjectSetVapBuild_bail0:						\
    ;									\
}

#endif /* _FCPRIVATE_H_ */

