#ifndef CBUF_H
#define CBUF_H

struct cbuf {
	int *buf;
	int len;
	int size;
	int start;
	int end;
};

#define BUF_FATAL		(-1)
#define BUF_NOSPACE		(-2)
#define BUF_EMPTY		(-3)

int init_cbuf(struct cbuf *buf, int length);
int delete_cbuf(struct cbuf *buf);
int cbuf_push(struct cbuf *buf, int item);
int cbuf_pop(struct cbuf *buf, int *item);
int cbuf_empty(struct cbuf *buf);

#endif
