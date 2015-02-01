#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <check.h>

#include "minimalloc.h"

static struct mini_state *st;
static int mallocs_count;
static int frees_count;
static void *last_malloc_rv;
static size_t last_malloc_size;
static void *last_free_arg;

static
void *wrap_malloc(size_t s)
{
	void *rv = malloc(s);
	mallocs_count++;
	fail_unless(s == 0 || rv != 0);
	last_malloc_rv = rv;
	last_malloc_size = s;
	return rv;
}

static
void wrap_free(void *p)
{
	free(p);
	frees_count++;
	last_free_arg = p;
}

START_TEST(test_init_deinit_quickly)
{
	st = mini_init(wrap_malloc, wrap_free);
	fail_unless(st != 0);
	mini_deinit(st);
	fail_unless(mallocs_count == 1);
	fail_unless(frees_count == 1);
	fail_unless(last_malloc_rv == last_free_arg);
}
END_TEST

START_TEST(test_one_malloc)
{
	char *ptr;
	void *first_chunk_ptr;
	struct mini_spans spans = {.spans_capacity = 1};
	int rv;
	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, &spans);
	first_chunk_ptr = spans.spans[0].at;
	fail_unless(rv == 1);
	ptr = mini_malloc(st, 1024);
	rv = mini_fill_mini_spans(st, &spans);
	fail_unless(rv == 1);
	fail_unless(spans.spans[0].at == (void *)(ptr + 1024));
	mini_free(st, ptr);
	rv = mini_fill_mini_spans(st, &spans);
	fail_unless(rv == 1);
	fail_unless(spans.spans[0].at == first_chunk_ptr);
	fail_unless(spans.spans[0].at == ptr - sizeof(size_t));
	mini_deinit(st);
}
END_TEST

START_TEST(test_two_mallocs)
{
	char *ptr1, *ptr2;
	void *first_chunk_ptr;
	size_t first_chunk_size;
	char spans_storage[sizeof(struct mini_spans) + (sizeof(void *) + sizeof(size_t)) * 256];
	struct mini_spans *spans = (void *)spans_storage;
	int rv;

	spans->spans_capacity = 128;

	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, spans);
	first_chunk_ptr = spans->spans[0].at;
	first_chunk_size = spans->spans[0].size;
	fail_unless(rv == 1);

	ptr1 = mini_malloc(st, 1024);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr1 + 1024));
	fail_unless((void *)(ptr1 - sizeof(size_t)) == first_chunk_ptr);

	ptr2 = mini_malloc(st, 2048+8);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr2 + 2048 + 8));

	mini_free(st, ptr1);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[0].at == (void *)(ptr1 - sizeof(size_t)), "%p, %p", spans->spans[0].at, ptr1 - sizeof(size_t));
	fail_unless(spans->spans[0].size == ptr2 - ptr1);

	mini_free(st, ptr2);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(first_chunk_ptr == spans->spans[0].at);
	fail_unless(first_chunk_size == spans->spans[0].size);

	mini_deinit(st);
}
END_TEST

START_TEST(test_two_mallocs_b)
{
	char *ptr1, *ptr2, *ptr3;
	void *first_chunk_ptr;
	size_t first_chunk_size;
	char spans_storage[sizeof(struct mini_spans) + (sizeof(void *) + sizeof(size_t)) * 256];
	struct mini_spans *spans = (void *)spans_storage;
	int rv;

	spans->spans_capacity = 128;

	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, spans);
	first_chunk_ptr = spans->spans[0].at;
	first_chunk_size = spans->spans[0].size;
	fail_unless(rv == 1);

	ptr1 = mini_malloc(st, 1024);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr1 + 1024));
	fail_unless((void *)(ptr1 - sizeof(size_t)) == first_chunk_ptr);

	ptr2 = mini_malloc(st, 2048+8);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr2 + 2048 + 8));

	ptr3 = mini_malloc(st, spans->spans[0].size - sizeof(void *) * 2);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 0);

	mini_free(st, ptr3);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr2 + 2048 + 8));

	mini_free(st, ptr1);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[0].at == (void *)(ptr1 - sizeof(size_t)), "%p, %p", spans->spans[0].at, ptr1 - sizeof(size_t));
	fail_unless(spans->spans[0].size == ptr2 - ptr1);

	mini_free(st, ptr2);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(first_chunk_ptr == spans->spans[0].at);
	fail_unless(first_chunk_size == spans->spans[0].size);

	mini_deinit(st);
}
END_TEST

