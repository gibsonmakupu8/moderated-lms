# Requirements traceability (implementation view)

| Req | Implemented in (source) | Tables | Tests |
|---|---|---|---|
| FR-01 | AdminService::createUser/updateUser/setActive/resetPassword | users | Admin tests (test_auth_admin.cpp) |
| FR-02 | User, SystemAdministrator, Lecturer, HeadOfDepartment, Student, UserFactory | users.role | Users.FactoryCreatesCorrectSubclass... |
| FR-03 | requirePermission(); scope checks in services | all | OnlyAdministratorsManage..., LecturerOnlySeesAndMarksOwnCourses, HodCanOnlyModerateOwnDepartment |
| FR-04 | AdminService::createDepartment/createCourse/update* | departments, courses | DepartmentAndCourseManagement |
| FR-05 | AdminService::assignLecturer/enrolStudent | course_lecturers, enrolments | LecturerAssignmentAndEnrolmentRules |
| FR-06 | CourseworkService::createAssignment | assignments | LecturerCreatesAssignmentWithValidation |
| FR-07 | CourseworkService::grantExtension | extensions | ExtensionRulesAndEffectiveDeadline |
| FR-07b (CR1) | CourseworkService::grantClassExtension | extensions | CR1_ClassExtension... (7 tests), ST17, ST18 |
| FR-08a (CR1) | AssignmentView::effectiveDeadline/extended; App student list | extensions | CR1_ExtendedDeadlineShownOnStudentAssignmentListOnly, ST17 |
| FR-08 | CourseworkService::assignmentsFor | assignments, enrolments | VisibleCoursesAndAssignmentsFollowTheRole |
| FR-09 | CourseworkService::submit / history | submissions | OnTimeSubmission..., ResubmissionBeforeDeadline..., ResubmissionAfterDeadlineIsRefused |
| FR-10 | CourseworkService::submit (effective deadline) | submissions, extensions | ExtensionMakesLateWorkOnTime |
| FR-11 | DailyTenPercentPenalty (Strategy) | submissions.days_late, marks.penalty_percent | LatePenalty.BoundaryValues, LatePenaltyIsAppliedToFinalMark |
| FR-12 | AssessmentService::recordMark | marks | MarkValidation |
| FR-13 | AssessmentService::submitForModeration | submissions.status | WorkflowOrderIsEnforced |
| FR-14 | AssessmentService::approve/returnForCorrection/adjustMark | marks, submissions | ReturnForCorrectionLoop, HodAdjustmentIsAudited... |
| FR-15 | AssessmentService::releasedResult/mySubmissions | marks, submissions | StudentCannotSeeMarkBeforeRelease |
| FR-16 | AssessmentService::releasedResult | marks | FullLifecycleFromSubmissionToRelease |
| FR-17 | AssessmentService::writeAudit | audit_entries (+ immutability triggers) | AuditEntriesCannotBeModifiedOrDeleted, ReturnForCorrectionLoop |
| FR-18 | SubmissionFilter, assignmentsFor(keyword) | - | SearchAndFilterByStatusAndKeyword |
| NFR-02/03 | Indexes in Schema.cpp | all | SearchesWithin3SecondsFor800Students |
| NFR-04 | SodiumPasswordHasher (Argon2id), AuthService | users.password_hash | SamePasswordGivesDifferentSaltedHashes, LoginFailuresGiveTheSameMessage |
| NFR-05 | Validation.cpp, Database::runInTransaction, CHECK constraints | all | MarkValidation, ConstraintsAreEnforcedByTheDatabase, TransactionRollsBack... |
| NFR-06 | Local SQLite file, no network code; backup in BUILD_GUIDE | - | (inspection) |
| NFR-08 | AuditEntry (actor, time, old/new, reason) | audit_entries | ReturnForCorrectionLoop |
| NFR-10 | Ownership checks, uniform "not available" message | - | StudentsCannotSeeEachOthersResults |


## System and acceptance tests (tests/test_system.cpp, tests/test_cr1.cpp)
ST01-ST06 -> UC-01 (submit: main, late, very late, resubmission, exceptions, extension); ST07-ST09 -> UC-02/UC-03 (mark, moderate, return, release, validation); ST10-ST16 -> login, input handling, administration, role menus; ST17-ST19 -> CR1 and regression.
