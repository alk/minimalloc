#ifndef MINIMALLOC_H
#define MINIMALLOC_H

struct mini_state;

typedef void *(*mini_mallocer)(size_t size);
typedef void (*mini_freer)(void *);

extern struct mini_state *mini_init(mini_mallocer mallocer, mini_freer freer);
extern void mini_deinit(struct mini_state *st);
extern void *mini_malloc(struct mini_state *, size_t size);
extern void mini_free(struct mini_state *, void *);
extern void *mini_realloc(struct mini_state *, void *, size_t);


#endif
