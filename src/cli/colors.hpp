#pragma once

#include <string>

#ifdef _WIN32
#include <io.h>
#define VIRL_ISATTY _isatty
#define VIRL_FILENO _fileno
#else
#include <unistd.h>
#define VIRL_ISATTY isatty
#define VIRL_FILENO fileno
#endif

#include <cstdio>

namespace virl::cli {

// Whether color codes should actually be emitted: true only when
// stdout is a real terminal, so piping/redirecting output (e.g.
// `./virl foo.virl > log.txt`) never embeds raw escape codes into a
// file. Overridden to false everywhere by --no-color.
inline bool stdoutIsTerminal() {
    return VIRL_ISATTY(VIRL_FILENO(stdout)) != 0;
}

// A tiny set of ANSI colorizing helpers. Each one returns the plain
// string unchanged when `enabled` is false, so call sites never need
// an if/else -- just `pal.red(text)` either way.
class Palette {
public:
    explicit Palette(bool enabled) : enabled_(enabled) {}

    std::string reset()  const { return code("\033[0m"); }
    std::string bold(const std::string& s)    const { return wrap(s, "\033[1m"); }
    std::string dim(const std::string& s)     const { return wrap(s, "\033[2m"); }
    std::string red(const std::string& s)     const { return wrap(s, "\033[31m"); }
    std::string green(const std::string& s)   const { return wrap(s, "\033[32m"); }
    std::string yellow(const std::string& s)  const { return wrap(s, "\033[33m"); }
    std::string blue(const std::string& s)    const { return wrap(s, "\033[34m"); }
    std::string magenta(const std::string& s) const { return wrap(s, "\033[35m"); }
    std::string cyan(const std::string& s)    const { return wrap(s, "\033[36m"); }
    std::string boldCyan(const std::string& s) const { return wrap(s, "\033[1;36m"); }

    bool enabled() const { return enabled_; }

private:
    bool enabled_;

    std::string code(const std::string& c) const { return enabled_ ? c : ""; }
    std::string wrap(const std::string& s, const std::string& c) const {
        if (!enabled_) return s;
        return c + s + "\033[0m";
    }
};

} // namespace virl::cli
