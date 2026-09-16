# third_party

Vendored third-party libraries, each in its own directory with its licence file and a `REVISION.md` naming the upstream, the exact tag or commit, the retrieval date and the files taken. Files here are unmodified copies; a local change is a fork and belongs elsewhere, with a note in `REVISION.md`.

What belongs here: header-only or single-file libraries that the engine-independent packages compile against (test framework, JSON, JSON Schema, ZIP). What does not: engine plugins, generated code, anything with a runtime dependency on the engine, and anything whose licence is not permissive.

To update a library: replace its directory wholesale from the new upstream revision, update `REVISION.md`, and run every package's tests.
