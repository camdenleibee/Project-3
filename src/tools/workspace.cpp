#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        if (user_path.empty()) {
            *reason = "empty path";
            return false;
        }

        // A NUL byte could make the OS read only part of the path.
        if (user_path.find('\0') != std::string_view::npos) {
            *reason = "path contains a NUL byte";
            return false;
        }

        fs::path p{std::string(user_path)};

        // The path must be relative to the workspace.
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }

        // Simplify things like "./", extra slashes, and "a/..".
        fs::path normal = p.lexically_normal();

        // A leading ".." means the path goes above the workspace.
        if (normal.begin() != normal.end() &&
            *normal.begin() == "..") {
            *reason = "path escapes the workspace";
            return false;
        }

        if (normal == ".") {
            *out = root();
        } else {
            *out = root() / normal;
        }

        reason->clear();
        return true;
    } catch (...) {
        // Report path errors without letting an exception escape.
        *reason = "could not process path";
        return false;
    }
}