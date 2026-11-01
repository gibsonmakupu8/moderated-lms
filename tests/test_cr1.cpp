// Change request CR1 (release v1.1): "A lecturer should be able to extend the deadline for a whole
// class, not just an individual student, and extensions should be shown on the student's assignment list."
#include <sstream>

#include "TestSupport.hpp"
#include "lms/App.hpp"

using namespace lmstest;

TEST_F(SystemFixture, CR1_ClassExtensionGivesEveryEnrolledStudentTheNewDeadline) {
    Id aid = makeAssignment("2026-11-10 12:00");
    auto r = coursework.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "Power cuts affected the whole class");
    EXPECT_EQ(r.granted, 2);
    EXPECT_EQ(r.skipped, 0);
    Assignment a = coursework.requireAssignment(aid);
    EXPECT_EQ(coursework.effectiveDeadline(a, studentId), "2026-11-13 12:00:00");
    EXPECT_EQ(coursework.effectiveDeadline(a, student2Id), "2026-11-13 12:00:00");
    auto ext = extRepo.findLatest(aid, studentId);
    ASSERT_TRUE(ext.has_value());
    EXPECT_EQ(ext->grantedBy, lecturerId);
    EXPECT_EQ(ext->reason, "Class extension: Power cuts affected the whole class");
}

TEST_F(SystemFixture, CR1_StudentsWithLaterIndividualExtensionAreSkippedNotShortened) {
    Id aid = makeAssignment("2026-11-10 12:00");
    coursework.grantExtension(*lecturer, aid, studentId, "2026-11-20 12:00", "Long illness");
    auto r = coursework.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "Class extension");
    EXPECT_EQ(r.granted, 1);
    EXPECT_EQ(r.skipped, 1);
    Assignment a = coursework.requireAssignment(aid);
    EXPECT_EQ(coursework.effectiveDeadline(a, studentId), "2026-11-20 12:00:00");   // never shortened
    EXPECT_EQ(coursework.effectiveDeadline(a, student2Id), "2026-11-13 12:00:00");
}

TEST_F(SystemFixture, CR1_ClassExtensionValidationAndPermissions) {
    Id aid = makeAssignment("2026-11-10 12:00");
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, aid, "2026-11-10 12:00", "Same"), ValidationError);      // not later than original
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, aid, "2026-11-09 12:00", "Earlier"), ValidationError);
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, aid, "garbage", "Reason"), ValidationError);
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "   "), ValidationError);        // reason required
    EXPECT_THROW(coursework.grantClassExtension(*lecturer2, aid, "2026-11-13 12:00", "Reason"), PermissionError);    // not this course
    EXPECT_THROW(coursework.grantClassExtension(*student, aid, "2026-11-13 12:00", "Reason"), PermissionError);
    EXPECT_THROW(coursework.grantClassExtension(*hod, aid, "2026-11-13 12:00", "Reason"), PermissionError);
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, 999, "2026-11-13 12:00", "Reason"), NotFoundError);
    EXPECT_EQ(extRepo.findAll().size(), 0u);  // nothing was written by any failed call
}

TEST_F(SystemFixture, CR1_RepeatingTheSameClassExtensionChangesNothing) {
    Id aid = makeAssignment("2026-11-10 12:00");
    coursework.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "First");
    EXPECT_THROW(coursework.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "Again"), BusinessRuleError);
    EXPECT_EQ(extRepo.findAll().size(), 2u);  // still only the two rows from the first call
    EXPECT_NO_THROW(coursework.grantClassExtension(*lecturer, aid, "2026-11-15 12:00", "Longer"));  // a later date is fine
}

TEST_F(SystemFixture, CR1_ClassExtensionIsAllOrNothing) {
    Id aid = makeAssignment("2026-11-10 12:00");
    class FailingSecondInsert : public SqliteExtensionRepository {
    public:
        using SqliteExtensionRepository::SqliteExtensionRepository;
        int calls = 0;
        Id insert(const Extension& e) override {
            if (++calls == 2) throw DatabaseError("simulated failure");
            return SqliteExtensionRepository::insert(e);
        }
    } flaky(db);
    CourseworkService faulty(courseRepo, enrolRepo, assignRepo, flaky, subRepo, content, penalty, clock, db);
    EXPECT_THROW(faulty.grantClassExtension(*lecturer, aid, "2026-11-13 12:00", "Class"), DatabaseError);
    EXPECT_EQ(extRepo.findAll().size(), 0u);  // the first student's row was rolled back
}

