#pragma once

#include <serial/serial.h>

#include <iostream>
#include <string_view>
#include <span>
#include <array>
#include <tuple>

#include <cstdint>

namespace snapmaker::bootloader {
  class BlockwiseSender {
    public:
      BlockwiseSender(serial::Serial &serial): serial(&serial) {}
      BlockwiseSender(const BlockwiseSender&) = delete;
      ~BlockwiseSender() { if (iter != buffer.begin() + 4) send_block(); }
      void send_file(std::istream&);
      void send_buffer(std::span<const std::uint8_t>);
    private:
      std::tuple<std::uint8_t *, std::uint16_t> get_pointer();
      void commit(std::uint16_t count);
      void send_block();

      serial::Serial *serial;
      std::array<std::uint8_t, 516> buffer {{0xa9, 0x01}};
      decltype(buffer)::iterator iter = buffer.begin() + 4;
      std::uint16_t count = 0;
  };
  void keep_alive(serial::Serial &serial);
  void announce(serial::Serial &serial, std::string_view version);
  void unlock_and_erase(serial::Serial &serial);
  inline void send_file(serial::Serial &serial, std::istream &stream) {
    BlockwiseSender{serial}.send_file(stream);
  }
  inline void send_buffer(serial::Serial &serial, std::span<const std::uint8_t> data) {
    BlockwiseSender{serial}.send_buffer(data);
  }
  void boot_machine(serial::Serial &serial);
}
