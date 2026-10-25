// Plain data classes that map one-to-one onto database tables (SDD section 6.6).
// An Id of 0 means "not saved yet".
#pragma once

#include <optional>
#include <string>

#include "lms/Common.hpp"

namespace lms {

struct UserRecord {
    Id id = 0;
    std::string username;
    std::string fullName;
    Role role = Role::Student;
    std::string passwordHash;  // salted Argon2id hash, never the password
    std::optional<Id> departmentId;
    bool active = true;
};

struct Department {
    Id id = 0;
    std::string name;
};

struct Course {
    Id id = 0;
    std::string code;
    std::string title;
    Id departmentId = 0;
};

struct Enrolment {
    Id id = 0;
    Id courseId = 0;
    Id studentId = 0;
};

struct Assignment {
    Id id = 0;
    Id courseId = 0;
    std::string title;
    std::string description;
    std::string deadline;  // "YYYY-MM-DD HH:MM:SS"
    double maxMark = 0;
    Id createdBy = 0;
    std::string createdAt;
};

struct Extension {
    Id id = 0;
    Id assignmentId = 0;
    Id studentId = 0;
    std::string newDeadline;
    std::string reason;
    Id grantedBy = 0;
    std::string grantedAt;
};

struct Submission {
    Id id = 0;
    Id assignmentId = 0;
    Id studentId = 0;
    std::string submittedAt;
    SubmissionStatus status = SubmissionStatus::Submitted;
    std::string contentPath;
    int attemptNo = 1;
    int daysLate = 0;
};

struct Mark {
    Id id = 0;
    Id submissionId = 0;
    double rawMark = 0;
    double penaltyPercent = 0;  // percentage of the assignment's maximum mark deducted
    double finalMark = 0;
    std::string feedback;
    Id markedBy = 0;
    std::string markedAt;
    ModerationStatus moderationStatus = ModerationStatus::Pending;
    std::optional<Id> moderatedBy;
    std::optional<std::string> moderatedAt;
    std::optional<std::string> moderationComment;
    std::optional<Id> releasedBy;
    std::optional<std::string> releasedAt;
};

enum class AuditEvent { MarkCreated, MarkChanged, SubmittedForModeration, Approved, Returned, Released };

struct AuditEntry {
    Id id = 0;
    Id markId = 0;
    Id changedBy = 0;
    std::string changedAt;
    AuditEvent event = AuditEvent::MarkCreated;
    std::optional<std::string> oldValue;
    std::optional<std::string> newValue;
    std::string reason;
};

std::string toString(AuditEvent event);
AuditEvent auditEventFromString(const std::string& text);

}  // namespace lms
