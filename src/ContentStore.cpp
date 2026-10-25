#include "lms/ContentStore.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

#include "lms/Validation.hpp"

namespace fs = std::filesystem;

namespace lms {

FileContentStore::FileContentStore(std::string directory) : directory_(std::move(directory)) {}

std::string FileContentStore::store(const std::string& sourcePath, Id studentId, Id assignmentId, int attemptNo) {
    static const std::set<std::string> allowed = {".pdf", ".doc", ".docx", ".odt", ".txt", ".rtf", ".zip",
                                                  ".xls", ".xlsx", ".ppt", ".pptx", ".cpp", ".h",  ".hpp",
                                                  ".c",   ".py",   ".java", ".png",  ".jpg", ".jpeg"};
    std::error_code ec;
    fs::path source(trim(sourcePath));
    if (source.empty() || !fs::is_regular_file(source, ec)) throw ValidationError("The file '" + sourcePath + "' was not found.");
    const auto size = fs::file_size(source, ec);
    if (ec || size == 0) throw ValidationError("The file is empty or cannot be read.");
    if (size > kMaxBytes) throw ValidationError("The file is larger than 10 MB.");
    std::string ext = toLower(source.extension().string());
    if (!allowed.count(ext)) throw ValidationError("Unsupported file type '" + ext + "'. Use PDF, Word, text, spreadsheet, slides, ZIP, source code or image files.");

    fs::create_directories(directory_, ec);
    if (ec) throw LmsError("Cannot create the submissions folder: " + ec.message());
    // The stored name is generated from ids, never from user text (prevents path tricks).
    fs::path target = fs::path(directory_) / ("a" + std::to_string(assignmentId) + "_s" + std::to_string(studentId) +
                                              "_attempt" + std::to_string(attemptNo) + ext);
    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
    if (ec) throw LmsError("Cannot save the submitted file: " + ec.message());
    return target.string();
}

void FileContentStore::discard(const std::string& storedPath) noexcept {
    std::error_code ec;
    fs::remove(fs::path(storedPath), ec);
}

}  // namespace lms
