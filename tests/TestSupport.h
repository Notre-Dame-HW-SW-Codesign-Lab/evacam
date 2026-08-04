#ifndef EVACAM_TESTS_TESTSUPPORT_H_
#define EVACAM_TESTS_TESTSUPPORT_H_

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#include <unistd.h>

namespace TestSupport {

inline void Require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void AssertNear(
        double actual,
        double expected,
        double absoluteTolerance = 1e-12,
        double relativeTolerance = 1e-12) {
    Require(absoluteTolerance >= 0, "absolute tolerance must be non-negative");
    Require(relativeTolerance >= 0, "relative tolerance must be non-negative");
    Require(std::isfinite(actual), "actual value must be finite");
    Require(std::isfinite(expected), "expected value must be finite");

    const double difference = std::fabs(actual - expected);
    const double scale = std::max(std::fabs(actual), std::fabs(expected));
    if (difference > absoluteTolerance + relativeTolerance * scale) {
        std::ostringstream message;
        message << "values differ: actual=" << actual << ", expected=" << expected
                << ", difference=" << difference;
        throw std::runtime_error(message.str());
    }
}

inline void AssertFiniteNonNegative(double value, const std::string &name) {
    Require(std::isfinite(value), name + " must be finite");
    Require(value >= 0, name + " must be non-negative");
}

inline void AssertFinitePositive(double value, const std::string &name) {
    Require(std::isfinite(value), name + " must be finite");
    Require(value > 0, name + " must be positive");
}

template <typename ExpectedException, typename Callable>
void AssertThrows(Callable &&callable, const std::string &messageSubstring) {
    static_assert(
            std::is_base_of<std::exception, ExpectedException>::value,
            "ExpectedException must derive from std::exception");

    try {
        std::invoke(std::forward<Callable>(callable));
    } catch (const ExpectedException &error) {
        const std::string message = error.what();
        Require(
                message.find(messageSubstring) != std::string::npos,
                "exception message did not contain '" + messageSubstring
                        + "': " + message);
        return;
    } catch (const std::exception &error) {
        throw std::runtime_error(
                std::string("unexpected exception type: ") + error.what());
    } catch (...) {
        throw std::runtime_error("unexpected non-standard exception type");
    }

    throw std::runtime_error("expected exception was not thrown");
}

class StreamCapture {
    public:
        explicit StreamCapture(std::ostream &stream)
            : stream_(stream), originalBuffer_(stream.rdbuf(buffer_.rdbuf())) {}

        StreamCapture(const StreamCapture&) = delete;
        StreamCapture& operator=(const StreamCapture&) = delete;

        ~StreamCapture() {
            Stop();
        }

        void Stop() {
            if (originalBuffer_ != nullptr) {
                stream_.rdbuf(originalBuffer_);
                originalBuffer_ = nullptr;
            }
        }

        std::string Text() const {
            return buffer_.str();
        }

    private:
        std::ostream &stream_;
        std::streambuf *originalBuffer_;
        std::ostringstream buffer_;
};

class TemporaryDirectory {
    public:
        explicit TemporaryDirectory(const std::string &prefix = "evacam-test") {
            const std::filesystem::path base = std::filesystem::temp_directory_path();
            for (unsigned int attempt = 0; attempt < 1000; attempt++) {
                const std::string name = prefix + "-" + std::to_string(getpid())
                        + "-" + std::to_string(attempt);
                const std::filesystem::path candidate = base / name;
                std::error_code error;
                if (std::filesystem::create_directory(candidate, error)) {
                    path_ = candidate;
                    return;
                }
                if (error && error != std::errc::file_exists) {
                    throw std::runtime_error(
                            "could not create temporary directory: " + error.message());
                }
            }
            throw std::runtime_error("could not allocate a unique temporary directory");
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        const std::filesystem::path &Path() const {
            return path_;
        }

        std::filesystem::path WriteFile(
                const std::filesystem::path &relativePath,
                const std::string &contents) const {
            Require(!relativePath.is_absolute(), "temporary file path must be relative");
            const std::filesystem::path outputPath = path_ / relativePath;
            std::filesystem::create_directories(outputPath.parent_path());
            std::ofstream output(outputPath);
            Require(output.is_open(), "could not open temporary file: " + outputPath.string());
            output << contents;
            Require(output.good(), "could not write temporary file: " + outputPath.string());
            return outputPath;
        }

    private:
        std::filesystem::path path_;
};

}  // namespace TestSupport

#endif  // EVACAM_TESTS_TESTSUPPORT_H_
