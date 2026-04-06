#ifndef OUTPUTFILELOCK_H_
#define OUTPUTFILELOCK_H_

#include <filesystem>
#include <string>

class OutputFileLock {
    public:
        OutputFileLock() = default;
        explicit OutputFileLock(std::filesystem::path lockPath);
        OutputFileLock(const OutputFileLock&) = delete;
        OutputFileLock& operator=(const OutputFileLock&) = delete;
        OutputFileLock(OutputFileLock&& other) noexcept;
        OutputFileLock& operator=(OutputFileLock&& other) noexcept;
        ~OutputFileLock();

        static OutputFileLock Acquire(const std::string &outputPath);

        bool IsHeld() const;

    private:
        void Release() noexcept;

        std::filesystem::path lockPath_;
};

#endif /* OUTPUTFILELOCK_H_ */
