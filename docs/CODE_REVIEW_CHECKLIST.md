# Self code-review checklist
1. Does the change follow the layering (UI -> service -> repository interface -> SQLite)?
2. Is every SQL value a bound parameter? Is there any SQL outside the repositories?
3. Does every service method check permission, then ownership/scope, then validate input?
4. Are multi-record changes inside a transaction? Is every mark change audited with a reason?
5. Can any user input reach `std::stoi`/`stoll`/`at()` or similar without validation?
6. Are all error paths covered by an exception type with a user-friendly message?
7. Are passwords/hashes kept out of logs, screens and exceptions?
8. Is there a unit test for each rule and a system test for each use-case flow, including exceptions?
9. Compiler warnings (-Wall -Wextra -Wpedantic -Wshadow -Wconversion) and cppcheck clean?
10. Are names, comments and documentation consistent with the design documents?
