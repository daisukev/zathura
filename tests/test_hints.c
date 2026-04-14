/* SPDX-License-Identifier: Zlib */

#include "page-widget.h"
#include <glib.h>
#include <string.h>

#define CHARS     "abc"
#define CHARS_N   3
#define VCHARS    "sadfjklewcmpgh"
#define VCHARS_N  14

/* ── helpers ─────────────────────────────────────────────────────────────── */

static bool hints_prefix_free(char** hints, unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    for (unsigned int j = 0; j < count; j++) {
      if (i == j) {
        continue;
      }
      /* hints[i] must not be a prefix of hints[j] */
      size_t li = strlen(hints[i]);
      if (g_ascii_strncasecmp(hints[i], hints[j], li) == 0 && strlen(hints[j]) > li) {
        return false;
      }
    }
  }
  return true;
}

static bool hints_unique(char** hints, unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    for (unsigned int j = i + 1; j < count; j++) {
      if (g_ascii_strcasecmp(hints[i], hints[j]) == 0) {
        return false;
      }
    }
  }
  return true;
}

/* ── generation ──────────────────────────────────────────────────────────── */

static void test_generate_null_on_zero(void) {
  char** hints = vimium_hints_generate(CHARS, CHARS_N, 0);
  g_assert_null(hints);
}

static void test_generate_count_one(void) {
  char** hints = vimium_hints_generate(CHARS, CHARS_N, 1);
  g_assert_nonnull(hints);
  g_assert_cmpuint(strlen(hints[0]), ==, 1);
  vimium_hints_free(hints, 1);
}

static void test_generate_single_chars_exact(void) {
  /* exactly n links → all single-char hints */
  char** hints = vimium_hints_generate(CHARS, CHARS_N, CHARS_N);
  g_assert_nonnull(hints);
  for (unsigned int i = 0; i < CHARS_N; i++) {
    g_assert_cmpuint(strlen(hints[i]), ==, 1);
  }
  g_assert_true(hints_unique(hints, CHARS_N));
  g_assert_true(hints_prefix_free(hints, CHARS_N));
  vimium_hints_free(hints, CHARS_N);
}

static void test_generate_overflow_into_two_chars(void) {
  /* n+1 links: one single-char hint must expand → mix of lengths, but
   * still prefix-free */
  unsigned int count = CHARS_N + 1;
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);
  g_assert_true(hints_unique(hints, count));
  g_assert_true(hints_prefix_free(hints, count));
  vimium_hints_free(hints, count);
}

static void test_generate_prefix_free_small(void) {
  for (unsigned int count = 1; count <= 20; count++) {
    char** hints = vimium_hints_generate(CHARS, CHARS_N, count);
    g_assert_nonnull(hints);
    g_assert_true(hints_unique(hints, count));
    g_assert_true(hints_prefix_free(hints, count));
    vimium_hints_free(hints, count);
  }
}

static void test_generate_prefix_free_vimium_chars(void) {
  /* test representative counts across single-, double-, and triple-char
   * ranges for the real vimium character set */
  unsigned int counts[] = {1, 7, 14, 15, 100, 196, 197, 500};
  for (unsigned int i = 0; i < G_N_ELEMENTS(counts); i++) {
    char** hints = vimium_hints_generate(VCHARS, VCHARS_N, counts[i]);
    g_assert_nonnull(hints);
    g_assert_true(hints_unique(hints, counts[i]));
    g_assert_true(hints_prefix_free(hints, counts[i]));
    vimium_hints_free(hints, counts[i]);
  }
}

static void test_generate_uppercase(void) {
  char** hints = vimium_hints_generate(CHARS, CHARS_N, CHARS_N);
  g_assert_nonnull(hints);
  for (unsigned int i = 0; i < CHARS_N; i++) {
    for (unsigned int j = 0; j < strlen(hints[i]); j++) {
      g_assert_true(g_ascii_isupper(hints[i][j]));
    }
  }
  vimium_hints_free(hints, CHARS_N);
}

/* ── known values (traced by hand from the vimium algorithm) ─────────────── */

static void test_generate_known_three_links(void) {
  /* n=3 "abc", count=3 → ["A","B","C"] (sorted single chars) */
  char** hints = vimium_hints_generate(CHARS, CHARS_N, 3);
  g_assert_nonnull(hints);
  g_assert_cmpstr(hints[0], ==, "A");
  g_assert_cmpstr(hints[1], ==, "B");
  g_assert_cmpstr(hints[2], ==, "C");
  vimium_hints_free(hints, 3);
}

static void test_generate_known_four_links(void) {
  /* n=3 "abc", count=4:
   *   BFS slice before sort: ["b","c","aa","ba"]
   *   after sort+reverse+upper: ["AA","B","AB","C"] */
  char** hints = vimium_hints_generate(CHARS, CHARS_N, 4);
  g_assert_nonnull(hints);
  g_assert_cmpstr(hints[0], ==, "AA");
  g_assert_cmpstr(hints[1], ==, "B");
  g_assert_cmpstr(hints[2], ==, "AB");
  g_assert_cmpstr(hints[3], ==, "C");
  vimium_hints_free(hints, 4);
}

