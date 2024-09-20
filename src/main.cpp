#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <variant>
#include <thread>
#include <vector>

enum class FileType {
    empty,
    ascii,
    latin1,
    utf8,
    data
};

enum class ErrorType {
    doesNotExist,
    readError
};

struct Utf8Sequence {
    std::uint8_t length;
    std::vector<unsigned char> bytes;

    explicit Utf8Sequence(const unsigned char byte) {
        bytes.reserve(4);
        length = std::countl_one(byte);
        if (length == 1 || length > 4) {
            length = 0;
            return;
        }
        if (length == 0) {
            length = 1;
        }
        bytes.push_back(byte);
    }

    bool addByte(const unsigned char byte) {
        if (bytes.size() + 1 > length) {
            return false;
        }
        if (byte >= 0b11'000000) {
            return false;
        }
        bytes.push_back(byte);
        return true;
    }

    [[nodiscard]] std::uint32_t getCodepoint() const {
        std::uint32_t codepoint = bytes[0];
        switch (length) {
            case 2:
                codepoint ^= 0b110'00000;
                break;
            case 3:
                codepoint ^= 0b1110'0000;
                break;
            case 4:
                codepoint ^= 0b1111'0000;
                break;
            case 1:
                break;
            default:
                return 0x110000;
        }
        if (bytes.size() != length) {
            return 0x110000;
        }
        for (int i = 1; i < length; i++) {
            codepoint = codepoint << 6 | bytes[i] ^ 0b10'000000;
        }
        return codepoint;
    }

    [[nodiscard]] bool isValidCodepoint() const {
        const std::uint32_t codepoint = getCodepoint();
        switch (length) {
            case 1:
                return codepoint <= 0x7F;
            case 2:
                return codepoint > 0x7F && codepoint <= 0x7FF;
            case 3:
                return codepoint > 0x7FF && codepoint <= 0xFFFF;
            case 4:
                return codepoint > 0xFFFF && codepoint <= 0x10FFFF;
            default:
                return false;
        }
    }
};

using FileState = std::variant<FileType, ErrorType>;

FileType classifyFile(std::ifstream file);

int main(const int argc, char* argv[]) {
    if (argc <= 1) {
        std::cerr << "Invalid number of arguments. Usage: " << (argc == 1 ? argv[0] : "file") << " [file]" << std::endl;
        return EXIT_FAILURE;
    }
    std::vector<std::filesystem::path> filePaths;
    filePaths.reserve(argc - 1);
    std::unordered_map<std::string, FileState> fileStates;
    fileStates.reserve(argc - 1);
    for (int i = 1; i < argc; i++) {
        try {
            std::filesystem::path filePath{argv[i]};
            if (!is_regular_file(filePath)) {
                fileStates.insert({filePath.generic_string(), FileState{ErrorType::doesNotExist}});
                continue;
            }
            if (is_empty(filePath)) {
                fileStates.insert({filePath.generic_string(), FileState{FileType::empty}});
                continue;
            }
            filePaths.push_back(std::move(filePath));
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    std::vector<std::pair<const std::string, std::ifstream> > files;
    files.reserve(filePaths.size());
    for (const std::filesystem::path& filePath: filePaths) {
        std::ifstream file{filePath, std::ios::binary | std::ios::in};
        if (!file.is_open()) {
            fileStates.insert({filePath.generic_string(), FileState{ErrorType::readError}});
            continue;
        }
        files.emplace_back(filePath.generic_string(), std::move(file));
    }
    if (files.size() > 1) {
        std::mutex mapMutex;
        std::vector<std::thread> threads;
        threads.reserve(files.size());
        for (auto& [name, file]: files) {
            threads.emplace_back([&] {
                std::pair<const std::string, FileState> result{name, FileState{classifyFile(std::move(file))}};
                std::unique_lock lock{mapMutex};
                fileStates.insert(std::move(result));
            });
        }
        for (std::thread& thread: threads) {
            thread.join();
        }
    } else {
        for (auto& [name, file]: files) {
            fileStates.insert({name, FileState{classifyFile(std::move(file))}});
        }
    }
    for (auto& [path, fileState]: fileStates) {
        std::cout << path << ": ";
        if (std::holds_alternative<FileType>(fileState)) {
            switch (std::get<FileType>(fileState)) {
                case FileType::empty:
                    std::cout << "empty" << "\n";
                    break;
                case FileType::ascii:
                    std::cout << "ASCII text" << "\n";
                    break;
                case FileType::latin1:
                    std::cout << "ISO 8859-1 text" << "\n";
                    break;
                case FileType::utf8:
                    std::cout << "UTF-8 text" << "\n";
                    break;
                case FileType::data:
                    std::cout << "data" << "\n";
                    break;
            }
        } else {
            switch (std::get<ErrorType>(fileState)) {
                case ErrorType::doesNotExist:
                    std::cout << "does not exist" << "\n";
                    break;
                case ErrorType::readError:
                    std::cout << "read error" << "\n";
                    break;
            }
        }
    }
    std::cout.flush();
    return EXIT_SUCCESS;
}


constexpr bool isByteAscii(const unsigned char byte) {
    return (byte >= 0x07 && byte <= 0x0D) || byte == 0x1B || (byte >= 0x20 && byte <= 0x7E);
}

constexpr bool isByteLatin1(const unsigned char byte) {
    return isByteAscii(byte) || byte >= 0xA0;
}

FileType classifyFile(std::ifstream file) {
    bool isAscii = true, isLatin1 = true, isUtf8 = true;
    std::optional<Utf8Sequence> sequence = std::nullopt;
    unsigned char byte;
    file.get(reinterpret_cast<char&>(byte));
    while (file.good()) {
        if (isAscii) {
            if (!isByteAscii(byte)) {
                isAscii = false;
            }
        }
        if (isLatin1) {
            if (!isByteLatin1(byte)) {
                isLatin1 = false;
            }
        }
        if (isUtf8) {
            if (!sequence.has_value()) {
                sequence = Utf8Sequence(byte);
                if (sequence->length == 0) {
                    isUtf8 = false;
                }
            } else if (sequence->bytes.size() < sequence->length) {
                if (!sequence->addByte(byte)) {
                    isUtf8 = false;
                }
            }
            if (isUtf8 && sequence->bytes.size() == sequence->length) {
                if (!sequence->isValidCodepoint()) {
                    isUtf8 = false;
                } if (sequence->length == 1 && !isByteAscii(sequence->bytes[0])) {
                    isUtf8 = false;
                }
                sequence = std::nullopt;
            }
        }
        if (!isAscii && !isLatin1 && !isUtf8) {
            return FileType::data;
        }
        file.get(reinterpret_cast<char&>(byte));
    }
    if (isUtf8 && sequence.has_value()) {
        // indicates incomplete utf8
        isUtf8 = false;
    }
    if (isAscii) {
        return FileType::ascii;
    }
    if (isUtf8) {
        return FileType::utf8;
    }
    if (isLatin1) {
        return FileType::latin1;
    }
    return FileType::data;
}
