# Simple Hierarchical Notebook

![SNB in action](https://raw.github.com/drbig/snb/master/snb.png)

Minimal nested list browser and editor with locale support and ncurses interface.

Features / Bugs:

- File format is Markdown compatible (GFM to be exact)
- Works with multi-byte encodings (including Unicode)
- Editing wide-char (e.g. Japanese) languages doesn't work yet, but browsing should
- Keyboard driven 'content oriented' UI
- Configuration by editing an include file
- The produced binary is all that is needed
- Ncursesw is the only runtime dependency

*STATUS:* **Alpha - looks like it's working.**

## Setup

    $ git clone https://github.com/drbig/snb.git
    $ cd snb
    $ make debug

If the build fails the most probable cause is you don't have ncurses**w**, or there is a configuration problem. Check `Makefile` and adjust paths for includes and/or `ncursesw` library.

If the build is successful:

    $ ./bin/snb help.md

This should start snb and present you with an introductory list to help you figure out the key bindings.

## Usage and configuration

In the current state I recommend running the debug build. Once the binary is compiled it has no dependencies, so feel free to put wherever you want (e.g. `~/bin`). As stated previously, `help.md` will introduce you to current default key bindings and some other information.

The binary accepts a single argument of a file to load on startup.

You can configure the UI appearance by editing `src/user.h` and perhaps `src/colors.c`. You can also set a default file that snb will try to load if no arguments were supplied, however remember the path should be absolute.

## Contributing

Feel free to hack the code to your liking.

If you wish to contribute back please make sure that: a. your changes are on a separate branch; b. you have run `make style` before committing (you'll need to have [astyle](http://astyle.sourceforge.net/) installed); and make a pull request (in other words follow the usual GitHub development model).

This was my first attempt at writing: a. non-trivial Unix program in C; b. locale aware program in C; c. ncurses-based interface. As such the code is rather clunky and rough, and you can follow the journey as I left the git history unaltered (do that on your own risk though).

However in the process I became good friends with [valgrind](http://valgrind.org/) and [gdb](http://www.gnu.org/software/gdb/), and got myself a working clone of the venerable [hnb](http://hnb.sourceforge.net/) that can speak languages.

The code has rudimentary comments and if you have [doxygen](http://www.stack.nl/~dimitri/doxygen/) installed `make docs` will work. I've also written a very ugly test suite for the data handling part, and if you have [check](http://check.sourceforge.net/) installed `make check` should also work.

What is most need, in my humble opinion:

- Configurable key bindings
- Better terminal resize handling
- Tab-completion for open and save as dialogs
- Some abstractions could most probably be done better (i.e. `vitree` and `elmopen` stuff)

## Licensing

Standard two-clause BSD license, see LICENSE.txt for details.

Any contributions will be licensed under the same conditions.

Copyright (c) 2014 Piotr S. Staszewski
