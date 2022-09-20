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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "miniexpect.h"

static void debug_buffer (FILE *, const char *);

static mexp_h *
create_handle (void)
{
  mexp_h *h = malloc (sizeof *h);
  if (h == NULL)
    return NULL;

  /* Initialize the fields to default values. */
  h->fd = -1;
  h->pid = 0;
  h->timeout = 60000;
  h->read_size = 1024;
  h->pcre_error = 0;
  h->buffer = NULL;
  h->len = h->alloc = 0;
  h->next_match = -1;
  h->debug_fp = NULL;
  h->user1 = h->user2 = h->user3 = NULL;

  return h;
}

static void
clear_buffer (mexp_h *h)
{
  free (h->buffer);
  h->buffer = NULL;
  h->alloc = h->len = 0;
  h->next_match = -1;
}

int
mexp_close (mexp_h *h)
{
  int status = 0;

  free (h->buffer);

  if (h->fd >= 0)
    close (h->fd);
  if (h->pid > 0) {
    if (waitpid (h->pid, &status, 0) == -1)
      return -1;
  }

  free (h);

  return status;
}

mexp_h *
mexp_spawnlf (unsigned flags, const char *file, const char *arg, ...)
{
  char **argv, **new_argv;
  size_t i;
  va_list args;
  mexp_h *h;

  argv = malloc (sizeof (char *));
  if (argv == NULL)
    return NULL;
  argv[0] = (char *) arg;

  va_start (args, arg);
  for (i = 1; arg != NULL; ++i) {
    arg = va_arg (args, const char *);
    new_argv = realloc (argv, sizeof (char *) * (i+1));
    if (new_argv == NULL) {
      free (argv);
      va_end (args);
      return NULL;
    }
    argv = new_argv;
    argv[i] = (char *) arg;
  }

  h = mexp_spawnvf (flags, file, argv);
  free (argv);
  va_end (args);
  return h;
}

mexp_h *
mexp_spawnvf (unsigned flags, const char *file, char **argv)
{
  mexp_h *h = NULL;
  int fd = -1;
  int err;
  char slave[1024];
  pid_t pid = 0;

  fd = posix_openpt (O_RDWR|O_NOCTTY);
  if (fd == -1)
    goto error;

  if (grantpt (fd) == -1)
    goto error;

  if (unlockpt (fd) == -1)
    goto error;

  /* Get the slave pty name now, but don't open it in the parent. */
  if (ptsname_r (fd, slave, sizeof slave) != 0)
    goto error;

  /* Create the handle last before we fork. */
  h = create_handle ();
  if (h == NULL)
    goto error;

  pid = fork ();
  if (pid == -1)
    goto error;

  if (pid == 0) {               /* Child. */
    int slave_fd;

    if (!(flags & MEXP_SPAWN_KEEP_SIGNALS)) {
      struct sigaction sa;
      int i;

      /* Remove all signal handlers.  See the justification here:
       * https://www.redhat.com/archives/libvir-list/2008-August/msg00303.html
       * We don't mask signal handlers yet, so this isn't completely
       * race-free, but better than not doing it at all.
       */
      memset (&sa, 0, sizeof sa);
      sa.sa_handler = SIG_DFL;
      sa.sa_flags = 0;
      sigemptyset (&sa.sa_mask);
      for (i = 1; i < NSIG; ++i)
        sigaction (i, &sa, NULL);
    }

    setsid ();

    /* Open the slave side of the pty.  We must do this in the child
     * after setsid so it becomes our controlling tty.
     */
    slave_fd = open (slave, O_RDWR);
    if (slave_fd == -1)
      goto error;

    if (!(flags & MEXP_SPAWN_COOKED_MODE)) {
      struct termios termios;

      /* Set raw mode. */
      tcgetattr (slave_fd, &termios);
      cfmakeraw (&termios);
      tcsetattr (slave_fd, TCSANOW, &termios);
    }

    /* Set up stdin, stdout, stderr to point to the pty. */
    dup2 (slave_fd, 0);
    dup2 (slave_fd, 1);
    dup2 (slave_fd, 2);
    close (slave_fd);

    /* Close the master side of the pty - do this late to avoid a
     * kernel bug, see sshpass source code.
     */
    close (fd);

    if (!(flags & MEXP_SPAWN_KEEP_FDS)) {
      int i, max_fd;

      /* Close all other file descriptors.  This ensures that we don't
       * hold open (eg) pipes from the parent process.
       */
      max_fd = sysconf (_SC_OPEN_MAX);
      if (max_fd == -1)
        max_fd = 1024;
      if (max_fd > 65536)
        max_fd = 65536;      /* bound the amount of work we do here */
      for (i = 3; i < max_fd; ++i)
        close (i);
    }

    /* Run the subprocess. */
    execvp (file, argv);
    perror (file);
    _exit (EXIT_FAILURE);
  }

  /* Parent. */

  h->fd = fd;
  h->pid = pid;
  return h;

 error:
  err = errno;
  if (fd >= 0)
    close (fd);
  if (pid > 0)
    waitpid (pid, NULL, 0);
  if (h != NULL)
    mexp_close (h);
  errno = err;
  return NULL;
}

