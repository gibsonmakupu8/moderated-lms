# Moderated Learning Management System (CS4433 final project)

C++17 console application with SQLite persistence for a secondary school: administrators manage accounts and courses; lecturers create assignments, grant extensions and mark work; Heads of Department moderate and release marks; students submit work and see only released marks.

* Build and run: see `docs/BUILD_GUIDE.md`
* Coding standard: `docs/CODING_STANDARD.md`
* Design and traceability: `docs/TRACEABILITY.md`
* Layers: `include/lms` + `src` (domain, services, repositories, UI), `tests` (GoogleTest), `sql/schema.sql`
