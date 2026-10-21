/**
 * ============================================================================
 * Project: Huffman Coding-Based File Compression System
 * Course:  Computer Engineering — Data Structures and Algorithms
 * Student: Shalom Dabre
 * Roll No: 11070
 * 
 * File:    tests/test_huffman.c
 * Purpose: Comprehensive automated test harness covering all 10 required test
 *          scenarios, data structure unit tests, edge cases, corruption,
 *          and byte-by-byte round-trip integrity verification.
 * ============================================================================
 */

#include "huffman.h"
#include "min_heap.h"
#include "bit_io.h"
#include "file_handler.h"
#include "crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* Helper to generate a test file with given content */
static int create_text_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (content && strlen(content) > 0) {
        fputs(content, f);
    }
    fclose(f);
    return 1;
}

/* Helper to generate a large text file */
static int create_large_text_file(const char *path, size_t target_size) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const char *sample_sentence = 
        "Data Structures and Algorithms form the foundation of computer science and software engineering. "
        "Huffman coding is an optimal prefix code algorithm used for lossless data compression. "
        "Invented by David A. Huffman in 1952, it assigns variable-length codes to input characters based on their frequencies. "
        "More frequent characters receive shorter bit sequences, while less frequent characters receive longer bit sequences.\n";
    size_t written = 0;
    size_t s_len = strlen(sample_sentence);
    while (written < target_size) {
        fwrite(sample_sentence, 1, s_len, f);
        written += s_len;
    }
    fclose(f);
    return 1;
}

/* Helper to perform a round-trip test and byte-by-byte check */
static int run_round_trip(const char *src_path, const char *comp_path, const char *decomp_path, const char *desc) {
    CompressionStats cstats;
    HuffmanStatus c_res = compress_file(src_path, comp_path, &cstats, 1);
    if (c_res != HUFF_SUCCESS) {
        printf("  [FAIL] %s - Compression failed with error: %s\n", desc, huffman_status_string(c_res));
        return 0;
    }

    CompressionStats dstats;
    HuffmanStatus d_res = decompress_file(comp_path, decomp_path, &dstats, 1);
    if (d_res != HUFF_SUCCESS) {
        printf("  [FAIL] %s - Decompression failed with error: %s\n", desc, huffman_status_string(d_res));
        return 0;
    }

    uint64_t diff = 0;
    int identical = compare_files_exact(src_path, decomp_path, &diff);
    if (identical != 1) {
        printf("  [FAIL] %s - Restored file differs at byte offset %llu\n", desc, (unsigned long long)diff);
        return 0;
    }

    if (cstats.original_crc != dstats.decompressed_crc) {
        printf("  [FAIL] %s - CRC mismatch (Original: 0x%08X, Restored: 0x%08X)\n",
               desc, cstats.original_crc, dstats.decompressed_crc);
        return 0;
    }

    return 1;
}

/* ========================================================================= */
/* UNIT TESTS FOR DATA STRUCTURES                                            */
/* ========================================================================= */