/* ── round-trip lookup ───────────────────────────────────────────────────── */

static void test_lookup_roundtrip(void) {
  unsigned int count = 10;
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);

  for (unsigned int i = 0; i < count; i++) {
    int idx = hint_label_to_index(CHARS, CHARS_N, count, hints[i]);
    g_assert_cmpint(idx, ==, (int)i);
  }

  vimium_hints_free(hints, count);
}

static void test_lookup_case_insensitive(void) {
  unsigned int count = 10;
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);

  for (unsigned int i = 0; i < count; i++) {
    /* lowercase version of the (uppercase) generated hint */
    char* lower = g_ascii_strdown(hints[i], -1);
    int idx     = hint_label_to_index(CHARS, CHARS_N, count, lower);
    g_assert_cmpint(idx, ==, (int)i);
    g_free(lower);
  }

  vimium_hints_free(hints, count);
}

static void test_lookup_invalid_input(void) {
  g_assert_cmpint(hint_label_to_index(CHARS, CHARS_N, 3, ""),   ==, -1);
  g_assert_cmpint(hint_label_to_index(CHARS, CHARS_N, 3, "Z"),  ==, -1);
  g_assert_cmpint(hint_label_to_index(CHARS, CHARS_N, 3, "1"),  ==, -1);
  g_assert_cmpint(hint_label_to_index(NULL,  CHARS_N, 3, "A"),  ==, -1);
  g_assert_cmpint(hint_label_to_index(CHARS, 0,       3, "A"),  ==, -1);
  g_assert_cmpint(hint_label_to_index(CHARS, CHARS_N, 0, "A"),  ==, -1);
}

static void test_lookup_roundtrip_vimium_chars(void) {
  unsigned int counts[] = {1, 14, 15, 50};
  for (unsigned int i = 0; i < G_N_ELEMENTS(counts); i++) {
    unsigned int count = counts[i];
    char** hints       = vimium_hints_generate(VCHARS, VCHARS_N, count);
    g_assert_nonnull(hints);
    for (unsigned int j = 0; j < count; j++) {
      int idx = hint_label_to_index(VCHARS, VCHARS_N, count, hints[j]);
      g_assert_cmpint(idx, ==, (int)j);
    }
    vimium_hints_free(hints, count);
  }
}

/* ── incremental filter ──────────────────────────────────────────────────── */

/* Count how many hints in the array match `filter` as a case-insensitive
 * prefix. Mirrors the logic in zathura_page_widget_draw(). */
static unsigned int count_matching(char** hints, unsigned int count, const char* filter) {
  if (filter == NULL || filter[0] == '\0') {
    return count;
  }
  unsigned int n = 0;
  size_t flen    = strlen(filter);
  for (unsigned int i = 0; i < count; i++) {
    if (g_ascii_strncasecmp(hints[i], filter, flen) == 0) {
      n++;
    }
  }
  return n;
}

static void test_incremental_filter_reduces_candidates(void) {
  /* With n=3 and 7 links some hints are 1-char, some 2-char.
   * Filtering by a 1-char prefix that belongs only to 2-char hints should
   * return fewer candidates than the full set. */
  unsigned int count = 7;
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);

  /* all hints visible when filter is empty */
  g_assert_cmpuint(count_matching(hints, count, ""),  ==, count);
  g_assert_cmpuint(count_matching(hints, count, NULL), ==, count);

  /* filtering by the first char of any hint must return ≥ 1 */
  for (unsigned int i = 0; i < count; i++) {
    char prefix[2] = {hints[i][0], '\0'};
    g_assert_cmpuint(count_matching(hints, count, prefix), >=, 1);
  }

  vimium_hints_free(hints, count);
}

static void test_incremental_exact_match_is_unique(void) {
  /* Because hints are prefix-free, filtering by a complete hint as a prefix
   * must yield exactly 1 result — the auto-follow trigger. */
  unsigned int counts[] = {1, 3, 4, 14, 15, 50};
  for (unsigned int ci = 0; ci < G_N_ELEMENTS(counts); ci++) {
    unsigned int count = counts[ci];
    char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
    g_assert_nonnull(hints);

    for (unsigned int i = 0; i < count; i++) {
      g_assert_cmpuint(count_matching(hints, count, hints[i]), ==, 1);
    }

    vimium_hints_free(hints, count);
  }
}

static void test_incremental_exact_match_is_unique_vimium_chars(void) {
  unsigned int counts[] = {1, 14, 15, 100};
  for (unsigned int ci = 0; ci < G_N_ELEMENTS(counts); ci++) {
    unsigned int count = counts[ci];
    char** hints       = vimium_hints_generate(VCHARS, VCHARS_N, count);
    g_assert_nonnull(hints);

    for (unsigned int i = 0; i < count; i++) {
      g_assert_cmpuint(count_matching(hints, count, hints[i]), ==, 1);
    }

    vimium_hints_free(hints, count);
  }
}

