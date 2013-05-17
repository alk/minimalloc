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

	return s;
#undef T
}

int main(void)
{
	int number_failed;
	Suite *s = mini_suite();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
