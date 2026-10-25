// NFR-02 / NFR-03: searches complete within 3 seconds with 800 students stored.
#include <chrono>

#include "TestSupport.hpp"

using namespace lmstest;

TEST_F(SystemFixture, SearchesWithin3SecondsFor800Students) {
    constexpr int kStudents = 800;
    UserRecord proto;
    proto.role = Role::Student;
    proto.passwordHash = hasher.hash("Password1");  // one fast hash reused: hashing cost is not what is measured
    db.runInTransaction([&] {
        for (int i = 0; i < kStudents; ++i) {
            proto.username = "pstud" + std::to_string(i);
            proto.fullName = "Perf Student " + std::to_string(i);
            Id sid = userRepo.insert(proto);
            enrolRepo.insert(Enrolment{0, courseId, sid});
        }
    });
    Id aid = makeAssignment("2026-11-10 12:00");
    clock.set("2026-11-09 10:00");
    db.runInTransaction([&] {
        for (const auto& s : userRepo.findByRole(Role::Student)) {
            Submission sub;
            sub.assignmentId = aid;
            sub.studentId = s.id;
            sub.submittedAt = "2026-11-09 10:00:00";
            sub.contentPath = "x.txt";
            sub.status = SubmissionStatus::Submitted;
            subRepo.insert(sub);
        }
    });
    EXPECT_GE(userRepo.findByRole(Role::Student).size(), static_cast<std::size_t>(kStudents));

    auto timed = [](const std::function<void()>& work) {
        auto start = std::chrono::steady_clock::now();
        work();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    };
    SubmissionFilter byName;
    byName.keyword = "Student 799";
    EXPECT_LT(timed([&] { EXPECT_GE(assessment.submissionsForCourse(*lecturer, courseId, byName).size(), 1u); }), 3.0);
    EXPECT_LT(timed([&] { EXPECT_GE(assessment.submissionsForCourse(*lecturer, courseId).size(), static_cast<std::size_t>(kStudents)); }), 3.0);
    EXPECT_LT(timed([&] { assessment.moderationQueue(*hod); }), 3.0);
    EXPECT_LT(timed([&] { coursework.assignmentsFor(*student, "essay"); }), 3.0);
    EXPECT_LT(timed([&] { login("pstud400", "Password1"); }), 2.0);  // login within 2 s with 800 users stored
}
