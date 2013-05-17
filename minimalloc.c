#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <bsd/sys/tree.h>
#include <assert.h>

#include "minimalloc.h"

struct free_span;

RB_HEAD(mini_rb, free_span);

struct mini_state {
	mini_mallocer mallocer;
	mini_freer freer;
	struct mini_rb head;
	void **next_chunk;
};

struct free_span {
	size_t size;
	RB_ENTRY(free_span) rb_link;
};

#define MIN_SPAN_SIZE (sizeof(struct free_span) + sizeof(intptr_t))

#define SPAN_SIZE_FREE_MASK (~(((size_t)-1) >> 1))
#define SPAN_SIZE_PREV_FREE_MASK (SPAN_SIZE_FREE_MASK >> 1)
#define SPAN_SIZE_VALUE_MASK (SPAN_SIZE_PREV_FREE_MASK - 1)

static inline
int mini_rb_cmp(struct free_span *a, struct free_span *b)
{
	if (a->size == b->size)
		return (intptr_t)a - (intptr_t)b;
	return (int)((ssize_t)(a->size) - (ssize_t)(b->size));
}

/* looks like some bug in libbsd I have on my box */
#ifndef __unused
#define __unused __attribute__((unused))
#endif

RB_PROTOTYPE_STATIC(mini_rb, free_span, rb_link, mini_rb_cmp);
RB_GENERATE_STATIC(mini_rb, free_span, rb_link, mini_rb_cmp);

#define CHUNK_SIZE (128*1024*1024)

static inline
size_t min_size(size_t a, size_t b)
{
	return (a > b) ? b : a;
}

static inline
size_t max_size(size_t a, size_t b)
{
	return (a < b) ? b : a;
}

static
void insert_span(struct mini_state *st, void *at, size_t size)
{
	struct free_span *span = (struct free_span *)at;
	span->size = size | SPAN_SIZE_FREE_MASK;
	((size_t *)(((char *)at) + size))[-1] |= SPAN_SIZE_PREV_FREE_MASK;
	mini_rb_RB_INSERT(&st->head, span);
}

struct mini_state *mini_init(mini_mallocer mallocer, mini_freer freer)
{
	struct initial_stuff {
		struct mini_state state;
		struct free_span first_span;
	};
	struct initial_stuff *first_chunk = mallocer(CHUNK_SIZE);
	if (!first_chunk)
		return 0;
	first_chunk->state.mallocer = mallocer;
	first_chunk->state.freer = freer;
	RB_INIT(&first_chunk->state.head);
	first_chunk->state.next_chunk = 0;
	insert_span(&first_chunk->state, &first_chunk->first_span,
		    CHUNK_SIZE - sizeof(size_t) - offsetof(struct initial_stuff, first_span));
	return &first_chunk->state;
}

void mini_deinit(struct mini_state *st)
{
	void **next = st->next_chunk;
	mini_freer freer = st->freer;
	void **current = (void **)st;
	do {
		freer(current);
		current = next;
		if (!current)
			return;
		next = (void **)*current;
	} while (1);
}

static void *do_malloc_with_fit(struct mini_state *st, size_t sz, struct free_span *fit);

static inline
size_t compute_allocation_sz(size_t size)
{
	return (max_size(size + sizeof(size_t), MIN_SPAN_SIZE) + sizeof(void *) - 1) & (size_t)(-sizeof(void *));
}

static void *mini_malloc_new_chunk(struct mini_state *st, size_t size)
{
	struct next_stuff {
		void **next_chunk;
		struct free_span first_span;
	};
	struct next_stuff *next_chunk;
	size_t chunk_overhead = sizeof(size_t) + sizeof(size_t)
		+ offsetof(struct next_stuff, first_span);
	size_t alloc_size = max_size(CHUNK_SIZE, size + chunk_overhead);

	next_chunk = st->mallocer(alloc_size);
	if (!next_chunk)
		return 0;
	next_chunk->next_chunk = st->next_chunk;
	st->next_chunk = (void **)next_chunk;
	insert_span(st, &next_chunk->first_span, alloc_size - chunk_overhead);
	return do_malloc_with_fit(st, compute_allocation_sz(size), &next_chunk->first_span);
}

void *mini_malloc(struct mini_state *st, size_t size)
{
	size_t sz = compute_allocation_sz(size);
	struct free_span perfect_fit = {.size = sz};
	struct free_span *fit;

	fit = mini_rb_RB_NFIND(&st->head, &perfect_fit);

	if (!fit)
		return mini_malloc_new_chunk(st, size);

	return do_malloc_with_fit(st, sz, fit);
}

static
void *do_malloc_with_fit(struct mini_state *st, size_t sz, struct free_span *fit)
{
	ssize_t remaining_space;
	size_t *hdr;

	mini_rb_RB_REMOVE(&st->head, fit);

	remaining_space = fit->size - sz;

	assert(remaining_space >= 0);

	if (remaining_space >= MIN_SPAN_SIZE) {
		struct free_span *hole = (struct free_span *)((char *)fit + sz);
		hole->size = remaining_space;
		mini_rb_RB_INSERT(&st->head, hole);
	} else {
		sz = fit->size;
	}

	hdr = (size_t *)fit;
	*hdr = sz;

	*((size_t *)(((char *)hdr) + sz)) &= ~SPAN_SIZE_PREV_FREE_MASK;

	return (void *)(hdr+1);
}

static void do_mini_free(struct mini_state *st, void *_ptr);

void mini_free(struct mini_state *st, void *_ptr)
{
	if (!_ptr) {
		return;
	}

	do_mini_free(st, _ptr);
}

static void do_mini_free(struct mini_state *st, void *_ptr)
{
	if (!_ptr) {
		return;
	}

	size_t *hdr = (size_t *)_ptr;
	size_t raw_sz = hdr[-1];
	size_t sz = raw_sz & SPAN_SIZE_VALUE_MASK;
	size_t *next_span = (size_t *)((char *)hdr + sz);

	if ((*next_span) & SPAN_SIZE_FREE_MASK) {
		struct free_span *real_next_span = (struct free_span *)next_span;
		mini_rb_RB_REMOVE(&st->head, real_next_span);
		sz += real_next_span->size & SPAN_SIZE_VALUE_MASK;
	}

	if (raw_sz & SPAN_SIZE_PREV_FREE_MASK) {
		size_t prev_size = hdr[-2];
		struct free_span *prev_span = (struct free_span *)((char *)hdr - prev_size);
		mini_rb_RB_REMOVE(&st->head, prev_span);
		sz += prev_size;
		_ptr = (void *)prev_span;
	}

	insert_span(st, _ptr, sz);
}

void *mini_realloc(struct mini_state *st, void *p, size_t new_size)
{
	void *new_p;
	if (new_size == 0) {
		mini_free(st, p);
		return 0;
	}
	new_p = mini_malloc(st, new_size);
	if (!new_p) {
		return new_p;
	}
	memcpy(new_p, p, min_size((((size_t *)p)[-1] & SPAN_SIZE_VALUE_MASK) - sizeof(size_t), new_size));
	mini_free(st, p);
	return new_p;
}
