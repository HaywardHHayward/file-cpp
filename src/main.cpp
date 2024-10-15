#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include "vle.hpp"
#include "vle/GbSequence.hpp"
#include "vle/unicode/Utf8Sequence.hpp"

static_assert(File::Vle<File::GbSequence, std::uint8_t>);
static_assert(File::Vle<File::Unicode::Utf8Sequence, std::uint8_t>);

enum class FileType {
    empty,
    ascii,
    latin1,
    utf8,
    utf16,
    gb,
    data
};

enum class FileError {
    metadataError,
    doesNotExist,
    invalidPerms,
    notRegularFile,
    unreadable,
};

using FileState = std::variant<FileType, FileError>;

void file(std::vector<char*>&& args);

std::optional<FileError> findMetadata(const std::filesystem::path& path) noexcept;

FileState classifyFile(std::ifstream&& fileReader);

int main(const int argc, char* argv[]) {
    File::Unicode::Utf8Sequence utf8 = File::Unicode::Utf8Sequence::build(0xE9).value();
    utf8.addPoint(0x80);
    utf8.addPoint(0x88);
    if (utf8.isValid()) {
        std::cout << "You win";
    }
    try {
        if (argc <= 1) {
            throw std::invalid_argument("Invalid number of arguments.");
        }
        std::vector<char*> arguments{};
        arguments.assign(argv + 1, argv + argc);
        file(std::move(arguments));
    } catch (std::exception& e) {
        std::cerr << e.what() << "Usage: file [files]" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void file(std::vector<char*>&& args) {
    std::ranges::sort(args);
    auto last = std::ranges::unique(args, [](const char* a, const char* b) {
        return std::filesystem::weakly_canonical(a) == std::filesystem::weakly_canonical(b);
    });
    args.erase(last.begin(), args.end());
    std::map<std::filesystem::path, FileState> fileStates{};
    std::mutex fileStateMutex{};
    std::vector<std::thread> threads{};
    threads.reserve(args.size());
    for (char*& arg: args) {
        threads.emplace_back([&arg, &fileStateMutex, &fileStates] {
            const std::filesystem::path path{arg, std::filesystem::path::generic_format};
            std::optional possibleError{findMetadata(path)};
            if (possibleError.has_value()) {
                std::lock_guard guard{fileStateMutex};
                fileStates.emplace(path, *possibleError);
                return;
            }
            const std::uintmax_t fileSize{file_size(path)};
            if (fileSize == 0) {
                std::lock_guard guard{fileStateMutex};
                fileStates.emplace(path, FileType::empty);
                return;
            }
            std::ifstream fileBuffer{path, std::ios::binary};
            if (!fileBuffer.is_open()) {
                std::lock_guard guard{fileStateMutex};
                fileStates.emplace(path, FileError::unreadable);
                return;
            }
            FileState fileState{classifyFile(std::move(fileBuffer))};
            std::lock_guard guard{fileStateMutex};
            fileStates.emplace(path, fileState);
        });
    }
    for (std::thread& thread: threads) {
        thread.join();
    }
    for (auto [path, result]: fileStates) {
        std::string message;
        if (std::holds_alternative<FileType>(result)) {
            switch (std::get<FileType>(result)) {
                case FileType::empty:
                    message = "empty";
                    break;
                case FileType::ascii:
                    message = "ASCII text";
                    break;
                case FileType::latin1:
                    message = "ISO-8859-1 text";
                    break;
                case FileType::utf8:
                    message = "UTF-8 text";
                    break;
                case FileType::utf16:
                    message = "UTF-16 text";
                    break;
                case FileType::gb:
                    message = "GB 18030 text";
                    break;
                case FileType::data:
                    message = "data";
                    break;
            }
        } else {
            switch (std::get<FileError>(result)) {
                case FileError::metadataError:
                    message = "Was unable to check status of file";
                    break;
                case FileError::doesNotExist:
                    message = "File does not exist";
                    break;
                case FileError::invalidPerms:
                    message = "Invalid permissions";
                    break;
                case FileError::notRegularFile:
                    message = "File is not a regular file";
                    break;
                case FileError::unreadable:
                    message = "Lacked read permissions";
            }
        }
        std::cout << path.generic_string() << ": " << message << "\n";
    }
    std::flush(std::cout);
}

constexpr bool isByteAscii(const std::uint8_t byte) {
    return (0x08 <= byte && byte <= 0x0D) || (byte == 0x1B) | (0x20 <= byte && byte <= 0x7E);
}

constexpr bool isByteLatin1(const std::uint8_t byte) {
    return isByteAscii(byte) || byte >= 0xA0;
}

FileState classifyFile(std::ifstream&& fileReader) {
    //todo
    return FileType::empty;
}


std::optional<FileError> findMetadata(const std::filesystem::path& path) noexcept {
    namespace fs = std::filesystem;
    std::error_code ec{};
    const fs::file_status metadata{status(path, ec)};
    if (!status_known(metadata)) {
        return std::make_optional(FileError::metadataError);
    }
    if (!exists(metadata)) {
        return std::make_optional(FileError::doesNotExist);
    }
    if (metadata.type() == fs::file_type::unknown) {
        return std::make_optional(FileError::invalidPerms);
    }
    if (!is_regular_file(metadata)) {
        return std::make_optional(FileError::notRegularFile);
    }
    if ((metadata.permissions() & (fs::perms::owner_read | fs::perms::group_read |
                                   fs::perms::others_read)) ==
        fs::perms::none) {
        return std::make_optional(FileError::unreadable);
    }
    return std::nullopt;
}

