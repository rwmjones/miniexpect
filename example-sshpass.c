/* miniexpect example.
 * Copyright (C) 2014-2022 Red Hat Inc.
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
 *   sshpass [-d] 123456 ssh remote.example.com
 *   sshpass [-d] 123456 ssh -l root remote.example.com
 *
 * Use the -d flag to enable debugging to stderr.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "miniexpect.h"

static pcre2_code *compile_re (const char *rex);

static void
usage (void)
{
  fprintf (stderr, "usage: sshpass [-d] PASSWORD ssh [SSH-ARGS...] HOST\n");
  exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  int opt;
  int debug = 0;
  mexp_h *h;
  const char *password;
  int status;
  pcre2_code *password_re, *prompt_re, *hello_re;
  pcre2_match_data *match_data;

  match_data = pcre2_match_data_create (4, NULL);

  while ((opt = getopt (argc, argv, "d")) != -1) {
    switch (opt) {
    case 'd':
      debug = 1;
      break;
    default:
      usage ();
    }
  }

  if (argc-optind <= 2)
    usage ();

  password = argv[optind];
  optind++;

  printf ("starting ssh command ...\n");

  h = mexp_spawnv (argv[optind], &argv[optind]);
  if (h == NULL) {
    perror ("mexp_spawnv: ssh");
    exit (EXIT_FAILURE);
  }
  if (debug)
    mexp_set_debug_file (h, stderr);

  /* Wait for the password prompt. */
  password_re = compile_re ("assword");
  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, .re = password_re },
                         { 0 }
                       },
                       match_data)) {
  case 100:
    break;
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reaching the password prompt\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", mexp_get_pcre_error (h));
    goto error;
  }

  /* Got the password prompt, so send a password.
   *
   * Note use of mexp_printf_password here which is identical to
   * mexp_printf except that it hides the password in debugging
   * output.
   */
  printf ("sending the password ...\n");

  if (mexp_printf_password (h, "%s", password) == -1 ||
      mexp_printf (h, "\n") == -1) {
    perror ("mexp_printf");
    goto error;
  }

  /* We have to wait for the prompt before we can send commands
   * (because the ssh connection may not be fully established).  If we
   * get "password" again, then probably the password was wrong.  This
   * expect checks all these possibilities.  Unfortunately since all
   * prompts are a little bit different, we have to guess here.
   */
  prompt_re = compile_re ("[#$]");
  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, .re = password_re },
                         { 101, .re = prompt_re },
                         { 0 },
                       },
                       match_data)) {
  case 100:                     /* Password. */
    fprintf (stderr, "error: ssh asked for password again, probably the password supplied is wrong\n");
    goto error;
  case 101:                     /* Prompt. */
    break;
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reaching the prompt\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", mexp_get_pcre_error (h));
    goto error;
  }

  /* Send a command which will have expected output. */
  printf ("sending a test command ...\n");

  if (mexp_printf (h, "echo h''ello\n") == -1) {
    perror ("mexp_printf");
    goto error;
  }

  /* Wait for expected output from echo hello command. */
  hello_re = compile_re ("hello");
  switch (mexp_expect (h,
                       (mexp_regexp[]) {
                         { 100, .re = hello_re },
                         { 0 },
                       },
                       match_data)) {
  case 100:
    break;
  case MEXP_EOF:
    fprintf (stderr, "error: ssh closed the connection unexpectedly\n");
    goto error;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before reading command output\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_PCRE_ERROR:
    fprintf (stderr, "error: PCRE error: %d\n", mexp_get_pcre_error (h));
    goto error;
  }

  /* Send exit command and wait for ssh to exit. */
  printf ("sending the exit command ...\n");

  if (mexp_printf (h, "exit\n") == -1) {
    perror ("mexp_printf");
    goto error;
  }

  switch (mexp_expect (h, NULL, NULL)) {
  case MEXP_EOF:
    /* This is what we're expecting: ssh will close the connection. */
    break;
  case MEXP_TIMEOUT:
    fprintf (stderr, "error: timeout before ssh closed the connection\n");
    goto error;
  case MEXP_ERROR:
    perror ("mexp_expect");
    goto error;
  case MEXP_PCRE_ERROR:
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

  pcre2_match_data_free (match_data);
  exit (EXIT_SUCCESS);

 error:
  mexp_close (h);
  exit (EXIT_FAILURE);
}

/* Helper function to compile a PCRE regexp. */
static pcre2_code *
compile_re (const char *rex)
{
  int errorcode;
  PCRE2_SIZE erroroffset;
  char errormsg[256];
  pcre2_code *ret;

  ret = pcre2_compile ((PCRE2_SPTR) rex, PCRE2_ZERO_TERMINATED,
                       0, &errorcode, &erroroffset, NULL);
  if (ret == NULL) {
    pcre2_get_error_message (errorcode,
                             (PCRE2_UCHAR *) errormsg, sizeof errormsg);
    fprintf (stderr, "error: "
             "failed to compile regular expression '%s': "
             "%s at offset %zu\n",
             rex, errormsg, erroroffset);
    exit (EXIT_FAILURE);
  }
  return ret;
}
