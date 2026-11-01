# Release notes

## v1.1.0 (change request CR1)
**New**
* Lecturers can grant one new deadline to a whole class (lecturer menu option 8, "Grant extension to a whole class"). Students who already have the same or a later deadline are left unchanged (never shortened). The action is all-or-nothing.
* Extensions are now shown on the student's assignment list: the column "Your deadline" shows the student's own deadline and marks extended ones with `*`.

**Changed**
* In the lecturer menu, "Change my password" moved from option 8 to option 9. Options 1-7 keep their numbers.

**Unchanged / compatibility**
* No database change: class extensions are stored as one extension record per student, so v1.0 databases work with v1.1 without migration. The late-penalty, resubmission, moderation and release rules are unchanged.

**Tests:** 85 automated tests (75 from v1.0, unchanged and all passing, plus 10 for CR1).

## v1.0.0
First release. Role-based console system for administrators, lecturers, Heads of Department and students: assignments with deadlines, individual extensions, submissions and resubmission before the deadline, late penalty (10% of the available mark per day for up to 3 days, then zero), marking with written feedback, Head of Department moderation (approve, return, adjust) and release, audit log of every mark change, salted Argon2id password hashes, SQLite storage, search and filter.

**Known limitations**
* Console interface only; no web or mobile interface (out of scope).
* Late "day" = started 24-hour period after the (extended) deadline; work more than 3 days late is stored and receives zero.
* Concurrent use relies on SQLite file locking; suited to one school computer or a small local network.