static void test_min_heap_operations(void) {
    printf("\n=== UNIT TEST 1: Min-Heap Priority Queue Operations ===\n");
    MinHeap *heap = create_min_heap(4);
    TEST_ASSERT(heap != NULL, "create_min_heap allocates valid heap structure");
    TEST_ASSERT(is_heap_empty(heap), "Newly created heap is empty");

    HuffmanNode *n1 = create_leaf_node('a', 50, 1);
    HuffmanNode *n2 = create_leaf_node('b', 10, 2);
    HuffmanNode *n3 = create_leaf_node('c', 30, 3);
    HuffmanNode *n4 = create_leaf_node('d', 5, 4);
    HuffmanNode *n5 = create_leaf_node('e', 20, 5);

    insert_min_heap(heap, n1);
    insert_min_heap(heap, n2);
    insert_min_heap(heap, n3);
    insert_min_heap(heap, n4);
    insert_min_heap(heap, n5); /* Tests automatic resizing beyond initial capacity 4 */

    TEST_ASSERT(heap->size == 5, "Min-heap dynamic resizing and insertion succeeds");

    /* Extracting elements must return them in strictly non-decreasing order of frequency */
    HuffmanNode *e1 = extract_min(heap);
    TEST_ASSERT(e1 != NULL && e1->freq == 5 && e1->data == 'd', "extract_min #1 returned minimum node (freq 5)");

    HuffmanNode *e2 = extract_min(heap);
    TEST_ASSERT(e2 != NULL && e2->freq == 10 && e2->data == 'b', "extract_min #2 returned minimum node (freq 10)");

    HuffmanNode *e3 = extract_min(heap);
    TEST_ASSERT(e3 != NULL && e3->freq == 20 && e3->data == 'e', "extract_min #3 returned minimum node (freq 20)");

    HuffmanNode *e4 = extract_min(heap);
    TEST_ASSERT(e4 != NULL && e4->freq == 30 && e4->data == 'c', "extract_min #4 returned minimum node (freq 30)");

    HuffmanNode *e5 = extract_min(heap);
    TEST_ASSERT(e5 != NULL && e5->freq == 50 && e5->data == 'a', "extract_min #5 returned minimum node (freq 50)");

    TEST_ASSERT(is_heap_empty(heap), "Min-heap is empty after extracting all elements");

    free(n1); free(n2); free(n3); free(n4); free(n5);
    destroy_min_heap(heap);
}

static void test_huffman_tree_and_codes(void) {
    printf("\n=== UNIT TEST 2: Huffman Tree Construction & Prefix Codes ===\n");
    uint64_t frequencies[256] = {0};
    /* Standard textbook example: A: 45, B: 13, C: 12, D: 16, E: 9, F: 5 */
    frequencies['A'] = 45;
    frequencies['B'] = 13;
    frequencies['C'] = 12;
    frequencies['D'] = 16;
    frequencies['E'] = 9;
    frequencies['F'] = 5;

    HuffmanNode *root = build_huffman_tree(frequencies);
    TEST_ASSERT(root != NULL, "build_huffman_tree returns valid root node");
    TEST_ASSERT(root->freq == 100, "Root node frequency equals sum of character frequencies (100)");

    HuffmanCodeTable table;
    generate_huffman_codes(root, &table);
    TEST_ASSERT(table.distinct_count == 6, "Code table has 6 distinct symbols");
    TEST_ASSERT(table.exists['A'] && table.codes['A'].length > 0, "Character 'A' has valid code");
    TEST_ASSERT(table.exists['F'] && table.codes['F'].length > table.codes['A'].length,
                "Least frequent character 'F' receives longer code than most frequent 'A'");

    /* Verify prefix property: no code can be a prefix of any other code */
    int prefix_violation = 0;
    for (int i = 0; i < 256; i++) {
        if (!table.exists[i]) continue;
        for (int j = 0; j < 256; j++) {
            if (i == j || !table.exists[j]) continue;
            size_t len_i = table.codes[i].length;
            if (table.codes[j].length >= len_i &&
                strncmp(table.codes[i].bits, table.codes[j].bits, len_i) == 0) {
                prefix_violation = 1;
                break;
            }
        }
        if (prefix_violation) break;
    }
    TEST_ASSERT(!prefix_violation, "Huffman codes strictly satisfy the Prefix (Fano) condition");

    free_huffman_tree(root);
}

/* ========================================================================= */
/* SYSTEM & END-TO-END TESTS (ALL 10 SCENARIOS)                             */
/* ========================================================================= */

