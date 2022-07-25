/* miniexpect test suite
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "miniexpect.h"
#include "tests.h"

int
main (int argc, char *argv[])
{
  mexp_h *h;
  int status;
  int r;
  int rv[5];
  size_t i;
  pcre2_code *multi_re = test_compile_re ("multi");
  pcre2_code *match_re = test_compile_re ("match");
  pcre2_code *ing_re = test_compile_re ("ing");
  pcre2_code *str_re = test_compile_re ("str");
  pcre2_code *s_re = test_compile_re ("s");
  pcre2_match_data *match_data = pcre2_match_data_create (4, NULL);

  /* If the subprocess prints multiple things, we should be able to
   * repeatedly call mexp_expect to match on each one.  This didn't
   * work correctly in earlier versions of the library.
   */
  h = mexp_spawnl ("echo", "echo", "multimatchingstrs", NULL);
  assert (h != NULL);

  for (i = 0; i < 5; ++i) {
    r = mexp_expect (h,
                     (mexp_regexp[]) {
                       { 100, multi_re },
                       { 101, match_re },
                       { 102, ing_re },
                       { 103, str_re },
                       { 104, s_re },
                       { 0 },
                     }, match_data);
    switch (r) {
    case 100: case 101: case 102: case 103: case 104:
      printf ("iteration %zu: matched %d\n", i, r);
      rv[i] = r;
      break;
    case MEXP_EOF:
      fprintf (stderr, "error: unexpected EOF in iteration %zu\n", i);
      exit (EXIT_FAILURE);
    case MEXP_TIMEOUT:
      fprintf (stderr, "error: unexpected timeout in iteration %zu\n", i);
      exit (EXIT_FAILURE);
    case MEXP_ERROR:
        perror ("mexp_expect");
      exit (EXIT_FAILURE);
    case MEXP_PCRE_ERROR:
      fprintf (stderr, "error: PCRE error: %d\n", mexp_get_pcre_error (h));
      exit (EXIT_FAILURE);
    }
  }

  assert (rv[0] == 100); /* multi */
  assert (rv[1] == 101); /* match */
  assert (rv[2] == 102); /* ing */
  assert (rv[3] == 103); /* str */
  assert (rv[4] == 104); /* s */

  status = mexp_close (h);
  if (status != 0 && !test_is_sighup (status)) {
    fprintf (stderr, "%s: non-zero exit status from subcommand: ", argv[0]);
    test_diagnose (status);
    fprintf (stderr, "\n");
    exit (EXIT_FAILURE);
  }

  exit (EXIT_SUCCESS);
}
