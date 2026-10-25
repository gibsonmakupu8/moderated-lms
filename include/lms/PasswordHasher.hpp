// Password hashing with libsodium (Argon2id, random per-password salt embedded in the hash string).
#pragma once

#include <string>

namespace lms {

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;
    virtual std::string hash(const std::string& password) const = 0;
    virtual bool verify(const std::string& storedHash, const std::string& password) const = 0;
};

class SodiumPasswordHasher : public IPasswordHasher {
public:
    // Interactive: libsodium's recommended interactive limits (about 64 MiB, well within an old school PC).
    // Fast: minimum limits, ONLY for automated tests and demo-data generation.
    enum class Cost { Interactive, Fast };

    explicit SodiumPasswordHasher(Cost cost = Cost::Interactive);
    std::string hash(const std::string& password) const override;
    bool verify(const std::string& storedHash, const std::string& password) const override;

private:
    Cost cost_;
};

}  // namespace lms
