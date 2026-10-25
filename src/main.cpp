// Composition root: builds the layers and starts the console application.
//   lms [--db FILE] [--storage DIR] [--log FILE]
#include <iostream>
#include <string>

#include "lms/AdminService.hpp"
#include "lms/App.hpp"
#include "lms/AssessmentService.hpp"
#include "lms/AuthService.hpp"
#include "lms/ContentStore.hpp"
#include "lms/CourseworkService.hpp"
#include "lms/Database.hpp"
#include "lms/LatePenalty.hpp"
#include "lms/PasswordHasher.hpp"
#include "lms/SqliteRepositories.hpp"
#include "lms/TimeUtil.hpp"

int main(int argc, char** argv) {
    using namespace lms;
    std::string dbPath = "lms.db", storage = "submissions", logPath = "lms.log";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: lms [--db FILE] [--storage DIR] [--log FILE]\n";
            return 0;
        }
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return 2;
        }
        std::string value = argv[++i];
        if (arg == "--db") dbPath = value;
        else if (arg == "--storage") storage = value;
        else if (arg == "--log") logPath = value;
        else {
            std::cerr << "Unknown option " << arg << "\n";
            return 2;
        }
    }
    try {
        Database db(dbPath);
        db.initialiseSchema();
        SqliteUserRepository users(db);
        SqliteDepartmentRepository departments(db);
        SqliteCourseRepository courses(db);
        SqliteEnrolmentRepository enrolments(db);
        SqliteAssignmentRepository assignments(db);
        SqliteExtensionRepository extensions(db);
        SqliteSubmissionRepository submissions(db);
        SqliteMarkRepository marks(db);
        SqliteAuditRepository audit(db);
        SodiumPasswordHasher hasher;
        SystemClock clock;
        DailyTenPercentPenalty penalty;  // Strategy: swap here to change the late policy
        FileContentStore content(storage);

        AuthService auth(users, hasher);
        AdminService admin(users, departments, courses, enrolments, hasher);
        CourseworkService coursework(courses, enrolments, assignments, extensions, submissions, content, penalty, clock, db);
        AssessmentService assessment(users, courses, assignments, submissions, marks, audit, penalty, clock, db);

        Console io(std::cin, std::cout, true);
        App app(io, auth, admin, coursework, assessment, logPath);
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal start-up error: " << e.what() << "\n";
        return 1;
    }
}
