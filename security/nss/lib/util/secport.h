/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * secport.h - portability interfaces for security libraries
 *
 * This file abstracts out libc functionality that libsec depends on
 * 
 * NOTE - These are not public interfaces
 *
 * $Id: secport.h,v 1.4.4.1 2002/04/10 03:29:24 cltbld%netscape.com Exp $
 */

#ifndef _SECPORT_H_
#define _SECPORT_H_

/*
 * define XP_MAC, XP_WIN, XP_BEOS, or XP_UNIX, in case they are not defined
 * by anyone else
 */
#ifdef macintosh
# ifndef XP_MAC
# define XP_MAC 1
# endif
#endif

#ifdef _WINDOWS
# ifndef XP_WIN
# define XP_WIN
# endif
#if defined(_WIN32) || defined(WIN32)
# ifndef XP_WIN32
# define XP_WIN32
# endif
#else
# ifndef XP_WIN16
# define XP_WIN16
# endif
#endif
#endif

#ifdef __BEOS__
# ifndef XP_BEOS
# define XP_BEOS
# endif
#endif

#ifdef unix
# ifndef XP_UNIX
# define XP_UNIX
# endif
#endif

#if defined(__WATCOMC__) || defined(__WATCOM_CPLUSPLUS__)
#include "watcomfx.h"
#endif

#ifdef XP_MAC
#include <types.h>
#include <time.h> /* for time_t below */
#else
#include <sys/types.h>
#endif

#ifdef notdef
#ifdef XP_MAC
#include "NSString.h"
#endif
#endif

#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "prtypes.h"
#include "prlog.h"	/* for PR_ASSERT */
#include "plarena.h"
#include "plstr.h"

/*
 * HACK for NSS 2.8 to allow Admin to compile without source changes.
 */
#ifndef SEC_BEGIN_PROTOS
#include "seccomon.h"
#endif

SEC_BEGIN_PROTOS

extern void *PORT_Alloc(size_t len);
extern void *PORT_Realloc(void *old, size_t len);
extern void *PORT_AllocBlock(size_t len);
extern void *PORT_ReallocBlock(void *old, size_t len);
extern void PORT_FreeBlock(void *ptr);
extern void *PORT_ZAlloc(size_t len);
extern void PORT_Free(void *ptr);
extern void PORT_ZFree(void *ptr, size_t len);
extern time_t PORT_Time(void);
extern void PORT_SetError(int value);
extern int PORT_GetError(void);

extern PLArenaPool *PORT_NewArena(unsigned long chunksize);
extern void *PORT_ArenaAlloc(PLArenaPool *arena, size_t size);
extern void *PORT_ArenaZAlloc(PLArenaPool *arena, size_t size);
extern void PORT_FreeArena(PLArenaPool *arena, PRBool zero);
extern void *PORT_ArenaGrow(PLArenaPool *arena, void *ptr,
			    size_t oldsize, size_t newsize);
extern void *PORT_ArenaMark(PLArenaPool *arena);
extern void PORT_ArenaRelease(PLArenaPool *arena, void *mark);
extern void PORT_ArenaUnmark(PLArenaPool *arena, void *mark);
extern char *PORT_ArenaStrdup(PLArenaPool *arena, char *str);

#ifdef __cplusplus
}
#endif

#define PORT_Assert PR_ASSERT
#define PORT_ZNew(type) (type*)PORT_ZAlloc(sizeof(type))
#define PORT_New(type) (type*)PORT_Alloc(sizeof(type))
#define PORT_ArenaNew(poolp, type)	\
		(type*) PORT_ArenaAlloc(poolp, sizeof(type))
#define PORT_ArenaZNew(poolp, type)	\
		(type*) PORT_ArenaZAlloc(poolp, sizeof(type))
#define PORT_NewArray(type, num)	\
		(type*) PORT_Alloc (sizeof(type)*(num))
#define PORT_ZNewArray(type, num)	\
		(type*) PORT_ZAlloc (sizeof(type)*(num))
#define PORT_ArenaNewArray(poolp, type, num)	\
		(type*) PORT_ArenaAlloc (poolp, sizeof(type)*(num))
#define PORT_ArenaZNewArray(poolp, type, num)	\
		(type*) PORT_ArenaZAlloc (poolp, sizeof(type)*(num))

/* Please, keep these defines sorted alphbetically.  Thanks! */

#ifdef XP_STRING_FUNCS

#define PORT_Atoi 	XP_ATOI

