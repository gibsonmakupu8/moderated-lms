#include "TestSupport.hpp"

using namespace lmstest;

TEST_F(SystemFixture, LoginSucceedsForEachRoleAndReturnsRightSubclass) {
    EXPECT_EQ(login("admin", "Admin#12345")->role(), Role::Admin);
    EXPECT_EQ(login("lect1", "Password1")->role(), Role::Lecturer);
    EXPECT_EQ(login("hod1", "Password1")->role(), Role::HoD);
    EXPECT_EQ(login("stud1", "Password1")->role(), Role::Student);
    EXPECT_EQ(login("  stud1  ", "Password1")->username(), "stud1");  // surrounding spaces are trimmed
}

TEST_F(SystemFixture, LoginFailuresGiveTheSameMessage) {
    std::string wrongPassword, unknownUser;
    try { login("stud1", "WrongPass1"); } catch (const AuthenticationError& e) { wrongPassword = e.what(); }
    try { login("nobody", "Password1"); } catch (const AuthenticationError& e) { unknownUser = e.what(); }
    EXPECT_FALSE(wrongPassword.empty());
    EXPECT_EQ(wrongPassword, unknownUser);  // does not reveal which usernames exist
    EXPECT_THROW(login("", ""), AuthenticationError);
    EXPECT_THROW(login(std::string(500, 'a'), "x"), AuthenticationError);
}

TEST_F(SystemFixture, DeactivatedAccountCannotLogIn) {
    admin.setActive(*adminUser, studentId, false);
    EXPECT_THROW(login("stud1", "Password1"), AuthenticationError);
    admin.setActive(*adminUser, studentId, true);
    EXPECT_NO_THROW(login("stud1", "Password1"));
}

TEST_F(SystemFixture, ChangePasswordNeedsCurrentPasswordAndPolicy) {
    EXPECT_THROW(auth.changePassword(*student, "WrongOld1", "NewPassw0rd"), AuthenticationError);
    EXPECT_THROW(auth.changePassword(*student, "Password1", "short"), ValidationError);
    auth.changePassword(*student, "Password1", "NewPassw0rd");
    EXPECT_THROW(login("stud1", "Password1"), AuthenticationError);
    EXPECT_NO_THROW(login("stud1", "NewPassw0rd"));
}

TEST_F(SystemFixture, SamePasswordGivesDifferentSaltedHashes) {
    auto a = userRepo.findByUsername("stud1");
    auto b = userRepo.findByUsername("stud2");  // both created with "Password1"
    ASSERT_TRUE(a && b);
    EXPECT_NE(a->passwordHash, b->passwordHash);
    EXPECT_TRUE(hasher.verify(a->passwordHash, "Password1"));
    EXPECT_FALSE(hasher.verify(a->passwordHash, "password1"));
    EXPECT_FALSE(hasher.verify("", "x"));
    EXPECT_FALSE(hasher.verify("not-a-hash", "x"));
}

TEST_F(SystemFixture, BootstrapOnlyWorksOnce) {
    EXPECT_FALSE(admin.needsBootstrap());
    EXPECT_THROW(admin.bootstrapAdmin("evil", "Evil", "Passw0rdOK"), PermissionError);
}

TEST(Bootstrap, FirstRunCreatesAdminWithValidation) {
    Database db(":memory:");
    db.initialiseSchema();
    SqliteUserRepository users(db);
    SqliteDepartmentRepository depts(db);
    SqliteCourseRepository courses(db);
    SqliteEnrolmentRepository enrol(db);
    SodiumPasswordHasher hasher(SodiumPasswordHasher::Cost::Fast);
    AdminService admin(users, depts, courses, enrol, hasher);
    EXPECT_TRUE(admin.needsBootstrap());
    EXPECT_THROW(admin.bootstrapAdmin("ab", "X", "Passw0rdOK"), ValidationError);
    EXPECT_THROW(admin.bootstrapAdmin("admin", "  ", "Passw0rdOK"), ValidationError);
    EXPECT_THROW(admin.bootstrapAdmin("admin", "Admin", "weak"), ValidationError);
    EXPECT_NO_THROW(admin.bootstrapAdmin("admin", "Admin", "Passw0rdOK"));
    EXPECT_FALSE(admin.needsBootstrap());
}

TEST_F(SystemFixture, OnlyAdministratorsManageAccountsAndStructure) {
    NewUser nu;
    nu.username = "newbie";
    nu.fullName = "New Bie";
    nu.password = "Password1";
    EXPECT_THROW(admin.createUser(*lecturer, nu), PermissionError);
    EXPECT_THROW(admin.createUser(*student, nu), PermissionError);
    EXPECT_THROW(admin.createDepartment(*hod, "X"), PermissionError);
    EXPECT_THROW(admin.createCourse(*lecturer, "X1", "X", deptId), PermissionError);
    EXPECT_THROW(admin.enrolStudent(*lecturer, courseId, studentId), PermissionError);
    EXPECT_THROW(admin.listUsers(*student), PermissionError);
    EXPECT_NO_THROW(admin.createUser(*adminUser, nu));
}

