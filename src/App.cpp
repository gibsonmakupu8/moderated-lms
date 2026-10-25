#include "lms/App.hpp"

#include <cctype>
#include <fstream>

#include "lms/TimeUtil.hpp"
#include "lms/Validation.hpp"

namespace lms {

App::App(Console& io, AuthService& auth, AdminService& admin, CourseworkService& coursework,
         AssessmentService& assessment, std::string logPath)
    : io_(io), auth_(auth), admin_(admin), coursework_(coursework), assessment_(assessment), logPath_(std::move(logPath)) {}

void App::log(const std::string& message) const {
    std::ofstream out(logPath_, std::ios::app);  // never log passwords or hashes
    if (out) out << SystemClock().nowString() << "  " << message << "\n";
}

// Runs one menu action; no failure may terminate the program (brief, Phase 4).
template <typename F>
void App::guarded(F&& action) {
    try {
        action();
    } catch (const DatabaseError& e) {
        log(e.what());
        io_.error("A database problem occurred and nothing was changed. Details were written to " + logPath_ + ".");
    } catch (const LmsError& e) {
        io_.error(e.what());
    } catch (const std::exception& e) {
        log(std::string("Unexpected: ") + e.what());
        io_.error("An unexpected problem occurred. Details were written to " + logPath_ + ".");
    }
}

std::string App::statusText(SubmissionStatus status) { return displayName(status); }

void App::run() {
    try {
        io_.say("Moderated Learning Management System v1.0");
        firstRunSetup();
        for (;;) {
            int choice = io_.menu("Welcome", {"Log in", "Exit"});
            if (choice == 0) return;
            if (auto user = loginScreen()) {
                io_.say("Welcome, " + user->fullName() + " (" + user->roleName() + ").");
                switch (user->role()) {
                    case Role::Admin: adminMenu(*user); break;
                    case Role::Lecturer: lecturerMenu(*user); break;
                    case Role::HoD: hodMenu(*user); break;
                    case Role::Student: studentMenu(*user); break;
                }
                io_.say("You have been logged out.");
            }
        }
    } catch (const InputClosed&) {
        io_.say("\nInput closed. Goodbye.");
    }
}

void App::firstRunSetup() {
    if (!admin_.needsBootstrap()) return;
    io_.heading("First-time set-up: create the administrator account");
    for (;;) {
        bool done = false;
        guarded([&] {
            std::string username = io_.requiredLine("Administrator username: ");
            std::string fullName = io_.requiredLine("Full name: ");
            std::string password = io_.password("Password (8+ characters, letters and digits): ");
            if (password != io_.password("Repeat password: ")) throw ValidationError("The passwords do not match.");
            admin_.bootstrapAdmin(username, fullName, password);
            io_.say("Administrator account created.");
            done = true;
        });
        if (done) return;
    }
}

std::unique_ptr<User> App::loginScreen() {
    for (int attempt = 1; attempt <= 5; ++attempt) {
        std::string username = io_.line("Username: ");
        std::string password = io_.password("Password: ");
        try {
            return auth_.login(username, password);
        } catch (const AuthenticationError& e) {
            io_.error(e.what());
        } catch (const DatabaseError& e) {
            log(e.what());
            io_.error("A database problem occurred.");
            return nullptr;
        }
    }
    io_.error("Too many failed attempts. Returning to the welcome screen.");
    return nullptr;
}

void App::changePassword(const User& me) {
    std::string oldPassword = io_.password("Current password: ");
    std::string newPassword = io_.password("New password: ");
    if (newPassword != io_.password("Repeat new password: ")) throw ValidationError("The passwords do not match.");
    auth_.changePassword(me, oldPassword, newPassword);
    io_.say("Password changed.");
}

void App::listUsers(const User& me, std::optional<Role> role) {
    io_.say(Console::pad("ID", 6) + Console::pad("Username", 18) + Console::pad("Name", 26) + Console::pad("Role", 22) + "Active");
    for (const auto& u : admin_.listUsers(me, role)) {
        io_.say(Console::pad(std::to_string(u.id), 6) + Console::pad(u.username, 18) + Console::pad(u.fullName, 26) +
                Console::pad(displayName(u.role), 22) + (u.active ? "yes" : "NO"));
    }
}

void App::listCourses(const User& me) {
    io_.say(Console::pad("ID", 6) + Console::pad("Code", 12) + "Title");
    for (const auto& c : admin_.listCourses(me)) {
        io_.say(Console::pad(std::to_string(c.id), 6) + Console::pad(c.code, 12) + c.title + "  [dept " + std::to_string(c.departmentId) + "]");
    }
}

// ================================ administrator ==================================================
void App::adminMenu(const User& me) {
    for (;;) {
        int c = io_.menu("Administrator", {"List users", "Create user", "Edit user", "Activate/deactivate user", "Reset a user's password",
                                           "List departments", "Create department", "Rename department", "List courses", "Create course",
                                           "Edit course", "Assign lecturer to course", "Enrol student in course", "List students of a course",
                                           "Change my password", "Log out"});
        if (c == 0) return;
        guarded([&] {
            switch (c) {
                case 1: listUsers(me, std::nullopt); break;
                case 2: {
                    NewUser nu;
                    nu.username = io_.requiredLine("Username: ");
                    nu.fullName = io_.requiredLine("Full name: ");
                    int r = io_.menu("Role", {"Administrator", "Lecturer", "Head of Department", "Student", "Cancel"});
                    if (r == 0) break;
                    nu.role = r == 1 ? Role::Admin : r == 2 ? Role::Lecturer : r == 3 ? Role::HoD : Role::Student;
                    if (nu.role == Role::HoD || nu.role == Role::Lecturer || nu.role == Role::Student) {
                        std::string dept = io_.line("Department id (blank for none): ");
                        if (!dept.empty()) nu.departmentId = std::stoll(dept);
                    }
                    nu.password = io_.password("Initial password: ");
                    admin_.createUser(me, nu);
                    io_.say("User created.");
                    break;
                }
                case 3: {
                    Id id = io_.integer("User id: ", 1, 1000000000);
                    std::string name = io_.requiredLine("New full name: ");
                    std::string dept = io_.line("Department id (blank for none): ");
                    admin_.updateUser(me, id, name, dept.empty() ? std::nullopt : std::optional<Id>(std::stoll(dept)));
                    io_.say("User updated.");
                    break;
                }
                case 4: {
                    Id id = io_.integer("User id: ", 1, 1000000000);
                    bool active = io_.yesNo("Activate this account? (n = deactivate)");
                    admin_.setActive(me, id, active);
                    io_.say(active ? "Account activated." : "Account deactivated.");
                    break;
                }
                case 5: {
                    Id id = io_.integer("User id: ", 1, 1000000000);
                    admin_.resetPassword(me, id, io_.password("New password: "));
                    io_.say("Password reset.");
                    break;
                }
                case 6:
                    for (const auto& d : admin_.listDepartments(me)) io_.say(Console::pad(std::to_string(d.id), 6) + d.name);
                    break;
                case 7: admin_.createDepartment(me, io_.requiredLine("Department name: ")); io_.say("Department created."); break;
                case 8: {
                    Id id = io_.integer("Department id: ", 1, 1000000000);
                    admin_.renameDepartment(me, id, io_.requiredLine("New name: "));
                    io_.say("Department renamed.");
                    break;
                }
                case 9: listCourses(me); break;
                case 10: {
                    std::string code = io_.requiredLine("Course code: ");
                    std::string title = io_.requiredLine("Course title: ");
                    Id dept = io_.integer("Department id: ", 1, 1000000000);
                    admin_.createCourse(me, code, title, dept);
                    io_.say("Course created.");
                    break;
                }
                case 11: {
                    Id id = io_.integer("Course id: ", 1, 1000000000);
                    std::string code = io_.requiredLine("Course code: ");
                    std::string title = io_.requiredLine("Course title: ");
                    Id dept = io_.integer("Department id: ", 1, 1000000000);
                    admin_.updateCourse(me, id, code, title, dept);
                    io_.say("Course updated.");
                    break;
                }
                case 12: {
                    Id course = io_.integer("Course id: ", 1, 1000000000);
                    Id lecturer = io_.integer("Lecturer user id: ", 1, 1000000000);
                    admin_.assignLecturer(me, course, lecturer);
                    io_.say("Lecturer assigned.");
                    break;
                }
                case 13: {
                    Id course = io_.integer("Course id: ", 1, 1000000000);
                    Id student = io_.integer("Student user id: ", 1, 1000000000);
                    admin_.enrolStudent(me, course, student);
                    io_.say("Student enrolled.");
                    break;
                }
                case 14: {
                    Id course = io_.integer("Course id: ", 1, 1000000000);
                    for (const auto& s : admin_.studentsOf(me, course)) io_.say(Console::pad(std::to_string(s.id), 6) + Console::pad(s.username, 18) + s.fullName);
                    break;
                }
                case 15: changePassword(me); break;
                default: break;
            }
        });
    }
}

// ================================ shared printing ================================================
void App::printSubmissions(const std::vector<SubmissionView>& list, bool showMarks) {
    if (list.empty()) {
        io_.say("(nothing to show)");
        return;
    }
    io_.say(Console::pad("ID", 6) + Console::pad("Student", 22) + Console::pad("Course", 9) + Console::pad("Assignment", 20) + Console::pad("Submitted", 20) +
            Console::pad("Att", 4) + Console::pad("Late", 5) + Console::pad("Status", 24) + (showMarks ? "Mark" : ""));
    for (const auto& v : list) {
        std::string mark = "-";
        if (showMarks && v.mark) mark = formatMark(v.mark->finalMark) + "/" + formatMark(v.maxMark);
        io_.say(Console::pad(std::to_string(v.submission.id), 6) + Console::pad(v.studentName, 22) + Console::pad(v.courseCode, 9) +
                Console::pad(v.assignmentTitle, 20) + Console::pad(v.submission.submittedAt, 20) + Console::pad(std::to_string(v.submission.attemptNo), 4) +
                Console::pad(v.submission.daysLate ? std::to_string(v.submission.daysLate) + "d" : "-", 5) +
                Console::pad(statusText(v.submission.status), 24) + (showMarks ? mark : ""));
    }
}

void App::printAudit(const User& me, Id submissionId) {
    auto trail = assessment_.auditTrail(me, submissionId);
    if (trail.empty()) io_.say("(no audit entries)");
    for (const auto& a : trail) {
        io_.say(a.entry.changedAt + "  " + Console::pad(toString(a.entry.event), 26) + Console::pad(a.actorName, 20) + " old: " +
                a.entry.oldValue.value_or("-") + " | new: " + a.entry.newValue.value_or("-") + " | reason: " + a.entry.reason);
    }
}

// ================================ lecturer =======================================================
void App::markFlow(const User& me) {
    for (const auto& c : coursework_.coursesFor(me)) io_.say(Console::pad(std::to_string(c.id), 6) + Console::pad(c.code, 10) + c.title);
    Id courseId = io_.integer("Course id: ", 1, 1000000000);
    SubmissionFilter filter;
    std::string statusText = io_.line("Filter by status (blank = all; e.g. SUBMITTED, LATE, MARKED): ");
    if (!statusText.empty()) {
        for (auto& ch : statusText) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        filter.status = submissionStatusFromString(statusText);
    }
    filter.keyword = io_.line("Search by student/assignment (blank = all): ");
    auto list = assessment_.submissionsForCourse(me, courseId, filter);
    printSubmissions(list, true);
    if (list.empty()) return;
    Id submissionId = io_.integer("Submission id to mark (0 = back): ", 0, 1000000000);
    if (submissionId == 0) return;
    double mark = io_.number("Mark: ");
    std::string feedback = io_.requiredLine("Written feedback: ");
    std::string reason = io_.line("Reason (required when changing an existing mark): ");
    assessment_.recordMark(me, submissionId, mark, feedback, reason);
    io_.say("Mark saved and recorded in the audit log. It stays hidden from the student.");
    if (io_.yesNo("Send this result to the Head of Department for moderation now?")) {
        assessment_.submitForModeration(me, submissionId);
        io_.say("Sent for moderation.");
    }
}

void App::lecturerMenu(const User& me) {
    for (;;) {
        int c = io_.menu("Lecturer", {"My courses", "Create assignment", "View assignments", "Grant individual extension", "View and mark submissions",
                                      "Send a marked submission for moderation", "View audit trail of a submission", "Change my password", "Log out"});
        if (c == 0) return;
        guarded([&] {
            switch (c) {
                case 1:
                    for (const auto& co : coursework_.coursesFor(me)) io_.say(Console::pad(std::to_string(co.id), 6) + Console::pad(co.code, 10) + co.title);
                    break;
                case 2: {
                    for (const auto& co : coursework_.coursesFor(me)) io_.say(Console::pad(std::to_string(co.id), 6) + Console::pad(co.code, 10) + co.title);
                    Id course = io_.integer("Course id: ", 1, 1000000000);
                    std::string title = io_.requiredLine("Title: ");
                    std::string description = io_.line("Description: ");
                    std::string deadline = io_.requiredLine("Deadline (YYYY-MM-DD HH:MM): ");
                    double max = io_.number("Maximum mark: ");
                    coursework_.createAssignment(me, course, title, description, deadline, max);
                    io_.say("Assignment created.");
                    break;
                }
                case 3: {
                    io_.say(Console::pad("ID", 6) + Console::pad("Course", 10) + Console::pad("Title", 26) + Console::pad("Deadline", 21) + "Max");
                    for (const auto& v : coursework_.assignmentsFor(me, io_.line("Search (blank = all): "))) {
                        io_.say(Console::pad(std::to_string(v.assignment.id), 6) + Console::pad(v.courseCode, 10) + Console::pad(v.assignment.title, 26) +
                                Console::pad(v.assignment.deadline, 21) + formatMark(v.assignment.maxMark));
                    }
                    break;
                }
                case 4: {
                    Id a = io_.integer("Assignment id: ", 1, 1000000000);
                    Id s = io_.integer("Student user id: ", 1, 1000000000);
                    std::string deadline = io_.requiredLine("New deadline (YYYY-MM-DD HH:MM): ");
                    std::string reason = io_.requiredLine("Reason: ");
                    coursework_.grantExtension(me, a, s, deadline, reason);
                    io_.say("Extension recorded.");
                    break;
                }
                case 5: markFlow(me); break;
                case 6: {
                    Id s = io_.integer("Submission id: ", 1, 1000000000);
                    assessment_.submitForModeration(me, s);
                    io_.say("Sent for moderation.");
                    break;
                }
                case 7: printAudit(me, io_.integer("Submission id: ", 1, 1000000000)); break;
                case 8: changePassword(me); break;
                default: break;
            }
        });
    }
}

// ================================ head of department =============================================
void App::moderateFlow(const User& me) {
    auto queue = assessment_.moderationQueue(me, SubmissionFilter{std::nullopt, io_.line("Search (blank = all): ")});
    printSubmissions(queue, true);
    if (queue.empty()) return;
    Id id = io_.integer("Submission id to review (0 = back): ", 0, 1000000000);
    if (id == 0) return;
    for (const auto& v : queue) {
        if (v.submission.id == id && v.mark) {
            io_.say("Raw mark " + formatMark(v.mark->rawMark) + ", late penalty " + formatMark(v.mark->penaltyPercent) + "% of " + formatMark(v.maxMark) +
                    ", final " + formatMark(v.mark->finalMark));
            io_.say("Feedback: " + v.mark->feedback);
        }
    }
    printAudit(me, id);
    int d = io_.menu("Decision", {"Approve", "Return for correction", "Adjust mark (then decide)", "Cancel"});
    if (d == 0) return;
    if (d == 1) {
        assessment_.approve(me, id, io_.line("Comment (optional): "));
        io_.say("Approved. Use 'Release approved results' to publish it to the student.");
    } else if (d == 2) {
        assessment_.returnForCorrection(me, id, io_.requiredLine("Reason for returning: "));
        io_.say("Returned to the lecturer; the student cannot see it.");
    } else {
        double mark = io_.number("New raw mark: ");
        assessment_.adjustMark(me, id, mark, io_.requiredLine("Reason for the adjustment: "));
        io_.say("Mark adjusted and audited. Review it again to approve.");
    }
}

void App::hodMenu(const User& me) {
    for (;;) {
        int c = io_.menu("Head of Department", {"Moderation queue (review, approve, return, adjust)", "Release approved results", "Department courses",
                                                 "Department assignments", "View audit trail of a submission", "Change my password", "Log out"});
        if (c == 0) return;
        guarded([&] {
            switch (c) {
                case 1: moderateFlow(me); break;
                case 2: {
                    auto ready = assessment_.approvedAwaitingRelease(me);
                    printSubmissions(ready, true);
                    if (ready.empty()) break;
                    if (io_.yesNo("Release ALL approved results above to students?")) {
                        io_.say(std::to_string(assessment_.releaseAllApproved(me)) + " result(s) released.");
                    } else {
                        Id id = io_.integer("Submission id to release (0 = cancel): ", 0, 1000000000);
                        if (id != 0) {
                            assessment_.release(me, id);
                            io_.say("Released.");
                        }
                    }
                    break;
                }
                case 3:
                    for (const auto& co : coursework_.coursesFor(me)) io_.say(Console::pad(std::to_string(co.id), 6) + Console::pad(co.code, 10) + co.title);
                    break;
                case 4:
                    for (const auto& v : coursework_.assignmentsFor(me, "")) {
                        io_.say(Console::pad(std::to_string(v.assignment.id), 6) + Console::pad(v.courseCode, 10) + Console::pad(v.assignment.title, 26) + v.assignment.deadline);
                    }
                    break;
                case 5: printAudit(me, io_.integer("Submission id: ", 1, 1000000000)); break;
                case 6: changePassword(me); break;
                default: break;
            }
        });
    }
}

// ================================ student ========================================================
void App::studentMenu(const User& me) {
    for (;;) {
        int c = io_.menu("Student", {"My courses", "My assignments", "Submit or resubmit an assignment", "My submissions and status", "View a released result",
                                     "Change my password", "Log out"});
        if (c == 0) return;
        guarded([&] {
            switch (c) {
                case 1:
                    for (const auto& co : coursework_.coursesFor(me)) io_.say(Console::pad(std::to_string(co.id), 6) + Console::pad(co.code, 10) + co.title);
                    break;
                case 2: {
                    io_.say(Console::pad("ID", 6) + Console::pad("Course", 10) + Console::pad("Title", 26) + Console::pad("Deadline", 21) + Console::pad("Max", 8) + "Your status");
                    for (const auto& v : coursework_.assignmentsFor(me, io_.line("Search (blank = all): "))) {
                        std::string st = v.submissionStatus ? (*v.submissionStatus == SubmissionStatus::Released ? "Result released" : *v.submissionStatus == SubmissionStatus::Late ? "Submitted (late)" : "Submitted") : "Not submitted";
                        io_.say(Console::pad(std::to_string(v.assignment.id), 6) + Console::pad(v.courseCode, 10) + Console::pad(v.assignment.title, 26) +
                                Console::pad(v.assignment.deadline, 21) + Console::pad(formatMark(v.assignment.maxMark), 8) + st);
                    }
                    break;
                }
                case 3: {
                    Id a = io_.integer("Assignment id: ", 1, 1000000000);
                    Assignment assignment = coursework_.requireAssignment(a);
                    io_.say("Deadline that applies to you: " + coursework_.effectiveDeadline(assignment, me.id()));
                    std::string path = io_.requiredLine("Path of the file to submit: ");
                    auto r = coursework_.submit(me, a, path);
                    io_.say("Submission recorded (attempt " + std::to_string(r.attemptNo) + ") at " + r.submittedAt + ".");
                    if (r.daysLate > 0) {
                        io_.say("This work is " + std::to_string(r.daysLate) + " day(s) late. " + (r.penaltyPercent >= 100 ? "Work more than 3 days late receives zero." :
                                "A penalty of " + formatMark(r.penaltyPercent) + "% of the available mark will apply."));
                    }
                    break;
                }
                case 4: printSubmissions(assessment_.mySubmissions(me), true); break;
                case 5: {
                    Id id = io_.integer("Submission id: ", 1, 1000000000);
                    ResultView r = assessment_.releasedResult(me, id);
                    io_.say(r.courseCode + " - " + r.assignmentTitle);
                    io_.say("Mark: " + formatMark(r.finalMark) + " / " + formatMark(r.maxMark) +
                            (r.penaltyPercent > 0 ? "   (raw " + formatMark(r.rawMark) + ", late penalty " + formatMark(r.penaltyPercent) + "% of the available mark)" : ""));
                    io_.say("Feedback: " + r.feedback);
                    io_.say("Released: " + r.releasedAt);
                    break;
                }
                case 6: changePassword(me); break;
                default: break;
            }
        });
    }
}

}  // namespace lms
