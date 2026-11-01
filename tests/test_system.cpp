// System and acceptance tests: the real console application (App + Console), services, repositories
// and SQLite are driven with scripted keyboard input and the screen output is checked. Each test is
// traced to a use case flow (UC-01 Submit assignment, UC-02 Mark submission, UC-03 Moderate and
// release marks) or to a stakeholder statement, and includes exception flows.
#include <sstream>

#include "TestSupport.hpp"
#include "lms/App.hpp"

using namespace lmstest;

class SystemTest : public SystemFixture {
protected:
    // Runs the application with the given input lines (one per prompt) and returns everything it printed.
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
    static std::vector<std::string> login(const std::string& u, const std::string& p = "Password1") { return {"1", u, p}; }
    static std::vector<std::string> cat(std::vector<std::string> a, const std::vector<std::string>& b) {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }
    static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
    std::size_t submissions() { return subRepo.findAll().size(); }
};

// ---- UC-01 Submit assignment ---------------------------------------------------------------------
TEST_F(SystemTest, ST01_SubmitOnTime_MainFlow) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    auto out = run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    EXPECT_TRUE(has(out, "Deadline that applies to you: 2026-11-10 12:00:00"));
    EXPECT_TRUE(has(out, "Submission recorded (attempt 1) at 2026-11-09 10:00:00"));
    EXPECT_FALSE(has(out, "late"));
    EXPECT_EQ(submissions(), 1u);
}

TEST_F(SystemTest, ST02_SubmitLate_AlternativeFlow_ShowsPenalty) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-11 09:00");  // 21 hours late = day 1
    auto out = run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    EXPECT_TRUE(has(out, "1 day(s) late"));
    EXPECT_TRUE(has(out, "10.00% of the available mark"));
    EXPECT_EQ(subRepo.findLatest(1, studentId)->status, SubmissionStatus::Late);  // accepted, not rejected
}

TEST_F(SystemTest, ST03_SubmitMoreThanThreeDaysLate_AcceptedWithZeroWarning) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-14 12:00");
    auto out = run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    EXPECT_TRUE(has(out, "more than 3 days late receives zero"));
    EXPECT_EQ(submissions(), 1u);
}

TEST_F(SystemTest, ST04_Resubmission_BeforeDeadlineAllowed_AfterDeadlineRefused) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    auto first = run(cat(login("stud1"), {"3", "1", makeFile(), "3", "1", makeFile("v2.txt", "better"), "0", "0"}));
    EXPECT_TRUE(has(first, "attempt 1"));
    EXPECT_TRUE(has(first, "attempt 2"));
    clock.set("2026-11-10 12:00:01");
    auto second = run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    EXPECT_TRUE(has(second, "deadline has passed"));
    EXPECT_EQ(submissions(), 2u);  // nothing added
}

TEST_F(SystemTest, ST05_SubmitExceptionFlows_BadFileNameAndType) {
    makeAssignment();
    clock.set("2026-11-09 10:00");
    auto out = run(cat(login("stud1"), {"3", "1", "/no/such/file.pdf", "3", "1", makeFile("run.exe", "MZ"), "3", "999", "0", "0"}));
    EXPECT_TRUE(has(out, "was not found"));
    EXPECT_TRUE(has(out, "Unsupported file type"));
    EXPECT_TRUE(has(out, "does not exist"));  // unknown assignment id
    EXPECT_EQ(submissions(), 0u);
}

TEST_F(SystemTest, ST06_ExtensionGivesStudentMoreTime_Stakeholder_Lecturer) {
    makeAssignment("2026-11-10 12:00");
    // Lecturer grants an extension through the menu (option 4).
    run(cat(login("lect1"), {"4", "1", std::to_string(studentId), "2026-11-12 12:00", "Hospital visit", "0", "0"}));
    clock.set("2026-11-11 15:00");
    auto out = run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    EXPECT_TRUE(has(out, "Deadline that applies to you: 2026-11-12 12:00:00"));
    EXPECT_FALSE(has(out, "late"));
}

