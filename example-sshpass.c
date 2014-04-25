/* miniexpect example.
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

/* There is a program called 'sshpass' which does roughly the same
 * as this simplified example.  We run ssh as a subprocess, and log
 * in using the given password from the command line.
 *
 * The first argument is the password to send at the password prompt.
 * The remaining arguments are passed to the ssh subprocess.
 *
 * For example:
 *   sshpass 123456 ssh remote.example.com
 *   sshpass 123456 ssh -l root remote.example.com
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <pcre.h>

#include "miniexpect.h"

static pcre *
compile_re (const char *rex)
{
  const char *errptr;
  int erroffset;
  pcre *ret;

  ret = pcre_compile (rex, 0, &errptr, &erroffset, NULL);
  if (ret == NULL) {
    fprintf (stderr, "error: failed to compile regular expression '%s': %s at offset %d\n",
             rex, errptr, erroffset);
    exit (EXIT_FAILURE);
  }
  return ret;
}

int
main (int argc, char *argv[])
{
  mexp_h *h;
  const char *password;
  int status;
  pcre *password_re, *prompt_re, *hello_re;
  const int ovecsize = 12;
  int ovector[ovecsize];
  int pcre_err;

  if (argc <= 3) {
    fprintf (stderr, "usage: sshpass PASSWORD ssh [SSH-ARGS...] HOST\n");
    exit (EXIT_FAILURE);
  }

  password = argv[1];

  printf ("starting ssh command ...\n");

  h = mexp_spawnv ("ssh", &argv[2]);
  if (h == NULL) {
    perror ("mexp_spawnv: ssh");
    exit (EXIT_FAILURE);
  }

  /* Wait for the password prompt. */
  password_re = compile_re ("assword");
  switch (mexp_expect (h, password_re, NULL, 0, ovector, ovecsize, &pcre_err)) {
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reaching the password prompt\n");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", pcre_err);
    goto error;
  case MEXP_MATCHED:
    break;
  }

  /* Got the password prompt, so send a password. */
  printf ("sending the password ...\n");

  if (mexp_printf (h, "%s\n", password) == -1) {
    perror ("mexp_printf");
    goto error;
  }

  /* We have to wait for the prompt before we can send commands
   * (because the ssh connection may not be fully established).  If we
   * get "password" again, then probably the password was wrong.  This
   * expect checks all these possibilities.  Unfortunately since all
   * prompts are a little bit different, we have to guess here.
   */
  prompt_re = compile_re ("(assword)|([#$])");
  switch (mexp_expect (h, prompt_re, NULL, 0, ovector, ovecsize, &pcre_err)) {
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reaching the prompt\n");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", pcre_err);
    goto error;
  case MEXP_MATCHED:
    /* Which part of the regexp matched? */
    if (ovector[2] >= 0) {      /* password */
      fprintf (stderr, "error: ssh asked for password again, probably the password supplied is wrong\n");
      goto error;
    }
    else if (ovector[4] >= 0) { /* prompt */
      break;
    }
    else
      abort ();                 /* shouldn't happen */
  }

  /* Send a command which will have expected output. */
  printf ("sending a test command ...\n");

  if (mexp_printf (h, "echo h''ello\n") == -1) {
    perror ("mexp_printf");
    goto error;
  }

  /* Wait for expected output from echo hello command. */
  hello_re = compile_re ("hello");
  switch (mexp_expect (h, hello_re, NULL, 0, ovector, ovecsize, &pcre_err)) {
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reading command output\n");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", pcre_err);
    goto error;
  case MEXP_MATCHED:
    break;
  }

  /* Send exit command and wait for ssh to exit. */
  printf ("sending the exit command ...\n");

  if (mexp_printf (h, "exit\n") == -1) {
    perror ("mexp_printf");
    goto error;
  }

  switch (mexp_expect (h, NULL, NULL, 0, NULL, 0, NULL)) {
  case MEXP_EOF:
    /* This is what we're expecting: ssh will close the connection. */
    break;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before ssh closed the connection\n");
    goto error;
  case MEXP_PCRE_ERROR:
  case MEXP_MATCHED:
    fprintf (stderr, "error: unexpected return value from mexp_expect\n");
    goto error;
  }

  /* Close the ssh connection. */
  status = mexp_close (h);
  if (status != 0) {
    fprintf (stderr, "error: bad exit status from ssh subprocess (status=%d)\n",
             status);
    exit (EXIT_FAILURE);
  }

  printf ("test was successful\n");

  exit (EXIT_SUCCESS);

 error:
  mexp_close (h);
  exit (EXIT_FAILURE);
}
