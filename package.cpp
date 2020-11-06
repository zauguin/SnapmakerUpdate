#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>
#include <string_view>

#include "endian-helper.h"

using namespace std::literals;

auto read_file(const char *filename) {
  std::vector<char> buffer(0x1000);
  std::ifstream file(filename);
  while(file.read(buffer.data() + (buffer.size() - 0x1000), 0x1000))
    buffer.resize(buffer.size() + 0x1000);
  buffer.resize(buffer.size() - 0x1000 + file.gcount());
  return buffer;
}

int main(int argc, char const* argv[])
{
  std::uint32_t flags;
  if (argv[1] && argv[1] == "--flag"sv) { // I have absolutely no idea what this flag is doing...
    flags = 1;
    ++argv; --argc;
  } else
    flags = 0;

  if (argc <= 2) {
    std::cerr << "Invalid usage. You need something like './package controller Snapmaker_Vx.y.z'\n";
    return 1;
  }
  char header[2048] = {};
  if (argv[1] == "0"sv || argv[1] == "controller"sv)
    header[0] = 0;
  else if (argv[1] == "1"sv || argv[1] == "module"sv)
    header[0] = 1;
  else {
    std::cerr << "Unsupported type\n";
    return 1;
  }
  std::string_view version = argv[2];
  if (!version.starts_with("Snapmaker_"))
    std::cerr << "Warning: Your specified version does not start with \"Snapmaker_\".\n"
                 "This can lead to issues with the Snapmaker interface. I will continue\n"
                 "but I recommend choosing another version name\n";
  if(version.size() > 32) {
    std::cerr << "Version too long (should have at most 32 bytes)\n";
    return 1;
  }
  std::copy(version.begin(), version.end(), header + 5);
  std::string arg3, arg4;
  if(argv[3]) {
    arg3 = argv[3];
    arg4 = argv[argv[4] ? 4 : 3];
  } else { // Default to the numbers used in the official updates
    arg3 = "0"; arg4 = "20";
  }
  *reinterpret_cast<std::uint16_t*>(header + 1) = htobe16(std::stoul(arg3));
  *reinterpret_cast<std::uint16_t*>(header + 3) = htobe16(std::stoul(arg4));

  std::ostringstream sstr;
  sstr << std::cin.rdbuf();
  auto content = sstr.str();
  *reinterpret_cast<std::uint32_t*>(header + 40) = htole32(content.size()); // No, this does not have to be in big endian. Yes, I appreciate the consistency too...
  *reinterpret_cast<std::uint32_t*>(header + 44) = htole32(std::accumulate((std::uint8_t*)content.data(), (std::uint8_t*)content.data() + content.size(), std::uint32_t(0)));
  *reinterpret_cast<std::uint32_t*>(header + 48) = htole32(flags);

  std::cout.write(header, sizeof header);
  std::cout << content;
  return 0;
}