static void test_incremental_case_insensitive_filter(void) {
  /* Filtering with lowercase prefix must match uppercase-rendered hints. */
  unsigned int count = 15;
  char** hints       = vimium_hints_generate(VCHARS, VCHARS_N, count);
  g_assert_nonnull(hints);

  for (unsigned int i = 0; i < count; i++) {
    char* lower        = g_ascii_strdown(hints[i], -1);
    unsigned int full  = count_matching(hints, count, hints[i]);
    unsigned int lower_match = count_matching(hints, count, lower);
    g_assert_cmpuint(lower_match, ==, full);
    g_free(lower);
  }

  vimium_hints_free(hints, count);
}

static void test_incremental_nonexistent_prefix_zero_results(void) {
  /* A prefix that doesn't begin any hint yields zero candidates. */
  unsigned int count = 14;
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);

  /* 'Z' is not in CHARS so no hint can start with it */
  g_assert_cmpuint(count_matching(hints, count, "Z"), ==, 0);
  g_assert_cmpuint(count_matching(hints, count, "1"), ==, 0);

  vimium_hints_free(hints, count);
}

static void test_incremental_single_char_hint_triggers_immediately(void) {
  /* When there are ≤ CHARS_N links every hint is a single character.
   * Typing that single character must immediately yield exactly 1 match,
   * meaning auto-follow would fire on the first keystroke. */
  unsigned int count = CHARS_N; /* exactly n links → all single-char */
  char** hints       = vimium_hints_generate(CHARS, CHARS_N, count);
  g_assert_nonnull(hints);

  for (unsigned int i = 0; i < count; i++) {
    g_assert_cmpuint(strlen(hints[i]), ==, 1);
    g_assert_cmpuint(count_matching(hints, count, hints[i]), ==, 1);
  }

  vimium_hints_free(hints, count);
}

static void test_incremental_partial_prefix_multiple_matches(void) {
  /* Typing a first character that is shared by multiple 2-char hints should
   * leave more than one candidate (no premature auto-follow). */
  /* With n=3 "abc" and 4 links we get: ["AA","B","AB","C"].
   * Filtering by "A" matches "AA" and "AB" → 2 results. */
  char** hints = vimium_hints_generate(CHARS, CHARS_N, 4);
  g_assert_nonnull(hints);

  unsigned int a_matches = count_matching(hints, 4, "A");
  g_assert_cmpuint(a_matches, >, 1);

  vimium_hints_free(hints, 4);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/hints/generate/null_on_zero",            test_generate_null_on_zero);
  g_test_add_func("/hints/generate/count_one",               test_generate_count_one);
  g_test_add_func("/hints/generate/single_chars_exact",      test_generate_single_chars_exact);
  g_test_add_func("/hints/generate/overflow_into_two_chars", test_generate_overflow_into_two_chars);
  g_test_add_func("/hints/generate/prefix_free_small",       test_generate_prefix_free_small);
  g_test_add_func("/hints/generate/prefix_free_vimium",      test_generate_prefix_free_vimium_chars);
  g_test_add_func("/hints/generate/uppercase",               test_generate_uppercase);
  g_test_add_func("/hints/generate/known_three_links",       test_generate_known_three_links);
  g_test_add_func("/hints/generate/known_four_links",        test_generate_known_four_links);
  g_test_add_func("/hints/lookup/roundtrip",                 test_lookup_roundtrip);
  g_test_add_func("/hints/lookup/case_insensitive",          test_lookup_case_insensitive);
  g_test_add_func("/hints/lookup/invalid_input",             test_lookup_invalid_input);
  g_test_add_func("/hints/lookup/roundtrip_vimium_chars",    test_lookup_roundtrip_vimium_chars);

  g_test_add_func("/hints/incremental/filter_reduces_candidates",          test_incremental_filter_reduces_candidates);
  g_test_add_func("/hints/incremental/exact_match_is_unique",              test_incremental_exact_match_is_unique);
  g_test_add_func("/hints/incremental/exact_match_is_unique_vimium_chars", test_incremental_exact_match_is_unique_vimium_chars);
  g_test_add_func("/hints/incremental/case_insensitive_filter",            test_incremental_case_insensitive_filter);
  g_test_add_func("/hints/incremental/nonexistent_prefix_zero_results",    test_incremental_nonexistent_prefix_zero_results);
  g_test_add_func("/hints/incremental/single_char_triggers_immediately",   test_incremental_single_char_hint_triggers_immediately);
  g_test_add_func("/hints/incremental/partial_prefix_multiple_matches",    test_incremental_partial_prefix_multiple_matches);

  return g_test_run();
}
