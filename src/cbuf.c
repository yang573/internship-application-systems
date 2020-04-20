/* Circular buffer */

#include <stdlib.h>

#include "cbuf.h"

int init_cbuf(struct cbuf *buf, int length)
{
	buf = malloc(length);
	if (buf == NULL)
		return BUF_FATAL;
	buf->len = length;
	buf->size = 0;
	buf->start = 0;
	buf->end = 0;

	return 0;
}

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

int cbuf_push(struct cbuf *buf, int item)
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

int cbuf_pop(struct cbuf *buf, int *item)
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

int cbuf_empty(struct cbuf *buf)
{
	return buf->size == 0;
}

