#include <cstdint>
#include <sstream>
#include <vector>
#include <array>
#include <span>
#include <string_view>
#include <iostream>
#include <fstream>
#include <optional>

#if __has_include(<format>)
#include <format>
constexpr auto operator ""_format(const char *str, std::size_t len) {
  return [=](auto ...args) {
    return std::format({str, len}, std::forward<decltype(args)>(args)...);
  };
}
#else
#include <fmt/format.h>
/* using namespace fmt; */
constexpr auto operator ""_format(const char *str, std::size_t len) {
  return [=](auto ...args) {
    return fmt::format(std::string_view{str, len}, std::forward<decltype(args)>(args)...);
  };
}
#endif
#include <arpa/inet.h>

using namespace std::literals::string_view_literals;
using namespace fmt;
struct Header {
  enum class Type {
    Controller = 0,
    Module = 1,
    Screen = 2
  };
  struct Entry {
    Type type;
    std::uint32_t offset;
    std::uint32_t size;
  };
  std::string version;
  std::uint32_t flags;
  std::vector<Entry> entries;
};

struct Update {
  std::string version;
  std::uint32_t flags;
  std::optional<std::vector<char>> screen;
  std::optional<std::vector<char>> controller;
  std::vector<std::vector<char>> modules;
};

constexpr std::size_t headerSize(std::size_t entries) noexcept {
  return 2 + 32 + 4 + 1 + 9 * entries;
}

Header parseHeader(std::span<const char> buffer) {
  if(buffer.size() < headerSize(0))
    throw "Buffer too small to contain header";
  auto iter = buffer.begin();
  std::uint16_t length = ntohs(*(std::uint16_t*)&*iter);
  if(length > buffer.size())
    throw "Header length exceeds buffer size";
  iter += 2; length -= 2;

  Header header;

  header.version = [&] {
    auto end_iter = iter + 32;
    while(end_iter != iter && !*(end_iter - 1))
      --end_iter;
    return std::string(iter, end_iter);
  }();
  iter += 32; length -= 32;

  header.flags = ntohl(*(std::uint32_t*)&*iter);
  iter += 4; length -= 4;

  header.entries.resize(*iter);
  iter += 1; length -= 1;
  if (length < header.entries.size() * 9)
    throw "Too many entries for specified header size";
  for (Header::Entry &entry : header.entries) {
    entry.type = Header::Type(*iter);
    iter += 1;
    entry.offset = ntohl(*(std::uint32_t*)&*iter);
    iter += 4; length -= 4;
    entry.size = ntohl(*(std::uint32_t*)&*iter);
    iter += 4; length -= 4;
  }
  return header;
}
Update parseUpdate(std::span<const char> buffer) {
  Update update;
  auto header = parseHeader(buffer);
  update.version = std::move(header.version);
  update.flags = header.flags;
  for (auto &&entry : header.entries) {
    if (entry.offset + entry.size > buffer.size())
      throw "Length inconsistency detected";
    auto &container = [&](Header::Type type) -> auto& {
      switch(type) {
        case Header::Type::Controller:
          if (update.controller)
            throw "Duplicate Controller packet";
          return update.controller.emplace();
        case Header::Type::Module:
          update.modules.emplace_back();
          return update.modules.back();
        case Header::Type::Screen:
          if (update.screen)
            throw "Duplicate Screen packet";
          return update.screen.emplace();
        default: throw "Unknown entry type";
      }
    }(entry.type);
    container.resize(entry.size);
    auto begin_iter = buffer.begin() + entry.offset;
    std::copy(begin_iter, begin_iter + entry.size, container.begin());
  }
  return update;
}

