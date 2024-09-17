#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
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
    std::vector<std::ifstream> files;
    files.reserve(filePaths.size());
    for (const std::filesystem::path& filePath: filePaths) {
        std::ifstream file{filePath, std::ios::binary | std::ios::in};
        if (!file.is_open()) {
            fileStates.insert({filePath.generic_string(), FileState{ErrorType::readError}});
            continue;
        }
        fileStates.insert({filePath.generic_string(), FileState{classifyFile(std::move(file))}});
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

FileType classifyFile(std::ifstream file) {
    bool isAscii = true, isLatin1 = true, isUtf8 = true;
    while (!file.eof()) {
        unsigned char byte;
        file.get(reinterpret_cast<char&>(byte));
        if (isAscii) {
            if (byte > 0x7F) {
                isAscii = false;
            }
        }
        if (isLatin1) {
            if (byte > 0x7E && byte < 0xA0) {
                isLatin1 = false;
            }
        }
        if (isUtf8) {
            if (byte >= 0b11111'000) {
                isUtf8 = false;
            } else if (byte >= 0b1111'0000) {
                std::array<unsigned char, 3> bytes{};
                file.read(reinterpret_cast<char*>(bytes.data()), 3);
                if (file.gcount() < 3) {
                    isUtf8 = false;
                } else {
                    isUtf8 = std::ranges::none_of(bytes, [&](const unsigned char utfByte) {
                        return utfByte >= 0b11'000000;
                    });
                }
                if (!isUtf8) {
                    file.seekg(-file.gcount(), std::ios_base::cur);
                }
            } else if (byte >= 0b111'00000) {
                std::array<unsigned char, 2> bytes{};
                file.read(reinterpret_cast<char*>(bytes.data()), 2);
                if (file.gcount() < 2) {
                    isUtf8 = false;
                } else {
                    isUtf8 = std::ranges::none_of(bytes, [&](const unsigned char utfByte) {
                        return utfByte >= 0b11'000000;
                    });
                }
                if (!isUtf8) {
                    file.seekg(-file.gcount(), std::ios_base::cur);
                }
            } else if (byte >= 0b11'000000) {
                unsigned char byte2;
                file.get(reinterpret_cast<char&>(byte2));
                if (file.gcount() < 1 || byte2 > 0b11'000000) {
                    isUtf8 = false;
                }
                if (!isUtf8) {
                    file.seekg(-file.gcount(), std::ios_base::cur);
                }
            }
        }
        if (!isAscii && !isLatin1 && !isUtf8) {
            return FileType::data;
        }
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
