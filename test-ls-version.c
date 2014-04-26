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

/* Note this test depends on the output of 'ls --version'.  You may
 * have to extend the regular expressions below if running on a
 * non-Linux OS.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <pcre.h>

#include "miniexpect.h"
#include "tests.h"

int
main (int argc, char *argv[])
{
  mexp_h *h;
  int status, r;
  pcre *ls_coreutils_re;
  pcre *ls_busybox_re;
  const int ovecsize = 12;
  int ovector[ovecsize];
  const char *version;

  ls_coreutils_re = test_compile_re ("^ls.* ([.\\d]+)");
  /* Busybox doesn't actually recognize the --version option, but
   * it prints out the version string in its error message.
   */
  ls_busybox_re = test_compile_re ("^BusyBox v([.\\d+]) ");

  h = mexp_spawnl ("ls", "ls", "--version", NULL);
  assert (h != NULL);

  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, ls_coreutils_re },
                         { 101, ls_busybox_re },
                         { 0 },
                       }, ovector, ovecsize)) {
  case 100:
  case 101:
    /* Get the matched version number. */
    r = pcre_get_substring (h->buffer, ovector, h->pcre_error, 1, &version);
    if (r < 0) {
      fprintf (stderr, "error: PCRE error reading version substring: %d\n",
               r);
      exit (EXIT_FAILURE);
    }
    printf ("ls version = %s\n", version);
    pcre_free_substring (version);
    break;
  case MEXP_EOF:
    fprintf (stderr, "error: EOF before matching version string\n");
    exit (EXIT_FAILURE);
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before matching version string\n");
    exit (EXIT_FAILURE);
  case MEXP_ERROR:
    perror ("mexp_expect");
    exit (EXIT_FAILURE);
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", h->pcre_error);
    exit (EXIT_FAILURE);
  }

  status = mexp_close (h);
  if (status != 0 && !test_is_sighup (status)) {
    fprintf (stderr, "%s: non-zero exit status from subcommand: ", argv[0]);
    test_diagnose (status);
    fprintf (stderr, "\n");
    exit (EXIT_FAILURE);
  }

  pcre_free (ls_coreutils_re);
  pcre_free (ls_busybox_re);

  exit (EXIT_SUCCESS);
}
