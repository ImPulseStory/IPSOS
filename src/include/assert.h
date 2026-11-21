#ifndef IPSOS_ASSERT_H
#define IPSOS_ASSERT_H

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
void abort(void);
#define assert(expression) ((expression) ? (void)0 : abort())
#endif

#endif
