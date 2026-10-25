// Common types, enumerations and the exception hierarchy shared by every layer.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace lms {

using Id = std::int64_t;

enum class Role { Admin, Lecturer, HoD, Student };

// Life cycle of a submission (see State Machine Diagram, SDD section 5.1).
enum class SubmissionStatus {
    Draft,
    Submitted,
    Late,
    Marked,
    AwaitingModeration,
    ReturnedForCorrection,
    Moderated,
    Released
};

enum class ModerationStatus { Pending, Approved, Returned };

// Conversions to/from the exact strings stored in SQLite (CHECK constraints use the same values).
std::string toString(Role role);
Role roleFromString(const std::string& text);
std::string toString(SubmissionStatus status);
SubmissionStatus submissionStatusFromString(const std::string& text);
std::string toString(ModerationStatus status);
ModerationStatus moderationStatusFromString(const std::string& text);

// Human friendly names for menus.
std::string displayName(Role role);
std::string displayName(SubmissionStatus status);

// ---------------------------------------------------------------------------------------------
// Exceptions. The UI catches LmsError, so no invalid input or database failure terminates the
// program (project brief, Phase 4 requirement on exceptions and validation).
// ---------------------------------------------------------------------------------------------
class LmsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class ValidationError : public LmsError {  // invalid or missing user input
public:
    using LmsError::LmsError;
};
class AuthenticationError : public LmsError {  // bad credentials
public:
    using LmsError::LmsError;
};
class PermissionError : public LmsError {  // role or ownership check failed
public:
    using LmsError::LmsError;
};
class NotFoundError : public LmsError {  // referenced record does not exist
public:
    using LmsError::LmsError;
};
class BusinessRuleError : public LmsError {  // a rule such as "deadline passed" or "invalid status"
public:
    using LmsError::LmsError;
};
class DatabaseError : public LmsError {  // SQLite failure
public:
    using LmsError::LmsError;
};

}  // namespace lms