std::size_t getSize(const Update &update) {
  std::size_t size = 0;
  std::size_t packets = update.modules.size();
  for (auto &&module : update.modules) {
    size += module.size();
  }
  if (update.controller) {
    size += update.controller->size(); ++packets;
  }
  if (update.screen) {
    size += update.screen->size(); ++packets;
  }
  return size + headerSize(packets);
}
std::span<char> serialize(const Header &header, std::span<char> buffer) {
  auto header_size = headerSize(header.entries.size());
  if (buffer.size() < header_size)
    throw "Buffer too small to hold header";
  if (header.version.size() > 32)
    throw "Version too long";
  if (header.entries.size() >= 0x100)
    throw "Too many entries in update header";
  *reinterpret_cast<std::uint16_t*>(buffer.data()) = htons(header_size);
  buffer = buffer.subspan<2>();
  std::fill(
      std::copy(header.version.begin(), header.version.end(), buffer.begin()),
      buffer.begin() + 32,
      0);
  buffer = buffer.subspan<32>();
  *reinterpret_cast<std::uint32_t*>(buffer.data()) = htonl(header.flags);
  buffer = buffer.subspan<4>();
  buffer.front() = header.entries.size();
  buffer = buffer.subspan<1>();
  for (auto &&entry : header.entries) {
    buffer.front() = (char)entry.type;
    buffer = buffer.subspan<1>();
    *reinterpret_cast<std::uint32_t*>(buffer.data()) = htonl(entry.offset);
    buffer = buffer.subspan<4>();
    *reinterpret_cast<std::uint32_t*>(buffer.data()) = htonl(entry.size);
    buffer = buffer.subspan<4>();
  }
  return buffer;
}
std::span<char> serialize(const Update &update, std::span<char> buffer) {
  Header header;
  header.version = update.version;
  header.flags = update.flags;
  std::uint32_t offset = headerSize(update.modules.size() + (update.controller?1:0) + (update.screen?1:0));
  for (auto &&module : update.modules) {
    header.entries.push_back(Header::Entry(Header::Type::Module, offset, module.size()));
    offset += module.size();
  }
  if (update.controller) {
    header.entries.push_back(Header::Entry(Header::Type::Controller, offset, update.controller->size()));
    offset += update.controller->size();
  }
  if (update.screen) {
    header.entries.push_back(Header::Entry(Header::Type::Screen,  offset, update.screen->size()));
    offset += update.screen->size();
  }
  if (buffer.size() < offset)
    throw "Buffer too small to hold update";
  buffer = serialize(header, buffer);
  for (auto &&module : update.modules) {
    std::copy(module.begin(), module.end(), buffer.begin());
    buffer = buffer.subspan(module.size());
  }
  if (update.controller) {
    std::copy(update.controller->begin(), update.controller->end(), buffer.begin());
    buffer = buffer.subspan(update.controller->size());
  }
  if (update.screen) {
    std::copy(update.screen->begin(), update.screen->end(), buffer.begin());
    buffer = buffer.subspan(update.screen->size());
  }
  return buffer;
}
std::vector<char> serialize(const Update &update) {
  std::vector<char> buffer(getSize(update));
  serialize(update, buffer);
  return buffer;
}

auto read_file(const char *filename) {
  std::vector<char> buffer(0x1000);
  std::ifstream file(filename);
  while(file.read(buffer.data() + (buffer.size() - 0x1000), 0x1000))
    buffer.resize(buffer.size() + 0x1000);
  buffer.resize(buffer.size() - 0x1000 + file.gcount());
  return buffer;
}

void write_file(const char *filename, const std::vector<char> &data) {
  std::ofstream file(filename);
  file.write(data.data(), data.size());
}

int main(int argc, char const* argv[])
try {
  switch(argc) {
    case 0: case 1:
      std::cerr << "Invalid usage. If you ever figure out the correct usage, feel free to contribute a nice help message.\n";
      return 1;
    case 2:
      {
        auto update = parseUpdate(read_file(argv[1]));
        if (update.screen)
          write_file("screen.apk", *update.screen);
        if (update.controller)
          write_file("controller.bin.packet", *update.controller);
        auto mod_begin = update.modules.begin(), mod_end = update.modules.end();
        for(auto iter = mod_begin; mod_end != iter; ++iter)
          write_file("module{}.bin.packet"_format(iter - mod_begin).c_str(), *iter);
        if (update.flags & 1)
          std::cout << "--force ";
        if (update.flags & ~1)
          std::cerr << "WARNING: Unknown flags dropped\n";
        std::cout << update.version << '\n';
      }
      break;
    default:
      {
        Update update;
        if (argv[1] == "--force"sv) {
          update.flags = 1; ++argv; --argc;
          if (argc == 2) {
            std::cerr << "Invalid usage\n";
            return 1;
          }
        } else
          update.flags = 0;
        update.version = argv[1];
        for (int i = 2; i != argc; ++i) {
          auto content = read_file(argv[i]);
          if (content.empty())
            std::cerr << "Skipping empty input file\n";
          else
            switch (content[0]) {
              case 0: update.controller = std::move(content); break;
              case 'P': update.screen = std::move(content); break;
              case 1: update.modules.push_back(std::move(content)); break;
              default: throw "Invalid input file\n";
            }
        }
        auto buffer = serialize(update);
        std::cout.write(buffer.data(), buffer.size());
      }
  }
  return 0;
} catch(const char *str) {
  std::cerr << "FATAL ERROR: " << str << '\n';
  return 1;
} catch(std::exception &ex) {
  std::cerr << "FATAL ERROR: " << ex.what() << '\n';
  return 1;
}
