#include "lms/Models.hpp"

namespace lms {

std::string toString(AuditEvent event) {
    switch (event) {
        case AuditEvent::MarkCreated: return "MARK_CREATED";
        case AuditEvent::MarkChanged: return "MARK_CHANGED";
        case AuditEvent::SubmittedForModeration: return "SUBMITTED_FOR_MODERATION";
        case AuditEvent::Approved: return "APPROVED";
        case AuditEvent::Returned: return "RETURNED";
        case AuditEvent::Released: return "RELEASED";
    }
    throw ValidationError("Unknown audit event");
}

AuditEvent auditEventFromString(const std::string& text) {
    static const AuditEvent all[] = {AuditEvent::MarkCreated, AuditEvent::MarkChanged,
                                     AuditEvent::SubmittedForModeration, AuditEvent::Approved,
                                     AuditEvent::Returned, AuditEvent::Released};
    for (AuditEvent e : all) {
        if (toString(e) == text) return e;
    }
    throw ValidationError("Unknown audit event: " + text);
}

}  // namespace lms