static void test_10_required_scenarios(void) {
    printf("\n=== 10 REQUIRED SYSTEM & EDGE-CASE TEST SCENARIOS ===\n");

    /* --------------------------------------------------------------------- */
    /* Scenario 1: Empty input (0 bytes)                                     */
    /* --------------------------------------------------------------------- */
    const char *f1_src = "output/test1_empty.txt";
    const char *f1_cmp = "output/test1_empty.txt.huf";
    const char *f1_dec = "output/test1_empty.txt.restored";
    create_text_file(f1_src, "");
    int res1 = run_round_trip(f1_src, f1_cmp, f1_dec, "Scenario 1: Empty input (0 bytes)");
    TEST_ASSERT(res1, "Scenario 1: Empty input round-trip matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 2: One repeated character                                    */
    /* --------------------------------------------------------------------- */
    const char *f2_src = "output/test2_single.txt";
    const char *f2_cmp = "output/test2_single.txt.huf";
    const char *f2_dec = "output/test2_single.txt.restored";
    FILE *f2 = fopen(f2_src, "wb");
    for (int i = 0; i < 2000; i++) fputc('Z', f2);
    fclose(f2);
    int res2 = run_round_trip(f2_src, f2_cmp, f2_dec, "Scenario 2: Single repeated character");
    TEST_ASSERT(res2, "Scenario 2: Single repeated character (2000 Zs) matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 3: Multiple repeated characters                              */
    /* --------------------------------------------------------------------- */
    const char *f3_src = "output/test3_repeats.txt";
    const char *f3_cmp = "output/test3_repeats.txt.huf";
    const char *f3_dec = "output/test3_repeats.txt.restored";
    FILE *f3 = fopen(f3_src, "wb");
    for (int i = 0; i < 500; i++) fputs("AAAAAABBBBBCCCCCDDDDDEEEEE", f3);
    fclose(f3);
    int res3 = run_round_trip(f3_src, f3_cmp, f3_dec, "Scenario 3: Multiple repeated characters");
    TEST_ASSERT(res3, "Scenario 3: Multiple repeated characters matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 4: Spaces, punctuation, and newlines                         */
    /* --------------------------------------------------------------------- */
    const char *f4_src = "output/test4_prose.txt";
    const char *f4_cmp = "output/test4_prose.txt.huf";
    const char *f4_dec = "output/test4_prose.txt.restored";
    const char *prose_text = 
        "To be, or not to be, that is the question:\n"
        "Whether 'tis nobler in the mind to suffer\n"
        "The slings and arrows of outrageous fortune,\n"
        "Or to take arms against a sea of troubles\n"
        "And by opposing end them. To die - to sleep;\n"
        "No more; and by a sleep to say we end\n"
        "The heart-ache and the thousand natural shocks\n"
        "That flesh is heir to: 'tis a consummation\n"
        "Devoutly to be wish'd.\t\t[Act III, Scene 1]\n";
    create_text_file(f4_src, prose_text);
    int res4 = run_round_trip(f4_src, f4_cmp, f4_dec, "Scenario 4: Spaces, punctuation, newlines, tabs");
    TEST_ASSERT(res4, "Scenario 4: Complex text with whitespace & punctuation matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 5: All 256 possible byte values                              */
    /* --------------------------------------------------------------------- */
    const char *f5_src = "output/test5_all_bytes.bin";
    const char *f5_cmp = "output/test5_all_bytes.bin.huf";
    const char *f5_dec = "output/test5_all_bytes.bin.restored";
    FILE *f5 = fopen(f5_src, "wb");
    for (int rep = 0; rep < 50; rep++) {
        for (int b = 0; b < 256; b++) {
            fputc((unsigned char)b, f5);
        }
    }
    fclose(f5);
    int res5 = run_round_trip(f5_src, f5_cmp, f5_dec, "Scenario 5: All 256 possible byte values");
    TEST_ASSERT(res5, "Scenario 5: Binary file with all 256 byte values matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 6: Larger text file (~50 KB)                                 */
    /* --------------------------------------------------------------------- */
    const char *f6_src = "output/test6_large.txt";
    const char *f6_cmp = "output/test6_large.txt.huf";
    const char *f6_dec = "output/test6_large.txt.restored";
    create_large_text_file(f6_src, 50 * 1024);
    int res6 = run_round_trip(f6_src, f6_cmp, f6_dec, "Scenario 6: Larger text file (~50 KB)");
    TEST_ASSERT(res6, "Scenario 6: Larger text file matches byte-by-byte");

    /* --------------------------------------------------------------------- */
    /* Scenario 7: Corrupted compressed file                                 */
    /* --------------------------------------------------------------------- */
    const char *f7_src = "output/test7_corrupt.huf";
    const char *f7_dec = "output/test7_corrupt.restored";
    /* Copy valid compressed file from test4 */
    FILE *src_valid = fopen(f4_cmp, "rb");
    FILE *dst_corrupt = fopen(f7_src, "wb");
    if (src_valid && dst_corrupt) {
        int ch;
        int idx = 0;
        while ((ch = fgetc(src_valid)) != EOF) {
            /* Corrupt magic signature byte: change 'H' to 'X' */
            if (idx == 0) ch = 'X';
            fputc(ch, dst_corrupt);
            idx++;
        }
        fclose(src_valid);
        fclose(dst_corrupt);
    }
    CompressionStats cstats7;
    HuffmanStatus res7 = decompress_file(f7_src, f7_dec, &cstats7, 1);
    TEST_ASSERT(res7 == HUFF_ERR_INVALID_MAGIC, "Scenario 7: Corrupted magic signature safely rejected");

    /* --------------------------------------------------------------------- */
    /* Scenario 8: Truncated compressed file                                 */
    /* --------------------------------------------------------------------- */
    const char *f8_src = "output/test8_truncated.huf";
    const char *f8_dec = "output/test8_truncated.restored";
    FILE *src_valid8 = fopen(f6_cmp, "rb");
    FILE *dst_trunc = fopen(f8_src, "wb");
    if (src_valid8 && dst_trunc) {
        /* Truncate file after writing only 100 bytes (header + partial payload) */
        for (int i = 0; i < 100; i++) {
            int ch = fgetc(src_valid8);
            if (ch == EOF) break;
            fputc(ch, dst_trunc);
        }
        fclose(src_valid8);
        fclose(dst_trunc);
    }
    CompressionStats cstats8;
    HuffmanStatus res8 = decompress_file(f8_src, f8_dec, &cstats8, 1);
    TEST_ASSERT(res8 == HUFF_ERR_TRUNCATED_FILE, "Scenario 8: Truncated compressed file safely rejected");

    /* --------------------------------------------------------------------- */
    /* Scenario 9: Invalid input path                                        */
    /* --------------------------------------------------------------------- */
    CompressionStats cstats9;
    HuffmanStatus res9 = compress_file("non_existent_directory/fake_file.txt", "output/fake.huf", &cstats9, 1);
    TEST_ASSERT(res9 == HUFF_ERR_FILE_NOT_FOUND, "Scenario 9: Non-existent input path safely handled");

    /* --------------------------------------------------------------------- */
    /* Scenario 10: Compression followed by decompression round-trip         */
    /* --------------------------------------------------------------------- */
    const char *f10_src = "output/test10_roundtrip.txt";
    const char *f10_cmp = "output/test10_roundtrip.txt.huf";
    const char *f10_dec = "output/test10_roundtrip.txt.restored";
    create_text_file(f10_src, "Shalom Dabre, Roll 11070. Data Structures and Algorithms Mini-Project Final Verification.");
    int res10 = run_round_trip(f10_src, f10_cmp, f10_dec, "Scenario 10: End-to-end verification");
    TEST_ASSERT(res10, "Scenario 10: Complete round-trip compression and decompression verified");
}

int main(void) {
    printf("==================================================================\n");
    printf("      HUFFMAN FILE COMPRESSION SYSTEM - AUTOMATED TEST SUITE      \n");
    printf("==================================================================\n");
    printf(" Student Name : Shalom Dabre\n");
    printf(" Roll Number  : 11070\n");
    printf(" Course       : Computer Engineering - Data Structures & Algorithms\n");
    printf("==================================================================\n");

    test_min_heap_operations();
    test_huffman_tree_and_codes();
    test_10_required_scenarios();

    printf("\n==================================================================\n");
    printf(" TEST EXECUTION SUMMARY: %d / %d Tests Passed (%.1f%%)\n",
           g_tests_passed, g_tests_run, ((double)g_tests_passed / (double)g_tests_run) * 100.0);
    printf("==================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
