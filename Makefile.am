# miniexpect
# Copyright (C) 2014 Red Hat Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = miniexpect.3

# The library.

lib_LTLIBRARIES = libminiexpect.la

libminiexpect_la_SOURCES = miniexpect.c miniexpect.h
libminiexpect_la_CFLAGS = $(PCRE2_CFLAGS) -Wall
libminiexpect_la_LIBADD = $(PCRE2_LIBS)
libminiexpect_la_LDFLAGS = -version-info 0:0:0

# Examples.

noinst_PROGRAMS = example-sshpass

example_sshpass_SOURCES = example-sshpass.c
example_sshpass_CFLAGS = $(PCRE2_CFLAGS) -Wall
example_sshpass_LDADD = libminiexpect.la

# Tests.

TESTS = $(check_PROGRAMS)
check_PROGRAMS = \
	test-spawn \
	test-ls-version \
	test-multi-match

test_spawn_SOURCES = test-spawn.c tests.h miniexpect.h
test_spawn_CFLAGS = $(PCRE2_CFLAGS) -Wall
test_spawn_LDADD = libminiexpect.la

test_ls_version_SOURCES = test-ls-version.c tests.h miniexpect.h
test_ls_version_CFLAGS = $(PCRE2_CFLAGS) -Wall
test_ls_version_LDADD = libminiexpect.la

test_multi_match_SOURCES = test-multi-match.c tests.h miniexpect.h
test_multi_match_CFLAGS = $(PCRE2_CFLAGS) -Wall
test_multi_match_LDADD = libminiexpect.la

# parallel-tests breaks the ability to put 'valgrind' into
# TESTS_ENVIRONMENT.  Hence we have to work around it:
check-valgrind:
	for t in $(TESTS); do \
	  $(LIBTOOL) --mode=execute $(VG) ./$$t; \
	  r=$$?; \
	  if [ $$r -ne 0 ]; then exit $$r; fi; \
	done

# Clean.

CLEANFILES = *~

# Man pages.

man_MANS = miniexpect.3

if HAVE_POD2MAN

CLEANFILES += miniexpect.3

miniexpect.3: miniexpect.pod
	$(POD2MAN) \
	  -u \
	  --section 3 \
	  --release "$(PACKAGE)-$(VERSION)" \
	  -c "Library functions" \
	  $< > $@-t
	mv $@-t $@

endif
