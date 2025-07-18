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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: error.c,v $ $Revision: 1.3.18.1 $ $Date: 2002/04/10 03:26:29 $ $Name: MOZILLA_1_0_RELEASE $";
#endif /* DEBUG */

/*
 * error.c
 *
 * This file contains the code implementing the per-thread error 
 * stacks upon which most NSS routines report their errors.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/*
 * The stack itself has a header, and a sequence of integers.
 * The header records the amount of space (as measured in stack
 * slots) already allocated for the stack, and the count of the
 * number of records currently being used.
 */

struct stack_header_str {
  PRUint16 space;
  PRUint16 count;
};

struct error_stack_str {
  struct stack_header_str header;
  PRInt32 stack[1];
};
typedef struct error_stack_str error_stack;

/*
 * error_stack_index
 *
 * Thread-private data must be indexed.  This is that index.
 * See PR_NewThreadPrivateIndex for more information.
 */

static PRUintn error_stack_index;

/*
 * call_once
 *
 * The thread-private index must be obtained (once!) at runtime.
 * This block is used for that one-time call.
 */

static PRCallOnceType error_call_once;

/*
 * error_once_function
 *
 * This is the once-called callback.
 */

static PRStatus
error_once_function
(
  void
)
{
  return nss_NewThreadPrivateIndex(&error_stack_index,PR_Free);
  /* return PR_NewThreadPrivateIndex(&error_stack_index, PR_Free); */
}

/*
 * error_get_my_stack
 *
 * This routine returns the calling thread's error stack, creating
 * it if necessary.  It may return NULL upon error, which implicitly
 * means that it ran out of memory.
 */

static error_stack *
error_get_my_stack
(
  void
)
{
  PRStatus st;
  error_stack *rv;
  PRUintn new_size;
  PRUint32 new_bytes;
  error_stack *new_stack;

  if( 0 == error_stack_index ) {
    st = PR_CallOnce(&error_call_once, error_once_function);
    if( PR_SUCCESS != st ) {
      return (error_stack *)NULL;
    }
  }

  rv = (error_stack *)nss_GetThreadPrivate(error_stack_index);
  if( (error_stack *)NULL == rv ) {
    /* Doesn't exist; create one */
    new_size = 16;
  } else {
    if( rv->header.count == rv->header.space ) {
      /* Too small, expand it */
      new_size = rv->header.space + 16;
    } else {
      /* Okay, return it */
      return rv;
    }
  }

  new_bytes = (new_size * sizeof(PRInt32)) + 
    sizeof(struct stack_header_str);
  /* Use NSPR's calloc/realloc, not NSS's, to avoid loops! */
  new_stack = PR_Calloc(1, new_bytes);
  
  if( (error_stack *)NULL != new_stack ) {
    if( (error_stack *)NULL != rv ) {
	(void)nsslibc_memcpy(new_stack,rv,rv->header.space);
    }
    new_stack->header.space = new_size;
  }

  /* Set the value, whether or not the allocation worked */
  nss_SetThreadPrivate(error_stack_index, new_stack);
  return new_stack;
}

/*
 * The error stack
 *
 * The public methods relating to the error stack are:
 *
 *  NSS_GetError
 *  NSS_GetErrorStack
 *
 * The nonpublic methods relating to the error stack are:
 *
 *  nss_SetError
 *  nss_ClearErrorStack
 *
 */

/*
 * NSS_GetError
 *
 * This routine returns the highest-level (most general) error set
 * by the most recent NSS library routine called by the same thread
 * calling this routine.
 *
 * This routine cannot fail.  However, it may return zero, which
 * indicates that the previous NSS library call did not set an error.
 *
 * Return value:
 *  0 if no error has been set
 *  A nonzero error number
 */

NSS_IMPLEMENT PRInt32
NSS_GetError
(
  void
)
{
  error_stack *es = error_get_my_stack();

  if( (error_stack *)NULL == es ) {
    return NSS_ERROR_NO_MEMORY; /* Good guess! */
  }

  if( 0 == es->header.count ) {
    return 0;
  }

  return es->stack[ es->header.count-1 ];
}

/*
 * NSS_GetErrorStack
 *
 * This routine returns a pointer to an array of integers, containing
 * the entire sequence or "stack" of errors set by the most recent NSS
 * library routine called by the same thread calling this routine.
 * NOTE: the caller DOES NOT OWN the memory pointed to by the return
 * value.  The pointer will remain valid until the calling thread
 * calls another NSS routine.  The lowest-level (most specific) error 
 * is first in the array, and the highest-level is last.  The array is
 * zero-terminated.  This routine may return NULL upon error; this
 * indicates a low-memory situation.
 *
 * Return value:
 *  NULL upon error, which is an implied NSS_ERROR_NO_MEMORY
 *  A NON-caller-owned pointer to an array of integers
 */

NSS_IMPLEMENT PRInt32 *
NSS_GetErrorStack
(
  void
)
{
  error_stack *es = error_get_my_stack();

  if( (error_stack *)NULL == es ) {
    return (PRInt32 *)NULL;
  }

  /* Make sure it's terminated */
  es->stack[ es->header.count ] = 0;

  return es->stack;
}

/*
 * nss_SetError
 *
 * This routine places a new error code on the top of the calling 
 * thread's error stack.  Calling this routine wiht an error code
 * of zero will clear the error stack.
 */

NSS_IMPLEMENT void
nss_SetError
(
  PRUint32 error
)
{
  error_stack *es;

  if( 0 == error ) {
    nss_ClearErrorStack();
    return;
  }

  es = error_get_my_stack();
  if( (error_stack *)NULL == es ) {
    /* Oh, well. */
    return;
  }

  es->stack[ es->header.count ] = error;
  es->header.count++;
  return;
}

/*
 * nss_ClearErrorStack
 *
 * This routine clears the calling thread's error stack.
 */

NSS_IMPLEMENT void
nss_ClearErrorStack
(
  void
)
{
  error_stack *es = error_get_my_stack();
  if( (error_stack *)NULL == es ) {
    /* Oh, well. */
    return;
  }

  es->header.count = 0;
  es->stack[0] = 0;
  return;
}
