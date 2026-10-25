// Unit tests of pure domain rules: time parsing, late penalty (boundary value analysis),
// state machine, validation. No database involved.
#include <gtest/gtest.h>

#include "lms/LatePenalty.hpp"
#include "lms/TimeUtil.hpp"
#include "lms/User.hpp"
#include "lms/Validation.hpp"

using namespace lms;

TEST(Time, RoundTripAndSeconds) {
    EXPECT_EQ(formatTimestamp(parseTimestamp("2026-11-10 12:00")), "2026-11-10 12:00:00");
    EXPECT_EQ(parseTimestamp("2000-01-01 00:00:00"), 946684800);
    EXPECT_EQ(parseTimestamp("2026-11-10 12:00:01") - parseTimestamp("2026-11-10 12:00:00"), 1);
    EXPECT_EQ(formatTimestamp(parseTimestamp("2028-02-29 23:59:59")), "2028-02-29 23:59:59");  // leap day
}

TEST(Time, RejectsInvalidInput) {
    EXPECT_THROW(parseTimestamp("tomorrow"), ValidationError);
    EXPECT_THROW(parseTimestamp(""), ValidationError);
    EXPECT_THROW(parseTimestamp("2026-13-01 10:00"), ValidationError);
    EXPECT_THROW(parseTimestamp("2026-02-30 10:00"), ValidationError);
    EXPECT_THROW(parseTimestamp("2027-02-29 10:00"), ValidationError);  // not a leap year
    EXPECT_THROW(parseTimestamp("2026-11-10 24:00"), ValidationError);
    EXPECT_THROW(parseTimestamp("2026-11-10 12:60"), ValidationError);
    EXPECT_THROW(parseTimestamp("2026-11-10 12:00 extra"), ValidationError);
}

TEST(Time, FixedClockAdvances) {
    FixedClock clock("2026-11-10 12:00:00");
    clock.advance(3600);
    EXPECT_EQ(clock.nowString(), "2026-11-10 13:00:00");
    SystemClock system;
    EXPECT_GT(system.nowSeconds(), parseTimestamp("2026-01-01 00:00"));
}

// ---- Boundary value analysis for the late-penalty rule (FR-11) -------------------------------
struct PenaltyCase {
    const char* submitted;
    int days;
    double percent;
};

TEST(LatePenalty, BoundaryValues) {
    DailyTenPercentPenalty rule;
    const std::string deadline = "2026-11-10 12:00:00";
    const PenaltyCase cases[] = {
        {"2026-11-10 11:59:59", 0, 0},    // just before deadline
        {"2026-11-10 12:00:00", 0, 0},    // exactly on the deadline = on time
        {"2026-11-10 12:00:01", 1, 10},   // 1 second late = day 1
        {"2026-11-11 12:00:00", 1, 10},   // exactly 24 h late = still day 1
        {"2026-11-11 12:00:01", 2, 20},   // 24 h + 1 s = day 2
        {"2026-11-12 12:00:00", 2, 20},   // 48 h
        {"2026-11-12 12:00:01", 3, 30},   // 48 h + 1 s = day 3
        {"2026-11-13 12:00:00", 3, 30},   // exactly 72 h = last penalised moment
        {"2026-11-13 12:00:01", 4, 100},  // 72 h + 1 s = zero
        {"2026-12-25 12:00:00", 45, 100}  // very late (exactly 45 days)
    };
    for (const auto& c : cases) {
        EXPECT_EQ(rule.daysLate(deadline, c.submitted), c.days);
        EXPECT_NEAR(rule.penaltyPercent(c.days), c.percent, 1e-9);
    }
}

TEST(LatePenalty, AppliesPercentageOfMaximumMark) {
    DailyTenPercentPenalty rule;
    EXPECT_NEAR(rule.apply(80, 100, 0), 80, 1e-9);
    EXPECT_NEAR(rule.apply(80, 100, 1), 70, 1e-9);  // 10% of 100 = 10 marks
    EXPECT_NEAR(rule.apply(80, 100, 3), 50, 1e-9);
    EXPECT_NEAR(rule.apply(40, 50, 2), 30, 1e-9);   // 20% of 50 = 10 marks
    EXPECT_NEAR(rule.apply(20, 100, 3), 0, 1e-9);   // never negative
    EXPECT_NEAR(rule.apply(100, 100, 4), 0, 1e-9);  // more than 3 days late = zero
    EXPECT_NEAR(rule.penaltyPercent(-5), 0, 1e-9);
}

TEST(StateMachine, AllowedAndForbiddenTransitions) {
    using S = SubmissionStatus;
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::Submitted, S::Marked));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::Late, S::Marked));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::Marked, S::AwaitingModeration));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::AwaitingModeration, S::Moderated));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::AwaitingModeration, S::ReturnedForCorrection));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::ReturnedForCorrection, S::Marked));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::Moderated, S::Released));
    EXPECT_TRUE(SubmissionStateMachine::canTransition(S::Draft, S::Late));

    EXPECT_FALSE(SubmissionStateMachine::canTransition(S::Submitted, S::Released));
    EXPECT_FALSE(SubmissionStateMachine::canTransition(S::Marked, S::Released));
    EXPECT_FALSE(SubmissionStateMachine::canTransition(S::AwaitingModeration, S::Released));
    EXPECT_FALSE(SubmissionStateMachine::canTransition(S::Released, S::Marked));
    EXPECT_FALSE(SubmissionStateMachine::canTransition(S::Moderated, S::Marked));
    EXPECT_THROW(SubmissionStateMachine::require(S::Marked, S::Released), BusinessRuleError);
    EXPECT_NO_THROW(SubmissionStateMachine::require(S::Moderated, S::Released));
}

