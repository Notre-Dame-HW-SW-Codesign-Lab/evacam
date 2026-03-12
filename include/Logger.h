#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <utility>

class Logger {
    public:
        class Line {
            public:
                explicit Line(std::ostream *stream) : stream_(stream) {}

                Line(const Line&) = delete;
                Line& operator=(const Line&) = delete;

                Line(Line&& other) noexcept : stream_(other.stream_) {
                    other.stream_ = nullptr;
                }

                ~Line() {
                    if (stream_)
                        *stream_ << std::endl;
                }

                template <typename T>
                Line& operator<<(const T& value) {
                    if (stream_)
                        *stream_ << value;
                    return *this;
                }

                Line& operator<<(std::ostream& (*manip)(std::ostream&)) {
                    if (stream_)
                        *stream_ << manip;
                    return *this;
                }

                Line& operator<<(std::ios_base& (*manip)(std::ios_base&)) {
                    if (stream_)
                        *stream_ << manip;
                    return *this;
                }

            private:
                std::ostream *stream_;
        };

        explicit Logger(bool enabled = false) : enabled_(enabled) {}

        void SetVerbose(bool enabled) {
            enabled_ = enabled;
        }

        bool IsVerbose() const {
            return enabled_;
        }

        Line Verbose() const {
            return Line(enabled_ ? &std::cout : nullptr);
        }

    private:
        bool enabled_;
};

#endif /* LOGGER_H_ */
