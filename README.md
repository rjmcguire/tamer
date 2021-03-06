Tamer [![Build Status](https://travis-ci.org/kohler/tamer.svg?branch=master)](https://travis-ci.org/kohler/tamer)
=====

   Tamer is a C++ language extension and library that simplifies
event-driven programming. Functions in Tamer can block to wait for an
event, then resume computation later. Other functions can continue
while a function is blocked. This is normally hard to code without
closures, and even somewhat annoying in languages that have closures;
Tamer makes it relatively easy. Compared with threaded programs, tamed
programs aren’t much harder to understand, but they avoid most
synchronization and locking issues and generally require less memory.

   Tamer contains a preprocessor that translates Tamer code into
conventional C++ code; libraries for event-driven programming; and
several examples, including a working port of the Knot web server
distributed with the Capriccio threading package.

   If you are building from a source repository (git), you will need
to generate configure scripts. (This is not necessary if you
downloaded a tarball.) Make sure you have the necessary development
packages: flex, bison, automake, and libtool. Then run
`./bootstrap.sh` from the top directory.

   Thereafter installation is standard. Run `./configure`, supplying
any options, then `make install`. Documentation is supplied in manual
page format, and as doxygen comments in the user-facing header files
in "tamer/". After `make install`, try `man 3 tamer`. (Before
installation, try `nroff -man doc/tamer.3 | less`.)


Tamer and SFSlite
-----------------

   Tamer is a lightweight version of the Tame event-driven programming
package distributed as part of SFSlite.  For historical reasons, Tame
follows SFS conventions, and requires a wide variety of less than
documented general-purpose classes, including strings, string buffers,
vectors, hash tables, linked lists, and reference counts.  Better, or at
least more standard, versions of many of these classes are available as
part of the C++ standard library and the Boost libraries.  Additionally,
the Tame abstractions are layered on top of SFSlite's existing "wrap"
callbacks, adding complexity and some overhead.

   Tamer is a freestanding port of the basic Tame abstractions to a
standard C++ environment.  Although currently less powerful than Tame and
SFSlite -- for example, Tamer code can block on disk I/O, a problem SFSlite
can solve with RPCs, and SFSlite ships with an asynchronous DNS resolver --
Tamer is much smaller and lighter weight.  It was also designed to be
easier to use.

   If your main concern is code readability, try Tamer.  If you really can
never block, or would benefit from built-in RPC support, try Tame.


License and Non-Warranty
------------------------

   The Tamer libraries and examples are distributed under the BSD license.
See the file "LICENSE", which is described in the source code as "the Tamer
LICENSE file".

   The "tamer" preprocessor, which consists of the source code in the
"compiler" subdirectory, is distributed under the GNU General Public
License, Version 2.  See the file "COPYING" in that directory.  This means
that any modifications to the compiler are subject to the terms of the GPL.
The preprocessor's output is NOT a "derived work", however, so the tamer
preprocessor may be used to compile software using any licensing terms.

   The "knot.tamer" server in the "knot" directory is derived from the
"knot" server, distributed as part of the Capriccio system.  We believe
this server was released under the BSD license.  See "knot/README".


Bugs and Contributing
---------------------

   We are happy to accept bug reports, patches, and contributions of code,
for example to improve Tamer's support for nonblocking disk I/O.  Send them
to Eddie Kohler at the address below.


Authors
-------

* Eddie Kohler <kohler@seas.harvard.edu>  
  http://read.seas.harvard.edu/~kohler/
* Dero Gharibian  
  File open/read/write/fstat helper for asynchronous disk I/O; asynchronous
  DNS resolver.
* Maxwell Krohn  
  http://www.okws.org/  
  Especially for the original version of the tamer preprocessor and
  consultation on the name of the fileio:: namespace.
