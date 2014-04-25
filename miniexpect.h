/* miniexpect
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

#ifndef MINIEXPECT_H_
#define MINIEXPECT_H_

#include <unistd.h>

#include <pcre.h>

/* This handle is created per subprocess that is spawned. */
struct mexp_h {
  int fd;                       /* File descriptor pointing to pty. */
  pid_t pid;                    /* Subprocess PID. */

  /* Timeout (milliseconds, 1/1000th seconds).  The caller may set
   * this before calling mexp_expect.  Set it to -1 to mean no
   * timeout.  The default is 60000 (= 60 seconds).
   */
  int timeout;

  /* The read buffer is allocated by the library when mexp_expect is
   * called.  It is available so you can examine the buffer to see
   * what part of the regexp matched.  Note this buffer does not
   * contain the full input from the process, but it will contain at
   * least the part matched by the regular expression (and maybe some
   * more).
   */
  char *buffer;                 /* Read buffer. */
  size_t len;                   /* Length of data in the buffer. */
  size_t alloc;                 /* Allocated size of the buffer. */

  /* The caller may set this to set the size (in bytes) for reads from
   * the subprocess.  The default is 1024.
   */
  size_t read_size;

  /* If mexp_expect returns MEXP_PCRE_ERROR, then the actual PCRE
   * error code is returned here.  See pcre_exec(3) for details.
   */
  int pcre_error;

  /* Opaque pointers for use of the caller.  The library will not
   * touch these.
   */
  void *user1;
  void *user2;
  void *user3;
};
typedef struct mexp_h mexp_h;

/* Spawn a subprocess.
 *
 * If successful it returns a handle.  If it fails, it returns NULL
 * and sets errno.
 */
extern mexp_h *mexp_spawnv (const char *file, char **argv);

/* Same as mexp_spawnv, but it uses a NULL-terminated variable length
 * list of arguments.
 */
extern mexp_h *mexp_spawnl (const char *file, const char *arg, ...);

/* Close the handle and clean up the subprocess.
 *
 * This returns:
 *   0:   successful close, subprocess exited cleanly.
 *   -1:  error in system call, see errno.
 *   > 0: exit status of subprocess if it didn't exit cleanly.  Use
 *        WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG etc macros to
 *        examine this.
 *
 * Notes:
 *
 * - Even in the error cases, the handle is always closed and
 *   freed by this call.
 *
 * - It is normal for the kernel to send SIGHUP to the subprocess.
 *   If the subprocess doesn't catch the SIGHUP, then it will die
 *   (WIFSIGNALED (status) && WTERMSIG (status) == SIGHUP).  This
 *   case should not necessarily be considered an error.
 */
extern int mexp_close (mexp_h *h);

/* The list of regular expressions passed to mexp_expect. */
struct mexp_regexp {
  int r;                        /* The returned value from mexp_expect
                                 * if this regular expression matches.
                                 * MUST be > 0.
                                 */
  const pcre *re;               /* The compiled regular expression. */
  const pcre_extra *extra;      /* See pcre_exec. */
  int options;                  /* See pcre_exec. */
};
typedef struct mexp_regexp mexp_regexp;

enum mexp_status {
  MEXP_EOF        = 0,
  MEXP_TIMEOUT    = -1,
  MEXP_ERROR      = -2,
  MEXP_PCRE_ERROR = -3,
};

/* Expect some output from the subprocess.  Match the output against
 * the PCRE regular expression(s) in the list, and return which one
 * matched.
 *
 * See example-sshpass.c for an example of how to pass in regular
 * expressions.
 *
 * This can return:
 *
 *   MEXP_TIMEOUT:
 *     No input matched before the timeout (mexp_h->timeout) was reached.
 *   MEXP_EOF:
 *     The subprocess closed the connection.
 *   MEXP_ERROR:
 *     There was a system call error (eg. from the read call).  See errno.
 *   MEXP_PCRE_ERROR
 *     There was a pcre_exec error.  h->pcre_error is set to the error code
 *     (see pcreapi(3) for a list of PCRE_* error codes and what they mean).
 *
 * Notes:
 *
 * - 'regexps' may be NULL or an empty list, which means we don't
 *   match against a regular expression.  This is useful if you just
 *   want to wait for EOF or timeout.
 *
 * - 'regexps[].re', 'regexps[].extra', 'regexps[].options', 'ovector'
 *   and 'ovecsize' are passed through to the pcre_exec function.
 */
extern int mexp_expect (mexp_h *h, const mexp_regexp *regexps,
                        int *ovector, int ovecsize);

/* This is a convenience function for writing something (eg. a
 * password or command) to the subprocess.  You could do this by
 * writing directly to 'h->fd', but this function does all the error
 * checking for you.
 *
 * Returns the number of bytes if the whole message was written OK
 * (partial writes are not possible with this function), or -1 if
 * there was an error (check errno).
 */
extern int mexp_printf (mexp_h *h, const char *fs, ...)
  __attribute__((format(printf,2,3)));

#endif /* MINIEXPECT_H_ */
