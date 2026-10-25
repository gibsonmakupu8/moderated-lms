// Shared fixture: a complete in-memory system with a controllable clock and fast password hashing.
#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "lms/AdminService.hpp"
#include "lms/AssessmentService.hpp"
#include "lms/AuthService.hpp"
#include "lms/ContentStore.hpp"
#include "lms/CourseworkService.hpp"
#include "lms/Database.hpp"
#include "lms/LatePenalty.hpp"
#include "lms/PasswordHasher.hpp"
#include "lms/SqliteRepositories.hpp"
#include "lms/TimeUtil.hpp"

namespace lmstest {

using namespace lms;

class SystemFixture : public ::testing::Test {
protected:
    SystemFixture()
        : db(":memory:"),
          hasher(SodiumPasswordHasher::Cost::Fast),
          clock("2026-11-02 09:00:00"),
          storageDir(std::filesystem::temp_directory_path() / ("lms_test_" + std::to_string(counter()))),
          userRepo(db), deptRepo(db), courseRepo(db), enrolRepo(db), assignRepo(db), extRepo(db), subRepo(db), markRepo(db), auditRepo(db),
          content(storageDir.string()),
          auth(userRepo, hasher),
          admin(userRepo, deptRepo, courseRepo, enrolRepo, hasher),
          coursework(courseRepo, enrolRepo, assignRepo, extRepo, subRepo, content, penalty, clock, db),
          assessment(userRepo, courseRepo, assignRepo, subRepo, markRepo, auditRepo, penalty, clock, db) {}

    void SetUp() override {
        db.initialiseSchema();
        std::filesystem::create_directories(storageDir);
        adminId = admin.bootstrapAdmin("admin", "System Admin", "Admin#12345");
        adminUser = login("admin", "Admin#12345");
        deptId = admin.createDepartment(*adminUser, "Science");
        otherDeptId = admin.createDepartment(*adminUser, "Humanities");
        courseId = admin.createCourse(*adminUser, "CS101", "Intro to Computing", deptId);
        otherCourseId = admin.createCourse(*adminUser, "HIS101", "History", otherDeptId);
        lecturerId = makeUser("lect1", "Lena Lecturer", Role::Lecturer, std::nullopt);
        lecturer2Id = makeUser("lect2", "Leo Lecturer", Role::Lecturer, std::nullopt);
        hodId = makeUser("hod1", "Hanna HoD", Role::HoD, deptId);
        otherHodId = makeUser("hod2", "Hugo HoD", Role::HoD, otherDeptId);
        studentId = makeUser("stud1", "Sipho Student", Role::Student, std::nullopt);
        student2Id = makeUser("stud2", "Thabo Student", Role::Student, std::nullopt);
        admin.assignLecturer(*adminUser, courseId, lecturerId);
        admin.enrolStudent(*adminUser, courseId, studentId);
        admin.enrolStudent(*adminUser, courseId, student2Id);
        lecturer = login("lect1", "Password1");
        lecturer2 = login("lect2", "Password1");
        hod = login("hod1", "Password1");
        otherHod = login("hod2", "Password1");
        student = login("stud1", "Password1");
        student2 = login("stud2", "Password1");
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(storageDir, ec);
    }

    static int counter() {
        static int n = 0;
        return ++n * 1000 + static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() % 997);
    }

    std::unique_ptr<User> login(const std::string& name, const std::string& password) { return auth.login(name, password); }

    Id makeUser(const std::string& name, const std::string& full, Role role, std::optional<Id> dept) {
        NewUser nu;
        nu.username = name;
        nu.fullName = full;
        nu.role = role;
        nu.password = "Password1";
        nu.departmentId = dept;
        return admin.createUser(*adminUser, nu);
    }

    std::string makeFile(const std::string& name = "work.txt", const std::string& text = "my answer") {
        auto path = storageDir / ("src_" + name);
        std::ofstream(path) << text;
        return path.string();
    }

    // Assignment due 2026-11-10 12:00, 100 marks.
    Id makeAssignment(const std::string& deadline = "2026-11-10 12:00") {
        return coursework.createAssignment(*lecturer, courseId, "Essay 1", "Write an essay", deadline, 100);
    }

    // Submit as student at a given moment.
    SubmissionReceipt submitAt(const std::string& when, Id assignmentId, User& who) {
        clock.set(when);
        return coursework.submit(who, assignmentId, makeFile());
    }

    Database db;
    SodiumPasswordHasher hasher;
    FixedClock clock;
    std::filesystem::path storageDir;
    SqliteUserRepository userRepo;
    SqliteDepartmentRepository deptRepo;
    SqliteCourseRepository courseRepo;
    SqliteEnrolmentRepository enrolRepo;
    SqliteAssignmentRepository assignRepo;
    SqliteExtensionRepository extRepo;
    SqliteSubmissionRepository subRepo;
    SqliteMarkRepository markRepo;
    SqliteAuditRepository auditRepo;
    FileContentStore content;
    DailyTenPercentPenalty penalty;
    AuthService auth;
    AdminService admin;
    CourseworkService coursework;
    AssessmentService assessment;

    Id adminId = 0, deptId = 0, otherDeptId = 0, courseId = 0, otherCourseId = 0;
    Id lecturerId = 0, lecturer2Id = 0, hodId = 0, otherHodId = 0, studentId = 0, student2Id = 0;
    std::unique_ptr<User> adminUser, lecturer, lecturer2, hod, otherHod, student, student2;
};

}  // namespace lmstest
