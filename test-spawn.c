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
main (int argc __attribute__ ((unused)), char *argv[])
{
  mexp_h *h;
  int status;

  h = mexp_spawnl ("cat", "cat", NULL);
  assert (h != NULL);
  status = mexp_close (h);
  if (status != 0 && !test_is_sighup (status)) {
    fprintf (stderr, "%s: non-zero exit status from subcommand: ", argv[0]);
    test_diagnose (status);
    fprintf (stderr, "\n");
    exit (EXIT_FAILURE);
  }

  exit (EXIT_SUCCESS);
}