START_TEST(test_three_mallocs)
{
	char *ptr1, *ptr2, *ptr3;
	void *first_chunk_ptr;
	size_t first_chunk_size;
	char spans_storage[sizeof(struct mini_spans) + (sizeof(void *) + sizeof(size_t)) * 256];
	struct mini_spans *spans = (void *)spans_storage;
	int rv;

	spans->spans_capacity = 128;

	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, spans);
	first_chunk_ptr = spans->spans[0].at;
	first_chunk_size = spans->spans[0].size;
	fail_unless(rv == 1);

	ptr1 = mini_malloc(st, 1024);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr1 + 1024));
	fail_unless((void *)(ptr1 - sizeof(size_t)) == first_chunk_ptr);

	ptr2 = mini_malloc(st, 2048+8);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == (void *)(ptr2 + 2048 + 8));

	mini_free(st, ptr1);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[0].at == (void *)(ptr1 - sizeof(size_t)), "%p, %p", spans->spans[0].at, ptr1 - sizeof(size_t));
	fail_unless(spans->spans[0].size == ptr2 - ptr1);
	fail_unless(spans->spans[1].at == (void *)(ptr2 + 2048 + 8));

	ptr3 = mini_malloc(st, 1024);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1, "rv: %d", rv);
	fail_unless(ptr3 == ptr1);
	mini_free(st, ptr3);

	ptr3 = mini_malloc(st, 512);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(ptr3 == ptr1);
	fail_unless(spans->spans[0].at == (void *)(ptr1 + 512));
	fail_unless(spans->spans[1].at == (void *)(ptr2 + 2048 + 8));
	mini_free(st, ptr3);

	ptr3 = mini_malloc(st, 3072);
	fail_unless(ptr3 == ptr2 + 2048 + 8 + sizeof(size_t));
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[1].at == (void *)(ptr3 + 3072));

	mini_free(st, ptr2);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 2);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[0].size = ptr3 - sizeof(size_t) - (char *)first_chunk_ptr);
	fail_unless(spans->spans[1].at == (void *)(ptr3 + 3072));

	mini_free(st, ptr3);
	rv = mini_fill_mini_spans(st, spans);
	fail_unless(rv == 1);
	fail_unless(spans->spans[0].at == first_chunk_ptr);
	fail_unless(spans->spans[0].size == first_chunk_size);

	mini_deinit(st);
	fail_unless(mallocs_count == 1);
	fail_unless(frees_count == 1);
	fail_unless(last_malloc_rv == last_free_arg);
}
END_TEST

START_TEST(test_huge_malloc)
{
	char *ptr;
	void *first_chunk_ptr;
	size_t first_chunk_size;
	struct mini_spans spans = {.spans_capacity = 1};
	int rv;
	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, &spans);
	first_chunk_ptr = spans.spans[0].at;
	first_chunk_size = spans.spans[0].size;
	fail_unless(rv == 1);

	ptr = mini_malloc(st, first_chunk_size + 1024);
	fail_if(abs(ptr - (char *)first_chunk_ptr) < 1024);

	rv = mini_fill_mini_spans(st, &spans);
	fail_unless(rv == 1);
	fail_unless(first_chunk_ptr == spans.spans[0].at);
	fail_unless(first_chunk_size == spans.spans[0].size);

	mini_deinit(st);
	fail_unless(mallocs_count == 2);
	fail_unless(frees_count == 2);
}
END_TEST

START_TEST(test_huge_malloc_b)
{
	char *ptr;
	void *first_chunk_ptr;
	size_t first_chunk_size;
	struct mini_spans spans = {.spans_capacity = 1};
	int rv;
	st = mini_init(wrap_malloc, wrap_free);
	rv = mini_fill_mini_spans(st, &spans);
	first_chunk_ptr = spans.spans[0].at;
	first_chunk_size = spans.spans[0].size;
	fail_unless(rv == 1);

	ptr = mini_malloc(st, first_chunk_size + 1333);
	fail_if(abs(ptr - (char *)first_chunk_ptr) < 1024);

	rv = mini_fill_mini_spans(st, &spans);
	fail_unless(rv == 1);
	fail_unless(first_chunk_ptr == spans.spans[0].at);
	fail_unless(first_chunk_size == spans.spans[0].size);

	mini_deinit(st);
	fail_unless(mallocs_count == 2);
	fail_unless(frees_count == 2);
}
END_TEST

static
void global_setup(void)
{
	st = 0;
	mallocs_count = 0;
	frees_count = 0;
}

static
Suite *mini_suite(void)
{
	Suite *s = suite_create ("minimalloc");

#define T(name)							\
	do {							\
		TCase *tc = tcase_create(#name);		\
		tcase_add_test(tc, name);			\
		tcase_add_checked_fixture(tc, global_setup, 0);	\
		suite_add_tcase(s, tc);				\
	} while (0)

	T(test_init_deinit_quickly);
	T(test_one_malloc);
	T(test_two_mallocs);
	T(test_two_mallocs_b);
	T(test_three_mallocs);
	T(test_huge_malloc);
	T(test_huge_malloc_b);

	return s;
#undef T
}

int main(void)
{
	int number_failed;
	Suite *s = mini_suite();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
