#ifndef ENDIAN_HELPER
#define ENDIAN_HELPER

#if __has_include(<endian.h>)
#include <endian.h>
#else
#if __has_include(<arpa/inet.h>)
#include <arpa/inet.h>
#elif __has_include(<winsock2.h>)
#include <winsock2.h>
#else
#error "htons is not available on your system"
#endif
#include <cstdint>
#define htobe16(x) htons(x)
#define htobe32(x) htonl(x)
#define htole16(x) std::uint16_t(x)
#define htole32(x) std::uint32_t(x)
#define be16toh(x) ntohs(x)
#define be32toh(x) ntohl(x)
#define le16toh(x) std::uint16_t(x)
#define le32toh(x) std::uint32_t(x)
#endif

#endif
