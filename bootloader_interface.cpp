#include "bootloader_interface.h"

#include <functional>
#include "endian-helper.h"
#include <numeric>
#include <serial/serial.h>
#include <vector>
#include <array>
#include <cassert>
#include <chrono>
#include <thread>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace snapmaker::bootloader {

  namespace {
    struct Header {
      std::uint8_t magic0 = 0xAA;
      std::uint8_t magic1 = 0x55;
      std::uint16_t length;
      std::uint8_t reserved = 0;
      std::uint8_t length_check;
      std::uint16_t checksum;
      void set_length(std::uint16_t len) {
        length = htobe16(len);
        length_check = len ^ (len >> 8);
      }
      std::uint16_t get_length() {
        if (length_check != ((length & 0xff) ^ (length >> 8))) {
          std::cout << "ABC " << length << ' ' << (int)length_check << '\n';
          throw "length validation failed";
        }
        return be16toh(length);
      }
    };
    static_assert(sizeof(Header) == 8);

    std::uint16_t calc_checksum(std::span<const std::uint8_t> data) {
      std::uint32_t init = data.size() % 2 ? data.back() : 0;
      std::span<const std::uint16_t> evendata((const std::uint16_t*)data.data(), data.size()/2);
      std::uint32_t checksum = std::transform_reduce(evendata.begin(), evendata.end(), init, std::plus<>(), [](auto i) { return be16toh(i); });
      while (checksum >= 0x10000)
        checksum = (checksum >> 16) + (checksum & 0xffff);
      return htobe16(~checksum);
    }
    void send_message(serial::Serial &serial, std::span<const std::uint8_t> data) {
      Header header;
      header.set_length(data.size());
      header.checksum = calc_checksum(data);

      serial.write((const std::uint8_t*)&header, sizeof header);
      serial.write(data.data(), data.size());
    }

    std::vector<std::uint8_t> receive_message(serial::Serial &serial) {
      Header header;
      {
        std::uint8_t c = 0;
        do {
          do {
            if (!serial.read(&c, 1))
              throw "Snapmaker doesn't respond";
          } while (c == 0xAA);
          while (c == 0xAA)
            if (!serial.read(&c, 1))
              throw "Snapmaker doesn't respond";
        } while(c != 0x55);
      }
      serial.read((std::uint8_t*)&header.length, (sizeof header) - 2);
      auto length = header.get_length();
      std::vector<std::uint8_t> data(length);
      serial.read(data.data(), length);
      if (header.checksum != calc_checksum(data))
        throw "invalid checksum";
      return data;
    }
  }

  void keep_alive(serial::Serial &serial) {
    std::array<std::uint8_t, 2> data{0x07, 0x01};
    send_message(serial, data);
  }
  // Probably this is supposed to allow the Snapmaker to deny the update request. We don't interpret the response yet though,
  // so it doesn't seem very useful.
  void announce(serial::Serial &serial, std::string_view version) {
    std::vector<std::uint8_t> data(version.size() + 3);
    data[0] = 0xa9; // bootloader command
    data[1] = 0x04; // announce
    std::copy(version.begin(), version.end(), data.begin() + 2);
    send_message(serial, data);
    auto response = receive_message(serial);
    /* std::clog << "Response to announce received, length " << response.size() << '\n'; */
  }
  void unlock_and_erase(serial::Serial &serial) {
    std::array<std::uint8_t, 2> data{0xa9, 0x00};
    send_message(serial, data);
    receive_message(serial);
  }

  std::tuple<std::uint8_t *, std::uint16_t> BlockwiseSender::get_pointer() {
    return {&*iter, buffer.end() - iter};
  }
  void BlockwiseSender::commit(std::uint16_t count) {
    iter += count;
    assert(iter <= buffer.end());
    if (iter == buffer.end())
      send_block();
  }
  void BlockwiseSender::send_block() {
    *(std::uint16_t*)&buffer[2] = htobe16(count++);
    send_message(*serial, std::span(buffer.begin(), iter));
    iter = buffer.begin() + 4;
    receive_message(*serial);
    std::clog << '.';
  }
    
  void BlockwiseSender::send_file(std::istream &stream) {
    while(stream) {
      auto [ptr, count] = get_pointer();
      commit(stream.read((char*)ptr, count).gcount());
    }
  }

  void BlockwiseSender::send_buffer(std::span<const std::uint8_t> data) {
    while(auto size = data.size()) {
      auto [ptr, count] = get_pointer();
      size = std::min(size_t(count), size);
      std::copy(data.begin(), data.begin() + size, ptr);
      commit(size);
      data = data.subspan(size);
    }
  }

  void boot_machine(serial::Serial &serial) {
    std::array<std::uint8_t, 2> data{0xa9, 0x02};
    send_message(serial, data);
    receive_message(serial);
  }

  serial::Serial trigger_bootloader(const char *path) {
    serial::Serial serial{path, 115200, serial::Timeout::simpleTimeout(200)};
    std::array<std::uint8_t, 2> data{0xa9, 0x04};
    send_message(serial, data); // Compare controller version with the empty string...
                                //Used as a way to detect if the bootloader is running
    if (serial.read(3) == "\xAA\x55\x00"sv)
      /* return std::move(serial); */
      { serial.close(); return ::serial::Serial{path, 115200, serial::Timeout::simpleTimeout(10000)}; }

    // bootloader does not seem to be active yet. Let's try running "M997" next.
    serial.write("\n");
    serial.readlines();
    serial.write("M997\n");
    for (int i = 10; i; --i) {
      std::this_thread::sleep_for(100ms);
      keep_alive(serial); // Send something ASAP
    }
    serial.readlines();

    send_message(serial, data); // Compare controller version with the empty string...
                                //Used as a way to detect if the bootloader is running
    if (serial.read(3) == "\xAA\x55\x00"sv)
      /* return std::move(serial); */
      { serial.close(); return ::serial::Serial{path, 115200, serial::Timeout::simpleTimeout(10000)}; }

    serial.close();
    std::clog << "Please turn the Snapmaker off" << std::endl;
    // This is ugly, but I couldn't find another way which works cross-platform
    for(;;) {
      try {
        serial.open();
        serial.close();
      } catch(...) { break; }
    }
    std::this_thread::sleep_for(10s);
    std::clog << "Please turn the Snapmaker on" << std::endl;
    for(;;) {
      try {
        serial.open();
        break;
      } catch(...) { }
    }
    for (int i = 3; i; --i) {
      std::this_thread::sleep_for(50ms);
      keep_alive(serial); // Send something ASAP
    }
    send_message(serial, data); // Compare controller version with the empty string...
                                //Used as a way to detect if the bootloader is running
    if (serial.read(3) == "\xAA\x55\x00"sv)
      /* return std::move(serial); */
      { serial.close(); return ::serial::Serial{path, 115200, serial::Timeout::simpleTimeout(10000)}; }

    throw "Unable to enter bootloader";
  }
}
