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

fs::path wait_for_snapmaker(fs::path path) {
  States state = INIT;
  for(;;) {
    auto status = fs::status(path);
    if (fs::is_character_file(status)) {
      switch (state) {
        case INIT:
          state = HAD_DEVICE;
          std::cerr << "Please turn the Snapmaker off.\n";
          break;
        case HAD_NO_DEVICE:
          std::this_thread::sleep_for(200ms);
          return path;
        case HAD_DEVICE: ;
      }
    } else if (fs::exists(status)) {
      std::cerr << "The provided device path does not refer to a device file.\n";
      std::exit(1);
    } else {
      switch (state) {
        case HAD_DEVICE:
          std::this_thread::sleep_for(10s);
          [[fallthrough]];
        case INIT:
          std::cerr << "Please turn the Snapmaker on.\n";
          state = HAD_NO_DEVICE;
        case HAD_NO_DEVICE: ;
      }
    }
    std::this_thread::yield();
  }
}

int main(int argc, char const* argv[]) try {
  if (argc < 4) {
    std::cerr << "Device path, version and firmware package required\n";
    return 1;
  }
  // Open the frmware file as soon as possible to detect errors before anything is happening
  std::ifstream firmware_file(argv[3]);
  if (!firmware_file)
    throw "Unable to open firmware file";
  serial::Serial device{wait_for_snapmaker(argv[1]), 115200, serial::Timeout::simpleTimeout(10000)};
  for(int i = 0; i != 10; ++i) {
    snapmaker::bootloader::keep_alive(device);
    std::this_thread::sleep_for(100ms);
  }
  snapmaker::bootloader::announce(device, argv[2]);
  snapmaker::bootloader::unlock_and_erase(device);
  snapmaker::bootloader::send_file(device, firmware_file);
  snapmaker::bootloader::boot_machine(device);
  return 0;
} catch(const char *str) {
  std::cerr << str << '\n';
  return 1;
}
