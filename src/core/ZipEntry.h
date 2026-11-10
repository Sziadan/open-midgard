#pragma once

#include <string>
#include <vector>

namespace ro {

std::vector<unsigned char> ReadZipEntry(const std::string& archivePath, const std::string& entryName);

}