enum mexp_status
mexp_expect (mexp_h *h, const mexp_regexp *regexps,
             pcre2_match_data *match_data)
{
  time_t start_t, now_t;
  int timeout;
  struct pollfd pfds[1];
  int r;
  ssize_t rs;

  time (&start_t);

  if (h->next_match == -1) {
    /* Fully clear the buffer, then read. */
    clear_buffer (h);
  } else {
    /* See the comment in the manual about h->next_match.  We have
     * some data remaining in the buffer, so begin by matching that.
     */
    memmove (&h->buffer[0], &h->buffer[h->next_match], h->len - h->next_match);
    h->len -= h->next_match;
    h->buffer[h->len] = '\0';
    h->next_match = -1;
    goto try_match;
  }

  for (;;) {
    /* If we've got a timeout then work out how many seconds are left.
     * Timeout == 0 is not particularly well-defined, but it probably
     * means "return immediately if there's no data to be read".
     */
    if (h->timeout >= 0) {
      time (&now_t);
      timeout = h->timeout - ((now_t - start_t) * 1000);
      if (timeout < 0)
        timeout = 0;
    }
    else
      timeout = 0;

    pfds[0].fd = h->fd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    r = poll (pfds, 1, timeout);
    if (h->debug_fp)
      fprintf (h->debug_fp, "DEBUG: poll returned %d\n", r);
    if (r == -1)
      return MEXP_ERROR;

    if (r == 0)
      return MEXP_TIMEOUT;

    /* Otherwise we expect there is something to read from the file
     * descriptor.
     */
    if (h->alloc - h->len <= h->read_size) {
      char *new_buffer;
      /* +1 here allows us to store \0 after the data read */
      new_buffer = realloc (h->buffer, h->alloc + h->read_size + 1);
      if (new_buffer == NULL)
        return MEXP_ERROR;
      h->buffer = new_buffer;
      h->alloc += h->read_size;
    }
    rs = read (h->fd, h->buffer + h->len, h->read_size);
    if (h->debug_fp)
      fprintf (h->debug_fp, "DEBUG: read returned %zd\n", rs);
    if (rs == -1) {
      /* Annoyingly on Linux (I'm fairly sure this is a bug) if the
       * writer closes the connection, the entire pty is destroyed,
       * and read returns -1 / EIO.  Handle that special case here.
       */
      if (errno == EIO)
        return MEXP_EOF;
      return MEXP_ERROR;
    }
    if (rs == 0)
      return MEXP_EOF;

    /* We read something. */
    h->len += rs;
    h->buffer[h->len] = '\0';
    if (h->debug_fp) {
      fprintf (h->debug_fp, "DEBUG: read %zd bytes from pty\n", rs);
      fprintf (h->debug_fp, "DEBUG: buffer content: ");
      debug_buffer (h->debug_fp, h->buffer);
      fprintf (h->debug_fp, "\n");
    }

  try_match:
    /* See if there is a full or partial match against any regexp. */
    if (regexps) {
      size_t i;
      int can_clear_buffer = 1;

      assert (h->buffer != NULL);

      for (i = 0; regexps[i].r > 0; ++i) {
        const int options = regexps[i].options | PCRE2_PARTIAL_SOFT;

        r = pcre2_match (regexps[i].re,
                         (PCRE2_SPTR) h->buffer, (int)h->len, 0,
                         options, match_data, NULL);
        h->pcre_error = r;

        if (r >= 0) {
          /* A full match. */
          const PCRE2_SIZE *ovector = NULL;

          if (match_data)
            ovector = pcre2_get_ovector_pointer (match_data);

          if (ovector != NULL && ovector[1] != ~(PCRE2_SIZE)0)
            h->next_match = ovector[1];
          else
            h->next_match = -1;
          if (h->debug_fp)
            fprintf (h->debug_fp, "DEBUG: next_match at buffer offset %zu\n",
                     h->next_match);
          return regexps[i].r;
        }

        else if (r == PCRE2_ERROR_NOMATCH) {
          /* No match at all. */
          /* (nothing here) */
        }

        else if (r == PCRE2_ERROR_PARTIAL) {
          /* Partial match.  Keep the buffer and keep reading. */
          can_clear_buffer = 0;
        }

        else {
          /* An actual PCRE error. */
          return MEXP_PCRE_ERROR;
        }
      }

      /* If none of the regular expressions matched (not partially)
       * then we can clear the buffer.  This is an optimization.
       */
      if (can_clear_buffer)
        clear_buffer (h);

    } /* if (regexps) */
  }
}