// ---- UC-02 / UC-03 Marking, moderation, release ---------------------------------------------------
TEST_F(SystemTest, ST07_FullModerationFlow_StudentSeesMarkOnlyAfterRelease_Stakeholder_Principal) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));

    // Lecturer marks and sends for moderation.
    run(cat(login("lect1"), {"5", std::to_string(courseId), "", "", "1", "80", "Good structure, improve referencing", "", "y", "0", "0"}));

    // Student looks for the result: not available yet (principal's rule).
    auto before = run(cat(login("stud1"), {"5", "1", "0", "0"}));
    EXPECT_TRUE(has(before, "No released result is available"));
    EXPECT_FALSE(has(before, "Good structure"));

    // HoD approves and releases.
    auto hodOut = run(cat(login("hod1"), {"1", "", "1", "1", "Fair", "2", "y", "0", "0"}));
    EXPECT_TRUE(has(hodOut, "Awaiting moderation"));
    EXPECT_TRUE(has(hodOut, "1 result(s) released"));

    auto after = run(cat(login("stud1"), {"5", "1", "0", "0"}));
    EXPECT_TRUE(has(after, "Mark: 80.00 / 100.00"));
    EXPECT_TRUE(has(after, "Good structure, improve referencing"));
}

TEST_F(SystemTest, ST08_HodReturnsMark_ReasonRequired_StudentStillHidden) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    run(cat(login("lect1"), {"5", std::to_string(courseId), "", "", "1", "95", "Excellent", "", "y", "0", "0"}));
    // decision 2 = return; first an empty reason (re-asked), then a real one.
    auto out = run(cat(login("hod1"), {"1", "", "1", "2", "", "Mark too generous for rubric", "0", "0"}));
    EXPECT_TRUE(has(out, "This field cannot be empty"));
    EXPECT_TRUE(has(out, "Returned to the lecturer"));
    EXPECT_EQ(subRepo.findById(1)->status, SubmissionStatus::ReturnedForCorrection);
    EXPECT_TRUE(has(run(cat(login("stud1"), {"5", "1", "0", "0"})), "No released result"));
}

TEST_F(SystemTest, ST09_MarkOutOfRangeAndMissingFeedback_ExceptionFlows) {
    makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    run(cat(login("stud1"), {"3", "1", makeFile(), "0", "0"}));
    auto out = run(cat(login("lect1"), {"5", std::to_string(courseId), "", "", "1", "150", "Feedback", "", "0", "0"}));
    EXPECT_TRUE(has(out, "between 0 and 100.00"));
    EXPECT_EQ(markRepo.findAll().size(), 0u);
    auto out2 = run(cat(login("lect1"), {"5", std::to_string(courseId), "", "", "1", "70", "", "Good", "", "n", "0", "0"}));
    EXPECT_TRUE(has(out2, "This field cannot be empty"));  // feedback re-asked
    EXPECT_EQ(markRepo.findAll().size(), 1u);
}

// ---- Login and input handling ---------------------------------------------------------------------
TEST_F(SystemTest, ST10_LoginFailures_AndLockoutBackToWelcome) {
    auto out = run({"1", "stud1", "bad1", "stud1", "bad2", "stud1", "bad3", "stud1", "bad4", "stud1", "bad5", "0"});
    EXPECT_TRUE(has(out, "Invalid username or password."));
    EXPECT_TRUE(has(out, "Too many failed attempts"));
    EXPECT_FALSE(has(out, "Welcome, Sipho"));
}

TEST_F(SystemTest, ST11_NonNumericMenuInputDoesNotCrash) {
    auto out = run(cat(login("stud1"), {"abc", "99", "", "0", "0"}));
    EXPECT_TRUE(has(out, "Please enter a whole number from 0 to 6"));
    EXPECT_TRUE(has(out, "You have been logged out."));
}

TEST_F(SystemTest, ST12_EndOfInputExitsCleanly) {
    auto out = run(login("stud1"));  // input ends while the menu is waiting
    EXPECT_TRUE(has(out, "Input closed. Goodbye."));
}

