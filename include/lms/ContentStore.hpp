// Managed local storage for submitted files (the database keeps only the path - SDD 6.5).
#pragma once

#include <string>

#include "lms/Common.hpp"

namespace lms {

class IContentStore {
public:
    virtual ~IContentStore() = default;
    // Validates and copies the student's file into managed storage; returns the stored path.
    virtual std::string store(const std::string& sourcePath, Id studentId, Id assignmentId, int attemptNo) = 0;
    // Removes a stored file (used to undo a store() when the database write fails).
    virtual void discard(const std::string& storedPath) noexcept = 0;
};

class FileContentStore : public IContentStore {
public:
    static constexpr unsigned long long kMaxBytes = 10ULL * 1024 * 1024;  // 10 MB per submission
    explicit FileContentStore(std::string directory);
    std::string store(const std::string& sourcePath, Id studentId, Id assignmentId, int attemptNo) override;
    void discard(const std::string& storedPath) noexcept override;

private:
    std::string directory_;
};

}  // namespace lms
