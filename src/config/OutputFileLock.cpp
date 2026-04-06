#include "config/OutputFileLock.h"

#include <stdexcept>

OutputFileLock::OutputFileLock(std::filesystem::path lockPath)
    : lockPath_(std::move(lockPath)) {
}

OutputFileLock::OutputFileLock(OutputFileLock&& other) noexcept
    : lockPath_(std::move(other.lockPath_)) {
    other.lockPath_.clear();
}

OutputFileLock& OutputFileLock::operator=(OutputFileLock&& other) noexcept {
    if (this != &other) {
        Release();
        lockPath_ = std::move(other.lockPath_);
        other.lockPath_.clear();
    }
    return *this;
}

OutputFileLock::~OutputFileLock() {
    Release();
}

OutputFileLock OutputFileLock::Acquire(const std::string &outputPath) {
    std::filesystem::path target(outputPath);
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path());
    }

    const std::filesystem::path lockPath = target.string() + ".lock";
    if (!std::filesystem::create_directory(lockPath)) {
        throw std::runtime_error("Output path is already reserved by another EvaCAM run: " + outputPath);
    }

    return OutputFileLock(lockPath);
}

bool OutputFileLock::IsHeld() const {
    return !lockPath_.empty();
}

void OutputFileLock::Release() noexcept {
    if (lockPath_.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove(lockPath_, ec);
    lockPath_.clear();
}
