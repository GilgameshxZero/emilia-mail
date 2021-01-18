# Changelog

## 2.2.7

* Additional typo fixes in `makefile`.

## 2.2.6

* Fix `WEB` -> `MAIL` typo in makefile.

## 2.2.5

* Revamp `makefile` to pass correct linker and compiler flags.
* Setup Visual Studio Code debugging workflows.

## 2.2.4

* Use new `send` functions introduced in `rain 6.4.2`.
* Serve on `localhost` again with the new `Node` implementation.

## 2.2.3

* Small bugfix to use `*` binding address rather than `localhost`.

## 2.2.2

* Fix `make noinc`.
* Don't sort `using` in `.clang-format`.
* Use `rain v6.4.0`.

## 2.2.1

* Return `450` temporary failure codes instead of permanent `554`s.
* Saves failed emails to a file on disk.
* Update `noinc` make command.

## 2.2.0

* Update to yet another RAII implementation of the underlying `Socket`, and rework inheritance model to match.
* Less verbose logging—focusing mainly on failed email relays.
* Fix `MacOS` builds.

## 2.1.3

* Update to the RAII implementation of `Rain::Socket`.
* Use WSL instead of Cygwin for Windows command line builds.
* Add `make run` and `make no-inc` build commands.

## 2.1.2

* Ignore case in email/domain matching.

## 2.1.1

* Shorten default timeouts.

## 2.1.0

* Finish implementing very basic "relay" SMTP server.

## 2.0.6

* Makefile now recompiles when any of the `rain` header changes.
* Add command-line parsing.

## 2.0.5

* Add `-lstdc++fs` flag to compilation options.
* Update `.clang-format`.

## 2.0.4

Carry over changes to makefile from `emilia-web`.

## 2.0.3

Change makefile to recompile when `rain` headers change.

## 2.0.2

Use header-only `rain`, and property pages provided by `rain`.

## 2.0.1

Update `.clang-format` with line limit and add a flag in the makefile for `struct addrinfo`.

## 2.0.0

Separate from `emilia`, continuing from `EmiliaSMTP` project.
