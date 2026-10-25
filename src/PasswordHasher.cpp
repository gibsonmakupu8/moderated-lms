#include "lms/PasswordHasher.hpp"

#include <sodium.h>

#include "lms/Common.hpp"

namespace lms {

SodiumPasswordHasher::SodiumPasswordHasher(Cost cost) : cost_(cost) {
    if (sodium_init() < 0) throw LmsError("Cannot initialise the cryptography library");
}

std::string SodiumPasswordHasher::hash(const std::string& password) const {
    char out[crypto_pwhash_STRBYTES];
    const unsigned long long ops = cost_ == Cost::Interactive ? crypto_pwhash_OPSLIMIT_INTERACTIVE : crypto_pwhash_OPSLIMIT_MIN;
    const size_t mem = cost_ == Cost::Interactive ? crypto_pwhash_MEMLIMIT_INTERACTIVE : crypto_pwhash_MEMLIMIT_MIN;
    if (crypto_pwhash_str(out, password.c_str(), password.size(), ops, mem) != 0) {
        throw LmsError("Password hashing failed (out of memory)");
    }
    return std::string(out);
}

bool SodiumPasswordHasher::verify(const std::string& storedHash, const std::string& password) const {
    if (storedHash.empty() || storedHash.size() >= crypto_pwhash_STRBYTES) return false;
    return crypto_pwhash_str_verify(storedHash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace lms
