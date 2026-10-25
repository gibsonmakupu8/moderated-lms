#include "lms/LatePenalty.hpp"

#include <algorithm>

#include "lms/TimeUtil.hpp"

namespace lms {

double ILatePenaltyStrategy::apply(double rawMark, double maxMark, int daysLate) const {
    const double deduction = maxMark * penaltyPercent(daysLate) / 100.0;
    return std::max(0.0, rawMark - deduction);
}

int DailyTenPercentPenalty::daysLate(const std::string& deadline, const std::string& submittedAt) const {
    const long long late = parseTimestamp(submittedAt) - parseTimestamp(deadline);
    if (late <= 0) return 0;
    return static_cast<int>((late + 86399) / 86400);  // ceiling to whole days
}

double DailyTenPercentPenalty::penaltyPercent(int daysLate) const {
    if (daysLate <= 0) return 0.0;
    if (daysLate > kMaxPenalisedDays) return 100.0;  // more than 3 days late: zero
    return 10.0 * daysLate;
}

bool SubmissionStateMachine::canTransition(SubmissionStatus from, SubmissionStatus to) {
    using S = SubmissionStatus;
    switch (from) {
        case S::Draft: return to == S::Submitted || to == S::Late;
        case S::Submitted:
        case S::Late: return to == S::Marked;
        case S::Marked: return to == S::Marked || to == S::AwaitingModeration;
        case S::AwaitingModeration: return to == S::Moderated || to == S::ReturnedForCorrection;
        case S::ReturnedForCorrection: return to == S::Marked;
        case S::Moderated: return to == S::Released;
        case S::Released: return false;
    }
    return false;
}

void SubmissionStateMachine::require(SubmissionStatus from, SubmissionStatus to) {
    if (!canTransition(from, to)) {
        throw BusinessRuleError("A submission that is '" + displayName(from) + "' cannot become '" +
                                displayName(to) + "'.");
    }
}

}  // namespace lms
