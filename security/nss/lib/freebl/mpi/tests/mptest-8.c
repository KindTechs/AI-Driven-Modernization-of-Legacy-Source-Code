/*
 *  Simple test driver for MPI library
 *
 *  Test 8: Probabilistic primality tester
 *
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
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
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
 * may use your version of this file under either the MPL or the GPL.
 *
 *  $Id: mptest-8.c,v 1.2.144.1 2002/04/10 03:27:40 cltbld%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#define MP_IOFUNC
#include "mpi.h"

#include "mpprime.h"

int main(int argc, char *argv[])
{
  int       ix;
  mp_digit  num;
  mp_int    a;

  srand(time(NULL));

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <a>\n", argv[0]);
    return 1;
  }

  printf("Test 8: Probabilistic primality testing\n\n");

  mp_init(&a);

  mp_read_radix(&a, argv[1], 10);

  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);

  printf("\nChecking for divisibility by small primes ... \n");
  num = 170;
  if(mpp_divis_primes(&a, &num) == MP_YES) {
    printf("it is not prime\n");
    goto CLEANUP;
  }
  printf("Passed that test (not divisible by any small primes).\n");

  for(ix = 0; ix < 10; ix++) {
    printf("\nPerforming Rabin-Miller test, iteration %d\n", ix + 1);

    if(mpp_pprime(&a, 5) == MP_NO) {
      printf("it is not prime\n");
      goto CLEANUP;
    }
  }
  printf("All tests passed; a is probably prime\n");

CLEANUP:
  mp_clear(&a);

  return 0;
}
