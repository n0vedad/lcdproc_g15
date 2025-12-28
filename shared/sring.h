// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/sring.h
 * \brief Circular buffer (ring buffer) data structure for string processing
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
 * Include this header to access the circular buffer functions for string processing.
 *
 * \details Header file for circular buffer implementation optimized for string
 * processing operations. Uses "Always Keep One Byte Open" strategy to
 * distinguish between full and empty states.
 */

#ifndef SRING_H
#define SRING_H

/**
 * \brief Circular buffer structure with read/write pointers
 * \details Uses "Always Keep One Byte Open" strategy to distinguish between
 * full and empty states. The actual data buffer size is (size) bytes.
 */
typedef struct sring_buffer_t {
	char *data;	   // Dynamically allocated data storage
	unsigned int size; // The buffer's size
	unsigned int w;	   // write pointer
	unsigned int r;	   // read pointer
} sring_buffer;

/**
 * \brief Allocate a new ring buffer data structure
 * \param iSize Desired usable size of the ring buffer
 * \retval NULL Memory allocation failure
 * \retval !NULL Pointer to newly created ring buffer
 *
 * \details Creates and initializes a new circular buffer with the specified size.
 * The implementation uses the 'Always Keep One Byte Open' strategy, so the
 * internal data buffer is actually (iSize+1) bytes large to distinguish
 * between full and empty states.
 */
sring_buffer *sring_create(int iSize);

/**
 * \brief Free memory used by ring buffer
 * \param buf Ring buffer to destroy (can be NULL)
 *
 * \details Safely deallocates all memory associated with the ring buffer,
 * including the data buffer and the buffer structure itself. Handles
 * NULL pointers gracefully.
 */
void sring_destroy(sring_buffer *buf);

/**
 * \brief Clear the internal ring buffer
 * \param buf Ring buffer to work on
 *
 * \details Resets read and write pointers to zero and overwrites existing
 * data with NUL bytes. The buffer becomes empty after this operation.
 */
void sring_clear(sring_buffer *buf);

/**
 * \brief Get the number of bytes that can be written
 * \param buf Ring buffer to work on
 * \retval 0 Buffer is NULL or full
 * \retval >0 Number of bytes available for writing
 *
 * \details Uses 'Always Keep One Byte Open' strategy to calculate available
 * write space. The returned value accounts for buffer wraparound.
 */
int sring_getMaxWrite(sring_buffer *buf);

/**
 * \brief Get the number of bytes that can be read
 * \param buf Ring buffer to work on
 * \retval 0 Buffer is NULL or empty
 * \retval >0 Number of bytes available for reading
 *
 * \details Calculates the number of bytes currently stored in the buffer
 * and available for reading. Handles buffer wraparound correctly.
 */
int sring_getMaxRead(sring_buffer *buf);

/**
 * \brief Write src_len bytes from src into ring buffer
 * \param buf Ring buffer to write to
 * \param src Pointer to source data buffer
 * \param src_len Number of bytes to write
 * \retval 0 Success: all bytes written
 * \retval -1 Error: insufficient space or invalid parameters
 *
 * \details Attempts to write the specified number of bytes into the ring buffer.
 * The operation fails if there is insufficient space to write all bytes.
 * The write operation handles buffer wraparound automatically.
 */
int sring_write(sring_buffer *buf, char *src, int src_len);

/**
 * \brief Read dst_len bytes from ring buffer into destination
 * \param buf Ring buffer to work on
 * \param dst Pointer to target buffer
 * \param dst_len Number of bytes to read at most
 * \retval -1 Error: invalid parameters
 * \retval >=0 Number of bytes actually read
 * \warning The target buffer must be allocated by the caller
 *
 * \details Reads up to dst_len bytes from the ring buffer. If fewer bytes
 * are available, only the available bytes are read. Handles buffer wraparound.
 */
int sring_read(sring_buffer *buf, char *dst, int dst_len);

/**
 * \brief Return the next string from the ring buffer
 * \param buf Ring buffer to read from
 * \retval NULL No complete string available or memory allocation failure
 * \retval !NULL Pointer to dynamically allocated string
 * \warning Caller is responsible for freeing the returned string
 *
 * \details Extracts a complete string from the ring buffer, where a string
 * is defined as a sequence of bytes terminated by \\r, \\n, or \\0. The
 * memory for the returned string is allocated dynamically and must be
 * freed by the caller. The returned string is always null-terminated
 * but does not include the terminating delimiter character.
 */
char *sring_read_string(sring_buffer *buf);

/**
 * \brief Print content of buffer to stdout for debugging
 * \param buf Ring buffer to examine
 * \warning Function only available in debug builds
 *
 * \details Outputs the entire contents of the ring buffer to stdout,
 * showing printable characters as-is and non-printable characters
 * in hexadecimal format. This function is only available when
 * compiled with DEBUG defined.
 */
void sring_dump(sring_buffer *buf);

#endif
