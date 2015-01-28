# Simple Hierarchical Notebook [![Build Status](https://travis-ci.org/drbig/snb.svg?branch=master)](https://travis-ci.org/drbig/snb)

![SNB in action](https://raw.github.com/drbig/snb/master/snb.png)

Minimal nested list browser and editor with locale support and ncurses interface.

Features / Bugs:

- File format is Markdown compatible (GFM to be exact)
- Works with multi-byte encodings (including Unicode)
- Editing wide-char (e.g. Japanese) languages doesn't work yet, but browsing should
- Keyboard driven 'content oriented' UI
- The produced binary is all that is needed
- Ncursesw is the only runtime dependency
- Provides a rudimentary undo function
- You can both cross-out and highlight entries
- Configuration by editing an include file
- Column mode, background color, highlight attributes and locale can be configured and/or overridden on command line
- Customizable key bindings

*STATUS:* **Stable** - should build cleanly on: Linux, FreeBSD, OS X.

There is a [AUR package](https://aur.archlinux.org/packages/snb-git/) for snb - thanks to Celti!

## Setup

**NOTE**: If you're not using a `UTF-8` locale you should edit `src/user.h` before compiling, as the default bullet strings use Unicode glyphs.

**Debian and derivative users**: You'll probably need *all* of these packages installed: ncurses5, ncurses5-dev, ncursesw5, and ncursesw5-dev. Please ensure you have them before you run `make` (relevant [issue](https://github.com/drbig/snb/issues/2)).

**FreeBSD users**: You'll need [devel/ncurses](http://www.freshports.org/devel/ncurses). Also, `make debug` doesn't work with BSD make, you can do a debug build with `CFLAGS="-DDEBUG -g" make`. If you happen to have newer ncurses you might need to add `NCURS_CONF=ncursesw6-config` (i.e. adjust the number to what you actually have).

**OS X users**: You'll need a newer version of ncurses. If you use [Homebrew](http://brew.sh), `brew install homebrew/dupes/ncurses` and pass the path to `make` like so: `make NCURS_CONF=/usr/local/opt/ncurses/bin/ncursesw5-config`

The Makefile is now a bit smarter. If you can successfully run `ncursesw5-config` then any compile problems are probably due to something else (please file an issue via GitHub in such case).

    $ git clone https://github.com/drbig/snb.git
    $ cd snb
    $ make debug

## Usage and configuration

For starters you should just try:

    $ ./bin/snb help.md

This should start snb and present you with an introductory list to help you figure out the key bindings.

In the current state I recommend running the debug build. Once the binary is compiled it has no dependencies, so feel free to put wherever you want (e.g. `~/bin`).

There are now some command line options available:

    $ ./bin/snb -h
      Usage: ./bin/snb [options...] (path)
    
    Options:
            -h        - print this message and exit
            -v        - print version and exit
            -l LOCALE - force locale
            -w WIDTH  - set fixed-column mode (0 - off, default: 80)
            -b        - use term bg color or black (default: term)

The distributed `src/user.h` assumes you're using a `UTF-8` locale and have everything setup properly. The `-l` option is a simple feature to override your defined locale, which might help if your locale is e.g. `en_US` but you still happen to have everything setup properly so that using `en_US.UTF-8` will work. It will tell you if the call to `setlocale()` failed.

The fixed-column mode refers to a feature usually found only in author-oriented software like [Scrivener](http://www.literatureandlatte.com/scrivener.php) or [WordGrinder](http://wordgrinder.sourceforge.net/), where the text you're working on is displayed in a centred fixed-width 'window'. With the `-w` option you can override this, regardless of `SCR_WIDTH` definition in `src/user.h`.

You can configure the UI appearance by editing `src/user.h` and perhaps `src/colors.c`. You can also set a default file that snb will try to load if no arguments were supplied, however remember the path should be absolute.

## Contributing

Feel free to hack the code to your liking.

If you wish to contribute back please:

1. Clone the repository
2. Make your changes on a separate branch
3. Make sure you have run `make full-check` before committing and there were no errors reported (you'll need a bunch of additional tools for that, listed below)
4. Make a pull request

In other words follow the usual GitHub development workflow.

The tools needed for `make full-check`:

1. [Artistic Style](http://astyle.sourceforge.net/) - so there is no need to discuss tastes
2. [Check](http://check.sourceforge.net/) - for automated testing
3. [Clang Analyzer](http://clang-analyzer.llvm.org/) - for static code analysis

I would also recommend the following tools:

1. [Valgrind](http://valgrind.org/) - for memory leak analysis, and much more
2. [GDB](http://www.gnu.org/software/gdb/) - yes, it really is as useful as they say
3. [Doxygen](http://www.stack.nl/~dimitri/doxygen/) - for pretty HTML docs

Personally I know I won't start any new C project without the above.

### Help needed here

I want to keep the whole thing simple therefore: no different IO formats, no config files to parse, no spell-check, no printing, no scripting, etc. Still there are many ways to improve upon this tool.

In order of perceived importance:

- ~~Separating key-bound actions in a way that makes it easy for a user to override the default bindings.~~ This should also enable a smooth transition to ESC-sequence parsing, so that we can do advanced stuff such as interpreting a shifted arrow key! (Ncurses is somewhat weird)
- Better terminal resize handling. Right now it's mostly a sham, as it works only in browsing mode
- Tab-completion for open and save as dialogs
- Code and docs clean up (I'll probably do it)
- Some abstractions could most probably be done better (i.e. `vitree` and `elmopen` stuff)

### General notes

This was my first attempt at writing: a. non-trivial Unix program in C; b. locale aware program in C; c. ncurses-based interface. As such the code is rather clunky and rough, and you can follow the journey as I left the git history unaltered (do that on your own risk though).

However in the process I became good friends with [valgrind](http://valgrind.org/), [gdb](http://www.gnu.org/software/gdb/) and [clang analyzer](http://clang-analyzer.llvm.org/). Eventually I got myself a working clone of the venerable [hnb](http://hnb.sourceforge.net/) that can speak languages.

The code has rudimentary comments and if you have [doxygen](http://www.stack.nl/~dimitri/doxygen/) installed `make docs` will work. I've also written a very ugly test suite for the data handling part, and if you have [check](http://check.sourceforge.net/) installed `make check` should also work.

## Licensing

Standard two-clause BSD license, see LICENSE.txt for details.

Any contributions will be licensed under the same conditions.

Copyright (c) 2014 - 2015 Piotr S. Staszewski