#define PORT_Memcmp 	XP_MEMCMP
#define PORT_Memcpy 	XP_MEMCPY
#define PORT_Memmove 	XP_MEMMOVE
#define PORT_Memset 	XP_MEMSET

#define PORT_Strcasecmp XP_STRCASECMP
#define PORT_Strcat 	XP_STRCAT
#define PORT_Strchr 	XP_STRCHR
#define PORT_Strrchr	XP_STRRCHR
#define PORT_Strcmp 	XP_STRCMP
#define PORT_Strcpy 	XP_STRCPY
#define PORT_Strdup 	XP_STRDUP
#define PORT_Strlen(s) 	XP_STRLEN(s)
#define PORT_Strncasecmp XP_STRNCASECMP
#define PORT_Strncat 	strncat
#define PORT_Strncmp 	XP_STRNCMP
#define PORT_Strncpy 	strncpy
#define PORT_Strstr 	XP_STRSTR
#define PORT_Strtok 	XP_STRTOK_R

#define PORT_Tolower 	XP_TO_LOWER

#else /* XP_STRING_FUNCS */

#define PORT_Atoi 	atoi

#define PORT_Memcmp 	memcmp
#define PORT_Memcpy 	memcpy
#ifndef SUNOS4
#define PORT_Memmove 	memmove
#else /*SUNOS4*/
#define PORT_Memmove(s,ct,n)    bcopy ((ct), (s), (n))
#endif/*SUNOS4*/
#define PORT_Memset 	memset

#define PORT_Strcasecmp PL_strcasecmp
#define PORT_Strcat 	strcat
#define PORT_Strchr 	strchr
#define PORT_Strrchr    PL_strrchr
#define PORT_Strcmp 	strcmp
#define PORT_Strcpy 	strcpy
#define PORT_Strdup 	PL_strdup
#define PORT_Strlen(s) 	strlen(s)
#define PORT_Strncasecmp PL_strncasecmp
#define PORT_Strncat 	strncat
#define PORT_Strncmp 	strncmp
#define PORT_Strncpy 	strncpy
#define PORT_Strstr 	strstr
#define PORT_Strtok 	strtok

#define PORT_Tolower 	tolower

#endif /* XP_STRING_FUNCS */

typedef PRBool (PR_CALLBACK * PORTCharConversionWSwapFunc) (PRBool toUnicode,
			unsigned char *inBuf, unsigned int inBufLen,
			unsigned char *outBuf, unsigned int maxOutBufLen,
			unsigned int *outBufLen, PRBool swapBytes);

typedef PRBool (PR_CALLBACK * PORTCharConversionFunc) (PRBool toUnicode,
			unsigned char *inBuf, unsigned int inBufLen,
			unsigned char *outBuf, unsigned int maxOutBufLen,
			unsigned int *outBufLen);

#ifdef __cplusplus
extern "C" {
#endif

void PORT_SetUCS4_UTF8ConversionFunction(PORTCharConversionFunc convFunc);
void PORT_SetUCS2_ASCIIConversionFunction(PORTCharConversionWSwapFunc convFunc);
PRBool PORT_UCS4_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
			unsigned int inBufLen, unsigned char *outBuf,
			unsigned int maxOutBufLen, unsigned int *outBufLen);
PRBool PORT_UCS2_ASCIIConversion(PRBool toUnicode, unsigned char *inBuf,
			unsigned int inBufLen, unsigned char *outBuf,
			unsigned int maxOutBufLen, unsigned int *outBufLen,
			PRBool swapBytes);
void PORT_SetUCS2_UTF8ConversionFunction(PORTCharConversionFunc convFunc);
PRBool PORT_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
			unsigned int inBufLen, unsigned char *outBuf,
			unsigned int maxOutBufLen, unsigned int *outBufLen);

PR_EXTERN(PRBool)
sec_port_ucs4_utf8_conversion_function
(
  PRBool toUnicode,
  unsigned char *inBuf,
  unsigned int inBufLen,
  unsigned char *outBuf,
  unsigned int maxOutBufLen,
  unsigned int *outBufLen
);

PR_EXTERN(PRBool)
sec_port_ucs2_utf8_conversion_function
(
  PRBool toUnicode,
  unsigned char *inBuf,
  unsigned int inBufLen,
  unsigned char *outBuf,
  unsigned int maxOutBufLen,
  unsigned int *outBufLen
);

extern int NSS_PutEnv(const char * envVarName, const char * envValue);

SEC_END_PROTOS

#endif /* _SECPORT_H_ */
