#include "config/OutputFileLock.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "TestSupport.h"

namespace {

void TestAcquireCreatesParentAndDestructorReleasesLock() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock");
    const std::filesystem::path outputPath = temporary.Path() / "nested" / "result.yaml";
    const std::filesystem::path lockPath = outputPath.string() + ".lock";

    {
        OutputFileLock lock = OutputFileLock::Acquire(outputPath.string());
        assert(lock.IsHeld());
        assert(std::filesystem::is_directory(outputPath.parent_path()));
        assert(std::filesystem::is_directory(lockPath));
    }

    assert(!std::filesystem::exists(lockPath));
}

void TestAcquireRejectsContentionAndPreservesOwnerLock() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock-contention");
    const std::filesystem::path outputPath = temporary.Path() / "result.yaml";
    const std::filesystem::path lockPath = outputPath.string() + ".lock";
    OutputFileLock owner = OutputFileLock::Acquire(outputPath.string());

    TestSupport::AssertThrows<std::runtime_error>(
            [&outputPath]() { (void)OutputFileLock::Acquire(outputPath.string()); },
            "already reserved");

    assert(owner.IsHeld());
    assert(std::filesystem::is_directory(lockPath));
}

void TestMoveConstructionTransfersOwnership() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock-move-construct");
    const std::filesystem::path outputPath = temporary.Path() / "result.yaml";
    const std::filesystem::path lockPath = outputPath.string() + ".lock";

    OutputFileLock source = OutputFileLock::Acquire(outputPath.string());
    {
        OutputFileLock destination(std::move(source));
        assert(!source.IsHeld());
        assert(destination.IsHeld());
        assert(std::filesystem::is_directory(lockPath));
    }
    assert(!std::filesystem::exists(lockPath));
}

void TestMoveAssignmentReleasesOldLockAndTransfersNewLock() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock-move-assign");
    const std::filesystem::path firstOutput = temporary.Path() / "first.yaml";
    const std::filesystem::path secondOutput = temporary.Path() / "second.yaml";
    const std::filesystem::path firstLock = firstOutput.string() + ".lock";
    const std::filesystem::path secondLock = secondOutput.string() + ".lock";

    OutputFileLock destination = OutputFileLock::Acquire(firstOutput.string());
    OutputFileLock source = OutputFileLock::Acquire(secondOutput.string());
    destination = std::move(source);

    assert(destination.IsHeld());
    assert(!source.IsHeld());
    assert(!std::filesystem::exists(firstLock));
    assert(std::filesystem::is_directory(secondLock));

}

void TestPathConstructorOwnsAndReleasesAnExistingLockDirectory() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock-path");
    const std::filesystem::path lockPath = temporary.Path() / "owned.lock";
    std::filesystem::create_directory(lockPath);

    {
        OutputFileLock lock(lockPath);
        assert(lock.IsHeld());
    }

    assert(!std::filesystem::exists(lockPath));
}

void TestAcquireRejectsAStaleLockFileWithoutRemovingIt() {
    TestSupport::TemporaryDirectory temporary("evacam-output-lock-stale");
    const std::filesystem::path outputPath = temporary.Path() / "result.yaml";
    const std::filesystem::path lockPath = temporary.WriteFile("result.yaml.lock", "stale");

    TestSupport::AssertThrows<std::filesystem::filesystem_error>(
            [&outputPath]() { (void)OutputFileLock::Acquire(outputPath.string()); },
            "cannot create directory");

    assert(std::filesystem::is_regular_file(lockPath));
}

}  // namespace

int main() {
    TestAcquireCreatesParentAndDestructorReleasesLock();
    TestAcquireRejectsContentionAndPreservesOwnerLock();
    TestMoveConstructionTransfersOwnership();
    TestMoveAssignmentReleasesOldLockAndTransfersNewLock();
    TestPathConstructorOwnsAndReleasesAnExistingLockDirectory();
    TestAcquireRejectsAStaleLockFileWithoutRemovingIt();
    std::cout << "Output file lock tests passed" << std::endl;
    return 0;
}
