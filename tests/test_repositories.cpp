// Integration tests: repositories against a real (in-memory) SQLite database.
#include "TestSupport.hpp"

using namespace lmstest;

TEST_F(SystemFixture, RepositoriesRoundTripAndLatestAttemptQueries) {
    auto user = userRepo.findByUsername("stud1");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->role, Role::Student);
    EXPECT_NE(user->passwordHash, "Password1");  // never plain text
    EXPECT_EQ(user->passwordHash.rfind("$argon2id$", 0), 0u);
    EXPECT_EQ(userRepo.findByRole(Role::Lecturer).size(), 2u);
    EXPECT_FALSE(userRepo.findById(99999).has_value());

    Id aid = makeAssignment();
    submitAt("2026-11-03 10:00", aid, *student);
    submitAt("2026-11-04 10:00", aid, *student);  // resubmission
    EXPECT_EQ(subRepo.historyOf(aid, studentId).size(), 2u);
    auto latest = subRepo.findLatest(aid, studentId);
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->attemptNo, 2);
    EXPECT_EQ(subRepo.latestByCourse(courseId, std::nullopt).size(), 1u);  // one student, latest only
    EXPECT_EQ(subRepo.latestByDepartment(deptId, SubmissionStatus::Submitted).size(), 1u);
    EXPECT_EQ(subRepo.latestByDepartment(otherDeptId, std::nullopt).size(), 0u);
    EXPECT_EQ(subRepo.latestByStudent(studentId).size(), 1u);
    EXPECT_EQ(subRepo.findAll().size(), 2u);
}

TEST_F(SystemFixture, ConstraintsAreEnforcedByTheDatabase) {
    // Duplicate username / course code / enrolment (UNIQUE constraints).
    UserRecord dup;
    dup.username = "stud1";
    dup.fullName = "Dup";
    dup.role = Role::Student;
    dup.passwordHash = "x";
    EXPECT_THROW(userRepo.insert(dup), DatabaseError);
    Course c{0, "CS101", "Dup", deptId};
    EXPECT_THROW(courseRepo.insert(c), DatabaseError);
    EXPECT_THROW(enrolRepo.insert(Enrolment{0, courseId, studentId}), DatabaseError);
    // Foreign keys are on: an enrolment for a non-existent student is rejected.
    EXPECT_THROW(enrolRepo.insert(Enrolment{0, courseId, 424242}), DatabaseError);
    // CHECK constraints: HoD needs a department; max mark must be positive.
    UserRecord hodNoDept = dup;
    hodNoDept.username = "hodx";
    hodNoDept.role = Role::HoD;
    EXPECT_THROW(userRepo.insert(hodNoDept), DatabaseError);
    Assignment bad;
    bad.courseId = courseId;
    bad.title = "t";
    bad.deadline = "2026-11-10 12:00:00";
    bad.maxMark = 0;
    bad.createdBy = lecturerId;
    bad.createdAt = "2026-11-01 00:00:00";
    EXPECT_THROW(assignRepo.insert(bad), DatabaseError);
}

TEST_F(SystemFixture, SqlInjectionAttemptIsTreatedAsPlainData) {
    const std::string attack = "x'; DROP TABLE users; --";
    EXPECT_FALSE(userRepo.findByUsername(attack).has_value());
    EXPECT_FALSE(courseRepo.findByCode("' OR '1'='1").has_value());
    EXPECT_THROW(login(attack, "whatever"), AuthenticationError);
    EXPECT_GT(userRepo.count(), 0);  // table still exists
    // Even stored verbatim as a department name it is just text.
    EXPECT_NO_THROW(admin.createDepartment(*adminUser, "Robert'); DROP TABLE departments;--"));
    EXPECT_EQ(deptRepo.findAll().size(), 3u);
}

TEST_F(SystemFixture, AuditEntriesCannotBeModifiedOrDeleted) {
    Id aid = makeAssignment();
    auto receipt = submitAt("2026-11-03 10:00", aid, *student);
    assessment.recordMark(*lecturer, receipt.submissionId, 70, "Good work", "");
    ASSERT_EQ(auditRepo.findAll().size(), 1u);
    EXPECT_THROW(db.execute("UPDATE audit_entries SET reason = 'tampered'"), DatabaseError);
    EXPECT_THROW(db.execute("DELETE FROM audit_entries"), DatabaseError);
    EXPECT_EQ(auditRepo.findAll().size(), 1u);
}

TEST_F(SystemFixture, TransactionRollsBackOnFailureAndJoinsWhenNested) {
    EXPECT_THROW(db.runInTransaction([&] {
                     deptRepo.insert(Department{0, "Temp"});
                     throw std::runtime_error("boom");
                 }),
                 std::runtime_error);
    EXPECT_FALSE(deptRepo.findByName("Temp").has_value());  // rolled back

    db.runInTransaction([&] {
        db.runInTransaction([&] { deptRepo.insert(Department{0, "Nested"}); });  // joins outer transaction
    });
    EXPECT_TRUE(deptRepo.findByName("Nested").has_value());
}

TEST(DatabaseOpen, BadPathThrowsDatabaseError) {
    EXPECT_THROW(Database("/nonexistent_dir_xyz/sub/lms.db"), DatabaseError);
}
