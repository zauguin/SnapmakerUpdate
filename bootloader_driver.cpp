#include "bootloader_interface.h"

#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

enum States {
  INIT,
  HAD_DEVICE,
  HAD_NO_DEVICE,
};

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main(int argc, char const* argv[]) try {
  if (argc < 4) {
    std::cerr << "Device path, version and firmware package required\n";
    return 1;
  }
  // Open the frmware file as soon as possible to detect errors before anything is happening
  std::ifstream firmware_file(argv[3]);
  if (!firmware_file)
    throw "Unable to open firmware file";
  auto serial = snapmaker::bootloader::trigger_bootloader(argv[1]);
  snapmaker::bootloader::announce(serial, argv[2]);
  snapmaker::bootloader::unlock_and_erase(serial);
  snapmaker::bootloader::send_file(serial, firmware_file);
  snapmaker::bootloader::boot_machine(serial);
  return 0;
} catch(const char *str) {
  std::cerr << str << '\n';
  return 1;
}