TEST_F(SystemFixture, CR1_ExtendedDeadlineShownOnStudentAssignmentListOnly) {
    Id aid = makeAssignment("2026-11-10 12:00");
    coursework.grantExtension(*lecturer, aid, studentId, "2026-11-12 12:00", "Hospital visit");
    auto mine = coursework.assignmentsFor(*student);
    ASSERT_EQ(mine.size(), 1u);
    EXPECT_TRUE(mine[0].extended);
    EXPECT_EQ(mine[0].effectiveDeadline, "2026-11-12 12:00:00");
    EXPECT_EQ(mine[0].assignment.deadline, "2026-11-10 12:00:00");  // original is still available
    auto other = coursework.assignmentsFor(*student2);
    EXPECT_FALSE(other[0].extended);
    EXPECT_EQ(other[0].effectiveDeadline, "2026-11-10 12:00:00");
    EXPECT_FALSE(coursework.assignmentsFor(*lecturer)[0].extended);  // staff lists show the assignment deadline
}

TEST_F(SystemFixture, CR1_LateRulesStillApplyAfterClassExtension) {
    Id aid = makeAssignment("2026-11-10 12:00");
    coursework.grantClassExtension(*lecturer, aid, "2026-11-12 12:00", "Class");
    EXPECT_EQ(submitAt("2026-11-11 23:00", aid, *student).status, SubmissionStatus::Submitted);   // on time thanks to extension
    auto late = submitAt("2026-11-12 12:00:01", aid, *student2);                                  // 1 s after the NEW deadline
    EXPECT_EQ(late.status, SubmissionStatus::Late);
    EXPECT_EQ(late.daysLate, 1);
    EXPECT_NEAR(late.penaltyPercent, 10, 1e-9);
}

class Cr1SystemTest : public SystemFixture {
protected:
    std::string run(const std::vector<std::string>& lines) {
        std::string input;
        for (const auto& l : lines) input += l + "\n";
        std::istringstream in(input);
        std::ostringstream out;
        Console io(in, out, false);
        App app(io, auth, admin, coursework, assessment, (storageDir / "app.log").string());
        app.run();
        return out.str();
    }
    static bool has(const std::string& t, const std::string& p) { return t.find(p) != std::string::npos; }
};

TEST_F(Cr1SystemTest, ST17_LecturerExtendsWholeClassThroughMenuAndStudentSeesIt) {
    makeAssignment("2026-11-10 12:00");
    auto lect = run({"1", "lect1", "Password1", "8", "1", "2026-11-13 12:00", "Load shedding", "0", "0"});
    EXPECT_TRUE(has(lect, "Grant extension to a whole class"));
    EXPECT_TRUE(has(lect, "Extension recorded for 2 student(s)."));
    auto stud = run({"1", "stud1", "Password1", "2", "", "0", "0"});
    EXPECT_TRUE(has(stud, "Your deadline"));
    EXPECT_TRUE(has(stud, "2026-11-13 12:00:00*"));
    EXPECT_TRUE(has(stud, "your deadline was extended"));
}

TEST_F(Cr1SystemTest, ST18_ClassExtensionExceptionFlowsShowFriendlyMessages) {
    makeAssignment("2026-11-10 12:00");
    auto out = run({"1", "lect1", "Password1", "8", "1", "2026-11-09 12:00", "Too early", "8", "1", "bad date", "Reason", "8", "99", "2026-11-13 12:00", "Reason", "0", "0"});
    EXPECT_TRUE(has(out, "later than the assignment's original deadline"));
    EXPECT_TRUE(has(out, "Date/time must look like"));
    EXPECT_TRUE(has(out, "does not exist"));
    EXPECT_EQ(extRepo.findAll().size(), 0u);
}

TEST_F(Cr1SystemTest, ST19_UnchangedLecturerMenuNumbersStillWork_Regression) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    run({"1", "stud1", "Password1", "3", "1", makeFile(), "0", "0"});
    // Option 5 (mark) and 4 (individual extension) keep their v1.0 numbers.
    auto out = run({"1", "lect1", "Password1", "5", std::to_string(courseId), "", "", "1", "70", "Fine work", "", "n", "0", "0"});
    EXPECT_TRUE(has(out, "Mark saved"));
}
