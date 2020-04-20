/* Circular buffer */

#include <stdlib.h>

#include "cbuf.h"

/* Initialize the values in a circular buffer with the given length
 * buf: A pointer to the cbuf struct to initialize
 * length: The length of the circular buffer
 * Returns: 0 on success, non-zero on error
 */
int init_cbuf(struct cbuf *buf, int length)
{
	buf->buf = malloc(sizeof(void*) * length);
	if (buf == NULL)
		return BUF_FATAL;
	buf->len = length;
	buf->size = 0;
	buf->start = 0;
	buf->end = 0;

	return 0;
}

/* Free up and clear the contents of a cbuf struct.
 * All operations should fail after calling this function.
 * buf: A pointer to the cbuf struct to clear
 * Returns: 0 on success, non-zero on error
 */
int delete_cbuf(struct cbuf *buf)
{
	if (buf == NULL)
		return BUF_FATAL;

	free(buf->buf);
	buf->buf = NULL;
	buf->len = 0;
	buf->size = 0;
	buf->start = 0;
	buf->end = 0;

	return 0;
}

/* Push an item onto the circular buffer
 * buf: The buffer to append
 * item: The item to push
 * Returns: 0 on success, non-zero on error
 */
int cbuf_push(struct cbuf *buf, void *item)
{
	if (buf == NULL)
		return BUF_FATAL;

	if (buf->size < buf->len)
		buf->size++;
	else
		return BUF_NOSPACE;

	buf->buf[buf->end] = item;
	buf->end++;

	if (buf->end == buf->len)
		buf->end = 0;

	return 0;
}

/* Pop the least recent item from the circular buffer
 * buf: The buffer to pop
 * item: The double pointer for storing the item
 * Returns: 0 on success, non-zero on error
 */
int cbuf_pop(struct cbuf *buf, void **item)
{
	if (buf == NULL)
		return BUF_FATAL;
	if (buf->size == 0)
		return BUF_EMPTY;

	*item = buf->buf[buf->start];

	buf->start++;
	buf->size--;

	if (buf->start == buf->len)
		buf->start = 0;

	return 0;
}

/* Convenience function to check if a buffer is empty
 * buf: The circular buffer to check
 * Returns: 0 if empty, 1 if not empty, BUF_FATAL on error
 */
int cbuf_empty(struct cbuf *buf)
{
	if (buf == NULL)
		return BUF_FATAL;

	return buf->size == 0;
}

