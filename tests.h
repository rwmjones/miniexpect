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

#ifndef TESTS_H_
#define TESTS_H_

#include <stdio.h>
#include <sys/wait.h>

static inline int
test_is_sighup (int status)
{
  return WIFSIGNALED (status) && WTERMSIG (status) == SIGHUP;
}

static inline void
test_diagnose (int status)
{
  if (WIFEXITED (status))
    fprintf (stderr, "exited with code %d", WEXITSTATUS (status));
  else if (WIFSIGNALED (status))
    fprintf (stderr, "terminated by signal %d", WTERMSIG (status));
  else if (WIFSTOPPED (status))
    fprintf (stderr, "stopped by signal %d", WSTOPSIG (status));
}

#endif /* TESTS_H_ */
