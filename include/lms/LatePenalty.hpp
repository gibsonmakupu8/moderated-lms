// Late-penalty rules (Strategy pattern, SDD 4.2) and the submission life-cycle rules (SDD 5.1).
#pragma once

#include <string>

#include "lms/Common.hpp"

namespace lms {

// Strategy interface: how many days late is a submission, and what share of the assignment's
// available mark is deducted? A different school policy is added by writing another subclass.
class ILatePenaltyStrategy {
public:
    virtual ~ILatePenaltyStrategy() = default;
    // Whole days late; 0 when on time (submittedAt <= deadline).
    virtual int daysLate(const std::string& deadline, const std::string& submittedAt) const = 0;
    // Percentage (0..100) of the MAXIMUM mark that is deducted for that many days late.
    virtual double penaltyPercent(int daysLate) const = 0;
    // Mark after the penalty has been applied; never below zero.
    double apply(double rawMark, double maxMark, int daysLate) const;
};

// Head of Department's rule: 10% of the available mark per day late for up to 3 days, then zero.
// A "day" is a completed or started 24-hour period after the (possibly extended) deadline, so
// 1 second to 24 hours late = day 1, ..., 72 hours + 1 second = day 4 (zero).
class DailyTenPercentPenalty : public ILatePenaltyStrategy {
public:
    static constexpr int kMaxPenalisedDays = 3;
    int daysLate(const std::string& deadline, const std::string& submittedAt) const override;
    double penaltyPercent(int daysLate) const override;
};

// Allowed submission status changes. Anything else is a BusinessRuleError.
class SubmissionStateMachine {
public:
    static bool canTransition(SubmissionStatus from, SubmissionStatus to);
    static void require(SubmissionStatus from, SubmissionStatus to);
};

}  // namespace lms