TEST(Enums, StringRoundTrip) {
    for (Role r : {Role::Admin, Role::Lecturer, Role::HoD, Role::Student}) {
        EXPECT_EQ(roleFromString(toString(r)), r);
        EXPECT_FALSE(displayName(r).empty());
    }
    for (int i = 0; i <= 7; ++i) {
        auto s = static_cast<SubmissionStatus>(i);
        EXPECT_EQ(submissionStatusFromString(toString(s)), s);
        EXPECT_FALSE(displayName(s).empty());
    }
    for (auto m : {ModerationStatus::Pending, ModerationStatus::Approved, ModerationStatus::Returned}) {
        EXPECT_EQ(moderationStatusFromString(toString(m)), m);
    }
    for (int i = 0; i <= 5; ++i) {
        auto e = static_cast<AuditEvent>(i);
        EXPECT_EQ(auditEventFromString(toString(e)), e);
    }
    EXPECT_THROW(roleFromString("ROOT"), ValidationError);
    EXPECT_THROW(submissionStatusFromString("???"), ValidationError);
    EXPECT_THROW(moderationStatusFromString("???"), ValidationError);
    EXPECT_THROW(auditEventFromString("???"), ValidationError);
}

TEST(Validation, TextUsernamePasswordMark) {
    EXPECT_EQ(trim("  hi \n"), "hi");
    EXPECT_EQ(requireText("  Essay ", "Title", 10), "Essay");
    EXPECT_THROW(requireText("   ", "Title", 10), ValidationError);
    EXPECT_THROW(requireText("12345678901", "Title", 10), ValidationError);
    EXPECT_TRUE(containsIgnoreCase("Introduction to C++", "TO c++"));
    EXPECT_FALSE(containsIgnoreCase("Intro", "xyz"));

    EXPECT_EQ(requireUsername("a.b_c-9"), "a.b_c-9");
    EXPECT_THROW(requireUsername("ab"), ValidationError);                 // too short (boundary 2)
    EXPECT_NO_THROW(requireUsername("abc"));                              // boundary 3
    EXPECT_NO_THROW(requireUsername(std::string(30, 'a')));               // boundary 30
    EXPECT_THROW(requireUsername(std::string(31, 'a')), ValidationError); // boundary 31
    EXPECT_THROW(requireUsername("bad name"), ValidationError);
    EXPECT_THROW(requireUsername("x'; DROP"), ValidationError);

    EXPECT_NO_THROW(requirePasswordPolicy("abcdefg1"));
    EXPECT_THROW(requirePasswordPolicy("abcdef1"), ValidationError);      // 7 chars
    EXPECT_THROW(requirePasswordPolicy("abcdefgh"), ValidationError);     // no digit
    EXPECT_THROW(requirePasswordPolicy("12345678"), ValidationError);     // no letter

    EXPECT_NEAR(requireMark(0, 100, "Mark"), 0, 1e-9);                    // boundaries 0 and max
    EXPECT_NEAR(requireMark(100, 100, "Mark"), 100, 1e-9);
    EXPECT_NEAR(requireMark(72.456, 100, "Mark"), 72.46, 1e-9);
    EXPECT_THROW(requireMark(-0.01, 100, "Mark"), ValidationError);
    EXPECT_THROW(requireMark(100.01, 100, "Mark"), ValidationError);
    EXPECT_THROW(requireMark(std::nan(""), 100, "Mark"), ValidationError);
    EXPECT_EQ(formatMark(7.5), "7.50");
}

TEST(Users, FactoryCreatesCorrectSubclassWithRolePermissions) {
    UserRecord r;
    r.id = 1;
    r.username = "x";
    r.fullName = "X";
    for (Role role : {Role::Admin, Role::Lecturer, Role::HoD, Role::Student}) {
        r.role = role;
        auto user = UserFactory::create(r);
        EXPECT_EQ(user->role(), role);
        EXPECT_FALSE(user->roleName().empty());
    }
    r.role = Role::Admin;
    auto admin = UserFactory::create(r);
    r.role = Role::Lecturer;
    auto lecturer = UserFactory::create(r);
    r.role = Role::HoD;
    auto hod = UserFactory::create(r);
    r.role = Role::Student;
    auto student = UserFactory::create(r);

    EXPECT_TRUE(admin->can(Permission::ManageUsers));
    EXPECT_FALSE(admin->can(Permission::MarkSubmissions));
    EXPECT_TRUE(lecturer->can(Permission::MarkSubmissions));
    EXPECT_FALSE(lecturer->can(Permission::ReleaseMarks));  // lecturers cannot release marks
    EXPECT_TRUE(hod->can(Permission::ReleaseMarks));
    EXPECT_FALSE(hod->can(Permission::MarkSubmissions));
    EXPECT_TRUE(student->can(Permission::SubmitWork));
    EXPECT_FALSE(student->can(Permission::ModerateMarks));
    EXPECT_THROW(requirePermission(*student, Permission::ManageUsers), PermissionError);
    EXPECT_NO_THROW(requirePermission(*admin, Permission::ManageUsers));

    r.role = static_cast<Role>(99);
    EXPECT_THROW(UserFactory::create(r), ValidationError);
    for (int i = 0; i <= 9; ++i) EXPECT_FALSE(toString(static_cast<Permission>(i)).empty());
}
