#include <string.h>

/**
 * strestr - string end string - tests if the needle is the last
 *           substring in the haystack
 */
static inline char *strestr(const char *haystack, const char *needle)
{
    haystack = haystack + strlen(haystack) - 1 - strlen(needle);
    return strstr(haystack, needle);
}

/**
 * @return Computed hash, memory pointed by len will contain length
 *         of the string with /0 character.
 */
static inline unsigned int gethash(const char *ptr, int *len)
{
	unsigned int hash = 0;
	const char *start = ptr;

	do {
		hash = 31 * hash + *ptr;
		ptr++;
	} while  (*ptr);

	*len = ptr - start + 1;
	return hash;
}

#include "config.h"
#include <stdint.h>

#ifdef __linux__
#include <asm/byteorder.h>
#endif

#ifdef __linux__
static inline uint64_t from_le64(uint64_t v)
{
    return __le64_to_cpu(v);
}
static inline uint64_t to_le64(uint64_t v)
{
    return __cpu_to_le64(v);
}
#else
#ifdef FC_LITTLE_ENDIAN
static inline uint64_t from_le64(uint64_t v)
{
    return v;
}
static inline uint64_t to_le64(uint64_t v)
{
    return v;
}
#else
#error no generic big-endian support
#endif
#endif