TEST_F(SystemFixture, UserValidationAndBusinessRules) {
    NewUser nu;
    nu.username = "stud1";  // duplicate
    nu.fullName = "Copy";
    nu.password = "Password1";
    EXPECT_THROW(admin.createUser(*adminUser, nu), ValidationError);
    nu.username = "hod3";
    nu.role = Role::HoD;  // HoD without department
    EXPECT_THROW(admin.createUser(*adminUser, nu), ValidationError);
    nu.departmentId = 999;  // unknown department
    EXPECT_THROW(admin.createUser(*adminUser, nu), NotFoundError);
    nu.departmentId = deptId;
    EXPECT_NO_THROW(admin.createUser(*adminUser, nu));

    EXPECT_NO_THROW(admin.updateUser(*adminUser, studentId, "Sipho Renamed", std::nullopt));
    EXPECT_EQ(userRepo.findById(studentId)->fullName, "Sipho Renamed");
    EXPECT_THROW(admin.updateUser(*adminUser, hodId, "Hanna", std::nullopt), ValidationError);
    EXPECT_THROW(admin.updateUser(*adminUser, 4242, "Nobody", std::nullopt), NotFoundError);
    EXPECT_NO_THROW(admin.resetPassword(*adminUser, studentId, "Another1Pass"));
    EXPECT_NO_THROW(login("stud1", "Another1Pass"));
    EXPECT_THROW(admin.resetPassword(*adminUser, studentId, "weak"), ValidationError);
    EXPECT_EQ(admin.listUsers(*adminUser, Role::Student).size(), 2u);
    EXPECT_GE(admin.listUsers(*adminUser).size(), 7u);
}

TEST_F(SystemFixture, CannotDeactivateSelfOrLastAdministrator) {
    EXPECT_THROW(admin.setActive(*adminUser, adminId, false), BusinessRuleError);  // self
    NewUser second;
    second.username = "admin2";
    second.fullName = "Second Admin";
    second.role = Role::Admin;
    second.password = "Password1";
    Id secondId = admin.createUser(*adminUser, second);
    auto secondUser = login("admin2", "Password1");
    EXPECT_NO_THROW(admin.setActive(*secondUser, adminId, false));   // another admin remains
    EXPECT_THROW(admin.setActive(*adminUser, secondId, false), BusinessRuleError);
}

TEST_F(SystemFixture, DepartmentAndCourseManagement) {
    EXPECT_THROW(admin.createDepartment(*adminUser, "Science"), ValidationError);  // duplicate
    EXPECT_THROW(admin.createDepartment(*adminUser, "  "), ValidationError);
    EXPECT_NO_THROW(admin.renameDepartment(*adminUser, deptId, "Natural Science"));
    EXPECT_THROW(admin.renameDepartment(*adminUser, deptId, "Humanities"), ValidationError);
    EXPECT_THROW(admin.renameDepartment(*adminUser, 999, "X"), NotFoundError);
    EXPECT_EQ(admin.listDepartments(*adminUser).size(), 2u);

    EXPECT_THROW(admin.createCourse(*adminUser, "CS101", "Dup", deptId), ValidationError);
    EXPECT_THROW(admin.createCourse(*adminUser, "CS102", "No dept", 999), NotFoundError);
    EXPECT_THROW(admin.createCourse(*adminUser, "", "Blank", deptId), ValidationError);
    Id c2 = admin.createCourse(*adminUser, "CS102", "Algorithms", deptId);
    EXPECT_NO_THROW(admin.updateCourse(*adminUser, c2, "CS102", "Algorithms II", deptId));
    EXPECT_THROW(admin.updateCourse(*adminUser, c2, "CS101", "Clash", deptId), ValidationError);
    EXPECT_THROW(admin.updateCourse(*adminUser, 999, "ZZ", "ZZ", deptId), NotFoundError);
    EXPECT_EQ(admin.listCourses(*adminUser).size(), 3u);
}

TEST_F(SystemFixture, LecturerAssignmentAndEnrolmentRules) {
    EXPECT_THROW(admin.assignLecturer(*adminUser, courseId, lecturerId), ValidationError);  // duplicate
    EXPECT_THROW(admin.assignLecturer(*adminUser, courseId, studentId), ValidationError);   // not a lecturer
    EXPECT_THROW(admin.assignLecturer(*adminUser, 999, lecturerId), NotFoundError);
    EXPECT_NO_THROW(admin.assignLecturer(*adminUser, courseId, lecturer2Id));

    EXPECT_THROW(admin.enrolStudent(*adminUser, courseId, studentId), ValidationError);     // duplicate
    EXPECT_THROW(admin.enrolStudent(*adminUser, courseId, lecturerId), ValidationError);    // not a student
    EXPECT_THROW(admin.enrolStudent(*adminUser, 999, studentId), NotFoundError);
    admin.setActive(*adminUser, student2Id, false);
    EXPECT_THROW(admin.enrolStudent(*adminUser, otherCourseId, student2Id), BusinessRuleError);  // deactivated
    admin.setActive(*adminUser, lecturer2Id, false);
    EXPECT_THROW(admin.assignLecturer(*adminUser, otherCourseId, lecturer2Id), BusinessRuleError);
    EXPECT_EQ(admin.studentsOf(*adminUser, courseId).size(), 2u);
}