TEST_F(SystemTest, ST13_DeactivatedAccountIsLoggedOutDuringSession) {
    // The student is logged in; before the next action an administrator deactivates the account.
    admin.setActive(*adminUser, studentId, false);
    auto out = run(cat(login("stud1"), {"1", "0"}));  // login itself fails now
    EXPECT_TRUE(has(out, "Invalid username or password."));
    admin.setActive(*adminUser, studentId, true);
    EXPECT_TRUE(auth.isActive(*student));
    admin.setActive(*adminUser, studentId, false);
    EXPECT_FALSE(auth.isActive(*student));  // used by the menu loop to end an open session
}

TEST_F(SystemTest, ST14_AdministratorCreatesAccountThroughMenus_NonNumericDepartmentHandled) {
    // option 2 = create user; role 4 = Student; department "abc" is re-asked, blank accepted.
    auto out = run(cat(login("admin", "Admin#12345"), {"2", "newstud", "New Student", "4", "abc", "", "Password9", "1", "0"}));
    EXPECT_TRUE(has(out, "Please enter a whole number"));
    EXPECT_TRUE(has(out, "User created."));
    EXPECT_TRUE(userRepo.findByUsername("newstud").has_value());
    EXPECT_FALSE(has(out, "unexpected problem"));
}

TEST_F(SystemTest, ST15_DuplicateUsernameAndWeakPasswordAreReportedNotCrashed) {
    auto out = run(cat(login("admin", "Admin#12345"), {"2", "stud1", "Dup", "4", "", "Password9", "2", "weakling", "Weak", "4", "", "short", "0"}));
    EXPECT_TRUE(has(out, "already taken"));
    EXPECT_TRUE(has(out, "Password must be 8-128 characters"));
}

TEST_F(SystemTest, ST16_RoleMenusOnlyOfferTheirOwnFunctions) {
    auto student = run(cat(login("stud1"), {"0", "0"}));
    EXPECT_TRUE(has(student, "Submit or resubmit an assignment"));
    EXPECT_FALSE(has(student, "Release approved results"));
    EXPECT_FALSE(has(student, "Create user"));
    auto lecturer = run(cat(login("lect1"), {"0", "0"}));
    EXPECT_FALSE(has(lecturer, "Release approved results"));
    EXPECT_TRUE(has(lecturer, "Grant individual extension"));
    auto hodOut = run(cat(login("hod1"), {"0", "0"}));
    EXPECT_TRUE(has(hodOut, "Release approved results"));
    EXPECT_FALSE(has(hodOut, "View and mark submissions"));
}

TEST(SystemFirstRun, EmptyDatabaseAsksForAdministratorFirst) {
    Database db(":memory:");
    db.initialiseSchema();
    SqliteUserRepository users(db);
    SqliteDepartmentRepository depts(db);
    SqliteCourseRepository courses(db);
    SqliteEnrolmentRepository enrol(db);
    SqliteAssignmentRepository assignments(db);
    SqliteExtensionRepository extensions(db);
    SqliteSubmissionRepository subs(db);
    SqliteMarkRepository marks(db);
    SqliteAuditRepository audit(db);
    SodiumPasswordHasher hasher(SodiumPasswordHasher::Cost::Fast);
    FixedClock clock("2026-11-02 09:00");
    DailyTenPercentPenalty penalty;
    FileContentStore content((std::filesystem::temp_directory_path() / "lms_firstrun").string());
    AuthService auth(users, hasher);
    AdminService admin(users, depts, courses, enrol, hasher);
    CourseworkService cw(courses, enrol, assignments, extensions, subs, content, penalty, clock, db);
    AssessmentService as(users, courses, assignments, subs, marks, audit, penalty, clock, db);
    // mismatching passwords are rejected and asked again; then the account works.
    std::istringstream in("admin\nFirst Admin\nPassw0rdOK\nDifferent1\nadmin\nFirst Admin\nPassw0rdOK\nPassw0rdOK\n0\n");
    std::ostringstream out;
    Console io(in, out, false);
    App app(io, auth, admin, cw, as, (std::filesystem::temp_directory_path() / "lms_firstrun.log").string());
    app.run();
    EXPECT_TRUE(out.str().find("The passwords do not match") != std::string::npos);
    EXPECT_TRUE(out.str().find("Administrator account created") != std::string::npos);
    EXPECT_FALSE(admin.needsBootstrap());
}
