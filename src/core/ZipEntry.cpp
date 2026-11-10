#include "ZipEntry.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <type_traits>
#include <unzip.h>

namespace ro {

std::vector<unsigned char> ReadZipEntry(const std::string& archivePath, const std::string& entryName)
{
    const std::unique_ptr<std::remove_pointer_t<unzFile>, decltype(&unzClose)>
        archive(unzOpen64(archivePath.c_str()), &unzClose);
    if (!archive) return {};

    std::string name = entryName;
    std::replace(name.begin(), name.end(), '\\', '/');
    if (unzLocateFile(archive.get(), name.c_str(), 2) != UNZ_OK) {
        std::replace(name.begin(), name.end(), '/', '\\');
        if (unzLocateFile(archive.get(), name.c_str(), 2) != UNZ_OK) return {};
    }

    unz_file_info64 entry{};
    if (unzGetCurrentFileInfo64(archive.get(), &entry, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK
        || entry.uncompressed_size == 0 || entry.uncompressed_size > std::numeric_limits<int>::max()
        || unzOpenCurrentFile(archive.get()) != UNZ_OK)
        return {};

    const auto size = static_cast<std::size_t>(entry.uncompressed_size);
    std::vector<unsigned char> bytes;
    std::array<unsigned char, 16384> chunk;
    int count = 0;
    while ((count = unzReadCurrentFile(archive.get(), chunk.data(), static_cast<unsigned>(chunk.size()))) > 0) {
        if (static_cast<std::size_t>(count) > size - bytes.size()) break;
        bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
    }
    const int closed = unzCloseCurrentFile(archive.get());
    if (count != 0 || closed != UNZ_OK || bytes.size() != size) return {};
    return bytes;
}

}
