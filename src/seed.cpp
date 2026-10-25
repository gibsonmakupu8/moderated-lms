// Demo/performance data generator:  lms_seed [--db FILE] [--students N]
// Creates departments, courses, staff and N students (default 800). DEMO ONLY: passwords use the
// fast hashing cost and every demo account has the password "Demo1234".
#include <iostream>
#include <string>

#include "lms/AdminService.hpp"
#include "lms/Database.hpp"
#include "lms/PasswordHasher.hpp"
#include "lms/SqliteRepositories.hpp"

int main(int argc, char** argv) {
    using namespace lms;
    std::string dbPath = "demo.db";
    int students = 800;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string arg = argv[i];
        if (arg == "--db") dbPath = argv[i + 1];
        else if (arg == "--students") students = std::stoi(argv[i + 1]);
    }
    try {
        Database db(dbPath);
        db.initialiseSchema();
        SqliteUserRepository users(db);
        SqliteDepartmentRepository departments(db);
        SqliteCourseRepository courses(db);
        SqliteEnrolmentRepository enrolments(db);
        SodiumPasswordHasher hasher(SodiumPasswordHasher::Cost::Fast);
        AdminService admin(users, departments, courses, enrolments, hasher);
        if (!admin.needsBootstrap()) {
            std::cerr << "Database already contains data; use a new file.\n";
            return 1;
        }
        admin.bootstrapAdmin("admin", "Demo Administrator", "Demo1234");
        auto adminUser = UserFactory::create(*users.findByUsername("admin"));
        Id sci = admin.createDepartment(*adminUser, "Science");
        Id hum = admin.createDepartment(*adminUser, "Humanities");
        Id cs = admin.createCourse(*adminUser, "CS101", "Introduction to Computing", sci);
        admin.createCourse(*adminUser, "HIS101", "World History", hum);
        auto add = [&](const std::string& u, const std::string& n, Role r, std::optional<Id> d) {
            NewUser nu{u, n, r, "Demo1234", d};
            return admin.createUser(*adminUser, nu);
        };
        Id lecturer = add("lecturer", "Demo Lecturer", Role::Lecturer, std::nullopt);
        add("hod", "Demo Head of Science", Role::HoD, sci);
        admin.assignLecturer(*adminUser, cs, lecturer);
        db.runInTransaction([&] {
            for (int i = 1; i <= students; ++i) {
                Id s = add("student" + std::to_string(i), "Student " + std::to_string(i), Role::Student, std::nullopt);
                admin.enrolStudent(*adminUser, cs, s);
            }
        });
        std::cout << "Created demo data in " << dbPath << " (accounts: admin, lecturer, hod, student1.." << students << "; password Demo1234)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Seed failed: " << e.what() << "\n";
        return 1;
    }
}
