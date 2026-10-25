// Presentation layer: role-based console menus. Contains no SQL and no business rules; it only
// collects input, calls the services and shows results or error messages.
#pragma once

#include <memory>
#include <string>

#include "lms/AdminService.hpp"
#include "lms/AssessmentService.hpp"
#include "lms/AuthService.hpp"
#include "lms/Console.hpp"
#include "lms/CourseworkService.hpp"

namespace lms {

class App {
public:
    App(Console& io, AuthService& auth, AdminService& admin, CourseworkService& coursework,
        AssessmentService& assessment, std::string logPath);
    void run();  // returns when the user exits (or input closes)

private:
    void firstRunSetup();
    std::unique_ptr<User> loginScreen();
    void adminMenu(const User& me);
    void lecturerMenu(const User& me);
    void hodMenu(const User& me);
    void studentMenu(const User& me);

    // helpers
    template <typename F> void guarded(F&& action);
    void changePassword(const User& me);
    void listUsers(const User& me, std::optional<Role> role);
    void listCourses(const User& me);
    void printSubmissions(const std::vector<SubmissionView>& list, bool showMarks);
    void printAudit(const User& me, Id submissionId);
    void markFlow(const User& me);
    void moderateFlow(const User& me);
    void log(const std::string& message) const;
    static std::string statusText(SubmissionStatus status);

    Console& io_;
    AuthService& auth_;
    AdminService& admin_;
    CourseworkService& coursework_;
    AssessmentService& assessment_;
    std::string logPath_;
};

}  // namespace lms
