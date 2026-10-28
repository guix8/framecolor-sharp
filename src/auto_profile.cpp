#include "auto_profile.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace {

bool validIndex(int index) { return index >= -1 && index <= 5; }

}  // namespace

AutoRules loadAutoRules(const std::filesystem::path& path) {
    AutoRules rules;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string kind;
        if (!(row >> kind)) continue;
        if (kind == "app") {
            // "app <index> <key>": the key is the rest of the line, so it may contain spaces.
            int index = -1;
            std::string key;
            if (!(row >> index) || !validIndex(index) || index < 0) continue;
            std::getline(row >> std::ws, key);
            while (!key.empty() && (key.back() == '\r' || key.back() == ' ')) key.pop_back();
            if (!key.empty()) rules.apps[key] = index;
        } else {
            int index = -1;
            if (!(row >> index) || !validIndex(index)) continue;
            if (kind == "standalone") rules.standalone = index;
            else if (kind == "streaming") rules.streaming = index;
        }
    }
    return rules;
}

bool saveAutoRules(const std::filesystem::path& path, const AutoRules& rules) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << "standalone " << rules.standalone << '\n' << "streaming " << rules.streaming << '\n';
        for (const auto& [key, index] : rules.apps) out << "app " << index << ' ' << key << '\n';
        out.close();
        if (!out) {
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

int resolveAutoPreset(const AutoRules& rules, const DisplayContext& context) {
    if (!context.appKey.empty()) {
        const auto it = rules.apps.find(context.appKey);
        if (it != rules.apps.end()) return it->second;
    }
    return context.streaming ? rules.streaming : rules.standalone;
}

bool isStreaming(const std::filesystem::path& procRoot) {
    std::error_code ec;
    for (std::filesystem::directory_iterator it(procRoot, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.empty() || name[0] < '0' || name[0] > '9') continue;
        std::ifstream cmdline(it->path() / "cmdline", std::ios::binary);
        std::string arg0;
        if (!std::getline(cmdline, arg0, '\0')) continue;
        const auto slash = arg0.rfind('/');
        if ((slash == std::string::npos ? arg0 : arg0.substr(slash + 1)) == "vrlink") return true;
    }
    return false;
}

int nextPresetChoice(int current, const bool customValid[3]) {
    for (int next = current + 1; next <= 5; ++next) {
        if (next < 3 || customValid[next - 3]) return next;
    }
    return -1;
}