static int mexp_vprintf (mexp_h *h, int password, const char *fs, va_list args)
  __attribute__((format(printf,3,0)));

static int
mexp_vprintf (mexp_h *h, int password, const char *fs, va_list args)
{
  char *msg;
  int len;
  size_t n;
  ssize_t r;
  char *p;

  len = vasprintf (&msg, fs, args);

  if (len < 0)
    return -1;

  if (h->debug_fp) {
    if (!password) {
      fprintf (h->debug_fp, "DEBUG: writing: ");
      debug_buffer (h->debug_fp, msg);
      fprintf (h->debug_fp, "\n");
    }
    else
      fprintf (h->debug_fp, "DEBUG: writing the password\n");
  }

  n = len;
  p = msg;
  while (n > 0) {
    r = write (h->fd, p, n);
    if (r == -1) {
      free (msg);
      return -1;
    }
    n -= r;
    p += r;
  }

  free (msg);
  return len;
}

int
mexp_printf (mexp_h *h, const char *fs, ...)
{
  int r;
  va_list args;

  va_start (args, fs);
  r = mexp_vprintf (h, 0, fs, args);
  va_end (args);
  return r;
}

int
mexp_printf_password (mexp_h *h, const char *fs, ...)
{
  int r;
  va_list args;

  va_start (args, fs);
  r = mexp_vprintf (h, 1, fs, args);
  va_end (args);
  return r;
}

int
mexp_send_interrupt (mexp_h *h)
{
  return write (h->fd, "\003", 1);
}

/* Print escaped buffer to fp. */
static void
debug_buffer (FILE *fp, const char *buf)
{
  while (*buf) {
    if (isprint (*buf))
      fputc (*buf, fp);
    else {
      switch (*buf) {
      case '\0': fputs ("\\0", fp); break;
      case '\a': fputs ("\\a", fp); break;
      case '\b': fputs ("\\b", fp); break;
      case '\f': fputs ("\\f", fp); break;
      case '\n': fputs ("\\n", fp); break;
      case '\r': fputs ("\\r", fp); break;
      case '\t': fputs ("\\t", fp); break;
      case '\v': fputs ("\\v", fp); break;
      default:
        fprintf (fp, "\\x%x", (unsigned char) *buf);
      }
    }
    buf++;
  }
}
