#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>

class Logger {
    public:
        static std::mutex& OutputMutex() {
            static std::mutex mutex;
            return mutex;
        }

        class Line {
            public:
                explicit Line(std::ostream *stream) : stream_(stream) {}

                Line(const Line&) = delete;
                Line& operator=(const Line&) = delete;

                Line(Line&& other) noexcept
                    : stream_(other.stream_), buffer_(std::move(other.buffer_)) {
                    other.stream_ = nullptr;
                }

                ~Line() {
                    if (!stream_)
                        return;

                    std::lock_guard<std::mutex> lock(Logger::OutputMutex());
                    *stream_ << buffer_.str() << std::endl;
                }

                template <typename T>
                Line& operator<<(const T& value) {
                    if (stream_)
                        buffer_ << value;
                    return *this;
                }

                Line& operator<<(std::ostream& (*manip)(std::ostream&)) {
                    if (stream_)
                        manip(buffer_);
                    return *this;
                }

                Line& operator<<(std::ios_base& (*manip)(std::ios_base&)) {
                    if (stream_)
                        manip(buffer_);
                    return *this;
                }

            private:
                std::ostream *stream_;
                std::ostringstream buffer_;
        };

        explicit Logger(bool enabled = false) : enabled_(enabled) {}

        void SetVerbose(bool enabled) {
            enabled_ = enabled;
        }

        bool IsVerbose() const {
            return enabled_;
        }

        void SetOutputEnabled(bool enabled) {
            outputEnabled_ = enabled;
        }

        bool IsOutputEnabled() const {
            return outputEnabled_;
        }

        Line Log() const {
            return Line(outputEnabled_ ? &std::cout : nullptr);
        }

        Line Verbose() const {
            return Line(enabled_ && outputEnabled_ ? &std::cout : nullptr);
        }

    private:
        bool enabled_;
        bool outputEnabled_ = true;
};

#endif /* LOGGER_H_ */
