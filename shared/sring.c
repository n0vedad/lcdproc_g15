// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/sring.c
 * \brief Circular buffer implementation for efficient string processing
 * \author Markus Dolze
 * \date 2009
 *
 * \features
 * - Circular buffer with configurable size
 * - "Always Keep One Byte Open" full/empty detection strategy
 * - Efficient read/write operations without data copying
 * - String-oriented operations with delimiter support
 * - Memory management functions (create, destroy, clear)
 * - Debug output capabilities
 * - Safe bounds checking and error handling
 *
 * \usage
 * - Create buffer with sring_create(size)
 * - Write data with sring_write()
 * - Read strings with sring_read_string() or raw data with sring_read()
 * - Check available space with sring_getMaxWrite() and sring_getMaxRead()
 * - Clean up with sring_destroy() when finished
 *
 * \details This file implements a circular (ring) buffer data structure optimized
 * for string processing operations. It uses the "Always Keep One Byte Open" strategy
 * to distinguish between full and empty states. The ring buffer is particularly
 * useful for buffering network data, parsing streaming input, and managing
 * producer-consumer scenarios.
 *
 * \todo Implement sring_peek() and sring_skip() functions.
 *
 * String ring buffer library is missing utility functions for non-destructive access:
 *
 * sring_peek():
 * - Read data from buffer without removing it (non-destructive read)
 * - Useful for protocol parsing where lookahead is needed
 * - Should return pointer to data at given offset
 * - Must not modify read/write pointers
 *
 * sring_skip():
 * - Advance read pointer without copying data
 * - Faster than read+discard for skipping unwanted data
 * - Should accept byte count to skip
 * - Must update read pointer and available count
 *
 * Both functions would improve efficiency for stream parsing and protocol handling.
 *
 * Impact: API completeness, parsing efficiency, protocol flexibility
 *
 * \ingroup ToDo_low
 */

#include <stdlib.h>
#include <string.h>

#include "sring.h"

#ifdef DEBUG
#include <ctype.h>
#include <stdio.h>
#endif

// Allocate a new ring buffer data structure with specified size
sring_buffer *sring_create(int iSize)
{
	sring_buffer *buf;

	if ((buf = malloc(sizeof(*buf))) == NULL)
		return NULL;

	// Allocate data buffer with extra byte for "Always Keep One Byte Open" strategy
	if ((buf->data = malloc(iSize + 1)) == NULL) {
		free(buf);
		return NULL;
	}

	buf->size = iSize + 1;
	buf->w = 0;
	buf->r = 0;

	return buf;
}

// Free memory used by ring buffer
void sring_destroy(sring_buffer *buf)
{
	if (buf == NULL)
		return;

	free(buf->data);
	buf->data = NULL;
	free(buf);
}

// Clear the internal ring buffer and reset pointers
void sring_clear(sring_buffer *buf)
{
	if (buf == NULL)
		return;

	buf->w = 0;
	buf->r = 0;

	memset(buf->data, '\0', buf->size);
}

/**
 * \brief Calculate available space/data in ring buffer
 * \param from Start position in ring buffer
 * \param to End position in ring buffer
 * \param size Total buffer size
 * \param reserve Reserved space (1 for write operations, 0 for read)
 * \return Available space (for write) or data length (for read)
 *
 * \details Handles wraparound arithmetic. When from==to, returns size-1
 * for write operations or 0 for read operations.
 */
static int calculate_ring_distance(int from, int to, int size, int reserve)
{
	int distance;

	if (from == to) {
		// Buffer is empty (for write) or full (for read)
		// For write (reserve=1): return size-1 (max writable space)
		// For read (reserve=0): return 0 (nothing to read)
		distance = (reserve > 0) ? (size - reserve) : 0;
	} else if (from < to) {
		distance = to - from - reserve;
	} else {
		distance = (size - from) + to - reserve;
	}

	// Ensure we never return negative values
	return (distance < 0) ? 0 : distance;
}

