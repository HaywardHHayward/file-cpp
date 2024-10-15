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
#include "vle/Unicode.hpp"
#include "vle/unicode/Utf16Sequence.hpp"
#include "vle/unicode/Utf8Sequence.hpp"

static_assert(File::Vle<File::GbSequence, std::uint8_t>);
static_assert(File::Vle<File::Unicode::Utf8Sequence, std::uint8_t>);
static_assert(File::Vle<File::Unicode::Utf16Sequence, std::uint16_t>);

template<typename Point, File::Vle<Point> T>
void validateVle(bool& isValid, std::optional<T>& vleSequence, typename T::Point point) {
    if (vleSequence.has_value()) {
        T& sequence{vleSequence.value()};
        if (!sequence.isComplete() && !sequence.addPoint(point)) {
            isValid = false;
            return;
        }
        if (sequence.isComplete()) {
            if (!sequence.isValid()) {
                isValid = false;
            }
            vleSequence.reset();
            return;
        }
        return;
    }
    std::optional<T> possibleSequence{T::build(point)};
    if (possibleSequence.has_value()) {
        T sequence{possibleSequence.value()};
        if (!sequence.isComplete()) {
            vleSequence.emplace(sequence);
            return;
        }
        if (!sequence.isValid()) {
            isValid = false;
            return;
        }
        return;
    }
    isValid = false;
}

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
    using namespace File;
    bool isAscii{true}, isLatin1{true}, isUtf8{true}, isUtf16{true}, isGb{true};
    std::optional<Unicode::Utf8Sequence> utf8Sequence{std::nullopt};
    std::optional<Unicode::Utf16Sequence> utf16Sequence{std::nullopt};
    std::optional<GbSequence> gbSequence{std::nullopt};
    std::optional<Unicode::Endianness> endianness{std::nullopt};
    std::array<std::uint8_t, 2> byteBuffer{0, 0};
    std::uintmax_t bytesRead{0};
    std::uint8_t byte;
    fileReader.get(reinterpret_cast<char&>(byte));
    while (fileReader.good()) {
        bytesRead++;
        if (isAscii && !isByteAscii(byte)) {
            isAscii = false;
        }
        if (!isAscii && isUtf16) {
            byteBuffer[(bytesRead - 1) % 2] = byte;
            if (bytesRead % 2 == 0) {
                if (endianness.has_value()) {
                    const std::uint16_t point = [&] {
                        switch (endianness.value()) {
                            case Unicode::Endianness::bigEndian:
                                return byteBuffer[0] << 8 | byteBuffer[1];
                            case Unicode::Endianness::littleEndian:
                                return byteBuffer[1] << 8 | byteBuffer[0];
                        }
                        return 0;
                    }();
                    validateVle<std::uint16_t, Unicode::Utf16Sequence>(
                        isUtf16, utf16Sequence, point);
                } else {
                    const std::uint16_t bigEndian{
                        static_cast<std::uint16_t>(byteBuffer[0] << 8 | byteBuffer[1])
                    };
                    const std::uint16_t littleEndian{
                        static_cast<std::uint16_t>(byteBuffer[1] << 8 | byteBuffer[0])
                    };
                    if (bigEndian == 0xFEFF) {
                        endianness = Unicode::Endianness::bigEndian;
                    } else if (littleEndian == 0xFEFF) {
                        endianness = Unicode::Endianness::littleEndian;
                    } else {
                        isUtf16 = false;;
                    }
                }
            }
        }
        if (!isAscii && isUtf8) {
            validateVle<std::uint8_t, Unicode::Utf8Sequence>(isUtf8, utf8Sequence, byte);
        }
        if (!isAscii && isGb) {
            validateVle<std::uint8_t, GbSequence>(isGb, gbSequence, byte);
        }
        if (!isAscii && isLatin1 && !isByteLatin1(byte)) {
            isLatin1 = false;
        }
        if (!isAscii && !isUtf16 && !isUtf8 && !isGb && !isLatin1) {
            return FileType::data;
        }
        fileReader.get(reinterpret_cast<char&>(byte));
    }
    if (utf16Sequence.has_value()) {
        isUtf16 = false;
    }
    if (utf8Sequence.has_value()) {
        isUtf8 = false;
    }
    if (gbSequence.has_value()) {
        isGb = false;
    }
    if (isAscii) {
        return FileType::ascii;
    }
    if (isUtf16) {
        return FileType::utf16;
    }
    if (isUtf8) {
        return FileType::utf8;
    }
    if (isLatin1) {
        return FileType::latin1;
    }
    if (isGb) {
        return FileType::gb;
    }
    return FileType::data;
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

