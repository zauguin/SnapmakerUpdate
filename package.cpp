#include <ios>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>
#include <string_view>
#include <thread>
#include <chrono>

#include "endian-helper.h"
#ifdef HAS_SERIAL
#include "bootloader_interface.h"
#endif

using namespace std::literals;

int main(int argc, char const* argv[]) try {
  std::uint32_t flags;
  std::ifstream input;
  std::ofstream output;
  const char *flash_interface = nullptr;
  while(argv[1]) {
    std::string_view arg = argv[1];
    if (arg == "--flag"sv)
      flags = 1;
    else if (arg.starts_with("--flash=")) {
      arg.remove_prefix(sizeof("--flash=")-1);
      flash_interface = arg.data();
    } else if (arg.starts_with("--input=")) {
      arg.remove_prefix(sizeof("--input=")-1);
      input.open(arg.data(), std::ios_base::in | std::ios_base::binary);
      if (!input.is_open()) {
        std::cerr << "Unable to open input file\n";
        return 1;
      }
    } else if (arg.starts_with("--output=")) {
      arg.remove_prefix(sizeof("--output=")-1);
      output.open(arg.data(), std::ios_base::out | std::ios_base::binary);
      if (!output.is_open()) {
        std::cerr << "Unable to open output file\n";
        return 1;
      }
    } else break;
    ++argv; --argc;
  }

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
  sstr << (input.is_open() ? (input) : (std::cin)).rdbuf();
  auto content = sstr.str();
  *reinterpret_cast<std::uint32_t*>(header + 40) = htole32(content.size()); // No, this does not have to be in big endian. Yes, I appreciate the consistency too...
  *reinterpret_cast<std::uint32_t*>(header + 44) = htole32(std::accumulate((std::uint8_t*)content.data(), (std::uint8_t*)content.data() + content.size(), std::uint32_t(0)));
  *reinterpret_cast<std::uint32_t*>(header + 48) = htole32(flags);

  if (output.is_open() || !flash_interface) {
    auto &out = [&]() -> std::ostream& {
      if (output.is_open())
        return output;
      else
        return std::cout;
    }();
    out.write(header, sizeof header);
    out << content;
  }
  if (flash_interface) {
#ifdef HAS_SERIAL
    serial::Serial serial{flash_interface, 115200, serial::Timeout::simpleTimeout(10000)};
    for (int i = 0; i != 3; ++i) {
      using namespace std::chrono_literals;
      snapmaker::bootloader::keep_alive(serial);
      std::this_thread::sleep_for(100ms);
    }
    snapmaker::bootloader::announce(serial, version);
    snapmaker::bootloader::unlock_and_erase(serial);
    {
      snapmaker::bootloader::BlockwiseSender sender(serial);
      sender.send_buffer(std::span{(const std::uint8_t*)header, sizeof header});
      sender.send_buffer(std::span{(const std::uint8_t*)content.data(), content.size()});
    }
    snapmaker::bootloader::boot_machine(serial);
#else
    std::cerr << "The version has been compiled without flashing support.\n";
#endif
  }
  return 0;
} catch (const char *err) {
  std::cerr << err << '\n';
  return 1;
}