/**
 * \brief Copy data with ring buffer wraparound handling
 * \param buffer Ring buffer storage
 * \param buffer_size Total ring buffer size
 * \param pointer Pointer to current position (read or write pointer)
 * \param data Source/destination data buffer
 * \param data_len Length of data to copy
 * \param is_write 1 for write operation, 0 for read operation
 *
 * \details Handles wrap-around by splitting copy into two memcpy() calls
 * if data crosses buffer boundary. Updates pointer position after copy.
 */
static void copy_with_wraparound(char *buffer, int buffer_size, unsigned int *pointer, char *data,
				 int data_len, int is_write)
{

	// Single block copy - no wraparound needed
	if (*pointer + data_len < buffer_size) {
		if (is_write)
			memcpy(buffer + *pointer, data, data_len);
		else
			memcpy(data, buffer + *pointer, data_len);
		*pointer += data_len;

		// Two block copy - wraparound required
	} else {
		int firstBlockLen = buffer_size - *pointer;
		int secondBlockLen = data_len - firstBlockLen;

		// First block: from current pointer to end of buffer
		if (is_write)
			memcpy(buffer + *pointer, data, firstBlockLen);
		else
			memcpy(data, buffer + *pointer, firstBlockLen);

		// Second block: wraparound to beginning of buffer
		if (secondBlockLen > 0) {
			if (is_write)
				memcpy(buffer, data + firstBlockLen, secondBlockLen);
			else
				memcpy(data + firstBlockLen, buffer, secondBlockLen);
		}

		*pointer = secondBlockLen;
	}
}

// Get the number of bytes that can be written to the buffer
int sring_getMaxWrite(sring_buffer *buf)
{
	if (buf == NULL)
		return 0;

	return calculate_ring_distance(buf->w, buf->r, buf->size, 1);
}

// Get the number of bytes that can be read from the buffer
int sring_getMaxRead(sring_buffer *buf)
{
	if (buf == NULL)
		return 0;

	return calculate_ring_distance(buf->r, buf->w, buf->size, 0);
}

// Write src_len bytes from src into ring buffer
int sring_write(sring_buffer *buf, char *src, int src_len)
{
	if (buf == NULL || src == NULL || src_len <= 0)
		return -1;

	// Check if enough space is available (all-or-nothing approach)
	if (src_len > sring_getMaxWrite(buf))
		return -1;

	copy_with_wraparound(buf->data, buf->size, &buf->w, src, src_len, 1);

	return 0;
}

// Read dst_len bytes from ring buffer into destination
int sring_read(sring_buffer *buf, char *dst, int dst_len)
{
	if (buf == NULL || dst == NULL || dst_len <= 0)
		return -1;

	// Limit read to available data
	if (dst_len > sring_getMaxRead(buf))
		dst_len = sring_getMaxRead(buf);

	copy_with_wraparound(buf->data, buf->size, &buf->r, dst, dst_len, 0);

	return dst_len;
}

// Return the next string from the ring buffer
char *sring_read_string(sring_buffer *buf)
{
	int n;
	char *border;
	char *p;
	char *dst;
	int dst_len;

	if (buf == NULL)
		return NULL;

	n = sring_getMaxRead(buf);
	border = buf->data + buf->size;
	p = buf->data + buf->r;

	// Scan for string terminator (\r, \n, or \0)
	while (--n >= 0) {
		if (*p == '\r' || *p == '\n' || *p == '\0')
			break;
		p++;

		// Handle buffer wraparound
		if (p == border)
			p = buf->data;
	};

	if (n == -1)
		return NULL;

	dst_len = sring_getMaxRead(buf) - n;
	if ((dst = malloc(dst_len)) == NULL)
		return NULL;

	sring_read(buf, dst, dst_len);
	dst[dst_len - 1] = '\0';

	return dst;
}

// Print content of buffer to stdout for debugging
void sring_dump(sring_buffer *buf)
{
#ifdef DEBUG
	int a;

	if (buf == NULL)
		return;

	// Print printable characters as-is, non-printable in hex
	for (a = 0; a < buf->size; a++) {
		if (isprint(buf->data[a]))
			printf("'%c' ", buf->data[a]);
		else
			printf("0x%02X ", buf->data[a]);
	}

	printf("\n");
#endif
}
