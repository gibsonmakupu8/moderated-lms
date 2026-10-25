# Coding standard
1. C++17, no compiler extensions; the build uses `-Wall -Wextra -Wpedantic` and must be warning free.
2. Files: one class (or tightly related group) per header/source pair; headers use `#pragma once`.
3. Naming: types `PascalCase`; functions and variables `camelCase`; private members end with `_`; constants `kPascalCase`; namespace `lms`.
4. Layering: UI (`App`, `Console`) -> services -> repository interfaces -> SQLite. No SQL outside `SqliteRepositories.cpp` and `Schema.cpp`; no business rules in the UI.
5. Every SQL value is bound with a parameter (`Statement::bind`). SQL text is never built from user input.
6. Ownership by RAII and `std::unique_ptr`; no raw `new`/`delete`; no global mutable state.
7. Errors: throw the `LmsError` family; never return error codes; the UI catches and reports. Never catch and ignore.
8. Services check permission first (`requirePermission`), then ownership/scope, then validate input, then change data inside a transaction.
9. Passwords are only handled by `IPasswordHasher`; never stored, logged or echoed.
10. Every business rule has a unit test; every use-case flow has an integration test including its exception flows.
11. Git: feature branches, pull-request review, meaningful commit messages ("Add ...", "Fix ..."), tagged releases (`v1.0`).
