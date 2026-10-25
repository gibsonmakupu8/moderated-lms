#include "lms/Common.hpp"

namespace lms {

std::string toString(Role role) {
    switch (role) {
        case Role::Admin: return "ADMIN";
        case Role::Lecturer: return "LECTURER";
        case Role::HoD: return "HOD";
        case Role::Student: return "STUDENT";
    }
    throw ValidationError("Unknown role");
}

Role roleFromString(const std::string& text) {
    if (text == "ADMIN") return Role::Admin;
    if (text == "LECTURER") return Role::Lecturer;
    if (text == "HOD") return Role::HoD;
    if (text == "STUDENT") return Role::Student;
    throw ValidationError("Unknown role: " + text);
}

std::string toString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Draft: return "DRAFT";
        case SubmissionStatus::Submitted: return "SUBMITTED";
        case SubmissionStatus::Late: return "LATE";
        case SubmissionStatus::Marked: return "MARKED";
        case SubmissionStatus::AwaitingModeration: return "AWAITING_MODERATION";
        case SubmissionStatus::ReturnedForCorrection: return "RETURNED_FOR_CORRECTION";
        case SubmissionStatus::Moderated: return "MODERATED";
        case SubmissionStatus::Released: return "RELEASED";
    }
    throw ValidationError("Unknown submission status");
}

SubmissionStatus submissionStatusFromString(const std::string& text) {
    static const SubmissionStatus all[] = {
        SubmissionStatus::Draft,  SubmissionStatus::Submitted,
        SubmissionStatus::Late,   SubmissionStatus::Marked,
        SubmissionStatus::AwaitingModeration, SubmissionStatus::ReturnedForCorrection,
        SubmissionStatus::Moderated, SubmissionStatus::Released};
    for (SubmissionStatus s : all) {
        if (toString(s) == text) return s;
    }
    throw ValidationError("Unknown submission status: " + text);
}

std::string toString(ModerationStatus status) {
    switch (status) {
        case ModerationStatus::Pending: return "PENDING";
        case ModerationStatus::Approved: return "APPROVED";
        case ModerationStatus::Returned: return "RETURNED";
    }
    throw ValidationError("Unknown moderation status");
}

ModerationStatus moderationStatusFromString(const std::string& text) {
    if (text == "PENDING") return ModerationStatus::Pending;
    if (text == "APPROVED") return ModerationStatus::Approved;
    if (text == "RETURNED") return ModerationStatus::Returned;
    throw ValidationError("Unknown moderation status: " + text);
}

std::string displayName(Role role) {
    switch (role) {
        case Role::Admin: return "System Administrator";
        case Role::Lecturer: return "Lecturer";
        case Role::HoD: return "Head of Department";
        case Role::Student: return "Student";
    }
    return "Unknown";
}

std::string displayName(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Draft: return "Draft";
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late: return "Late";
        case SubmissionStatus::Marked: return "Marked";
        case SubmissionStatus::AwaitingModeration: return "Awaiting moderation";
        case SubmissionStatus::ReturnedForCorrection: return "Returned for correction";
        case SubmissionStatus::Moderated: return "Moderated (approved)";
        case SubmissionStatus::Released: return "Released";
    }
    return "Unknown";
}

}  // namespace lms
