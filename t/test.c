#include "common.h"

START_TEST(test_chunk)
{
    chunk_t chunk;
    init_chunk(&chunk);

    const int line = 1;
    write_chunk(&chunk, OP_CONSTANT_LONG, line);
    write_chunk(&chunk, (uint8_t)1, line);

    const int index = 656666;
    write_chunk(&chunk, (uint8_t)(index & 0xff), line);
    write_chunk(&chunk, (uint8_t)((index >> 8) & 0xff), line);
    write_chunk(&chunk, (uint8_t)((index >> 16) & 0xff), line);

    add_constant(&chunk, (value_t){.type=VAL_NUMBER, .as={.number=9}});

    int rline = get_line(&chunk, 2);
    ck_assert_int_eq(rline, 1);

    free_chunk(&chunk);
}

START_TEST(test_scanner)
{
    const char *source = "var foo = \"foo\"; { var bar = \"bar\"; var foobar = foo + bar; print foobar;}";
    const token_t results[] = {
        {.type=TOKEN_VAR, .start="var", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="foo", .length=3, .line=1},
        {.type=TOKEN_EQUAL, .start="=", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"foo\"", .length=5, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_LEFT_BRACE, .start="{", .length=1, .line=1},
        {.type=TOKEN_VAR, .start="var", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="bar", .length=3, .line=1},
        {.type=TOKEN_EQUAL, .start="=", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"bar\"", .length=5, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_VAR, .start="var", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="foobar", .length=6, .line=1},
        {.type=TOKEN_EQUAL, .start="=", .length=1, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="foo", .length=3, .line=1},
        {.type=TOKEN_PLUS, .start="+", .length=1, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="bar", .length=3, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_PRINT, .start="print", .length=5, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="foobar", .length=6, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_RIGHT_BRACE, .start="}", .length=1, .line=1},
        {.type=TOKEN_EOF, .start="", .length=0, .line=1},
    };
    init_scanner(source);
    for (int idx = 0;results[idx].type != TOKEN_EOF;idx++) {
        token_t t = scan_token();
        ck_assert(t.type == results[idx].type);
        ck_assert(memcmp(t.start, results[idx].start, t.length) == 0);
        ck_assert(t.length == results[idx].length);
        ck_assert(t.line == results[idx].line);
    }
}

START_TEST(test_compiler)
{
    const char *source = "var v = 27; var foo = \"foo\"; { var bar = \"bar\"; var foobar = foo + bar; print foobar; var t = true;}";
    chunk_t chunk;
    init_chunk(&chunk);
    ck_assert(compile(source, &chunk));

    /*
    0000    1 OP_CONSTANT      1 '27'
    0002    | OP_DEFINE_GLOBAL 0 'v'
    0004    | OP_CONSTANT      2 'foo'
    0006    | OP_DEFINE_GLOBAL 0 'v'
    0008    | OP_CONSTANT      3 'bar'
    0010    | OP_GET_GLOBAL    0 'v'
    0012    | OP_GET_LOCAL        0
    0014    | OP_ADD
    0015    | OP_GET_LOCAL        1
    0017    | OP_PRINT
    0018    | OP_TRUE
    0019    | OP_POPN             3
    0021    | OP_RETURN
    */
    ck_assert(chunk.count == 22);
    ck_assert(chunk.code[ 0] == OP_CONSTANT);
    ck_assert(chunk.code[ 2] == OP_DEFINE_GLOBAL);
    ck_assert(chunk.code[ 4] == OP_CONSTANT);
    ck_assert(chunk.code[ 6] == OP_DEFINE_GLOBAL);
    ck_assert(chunk.code[ 8] == OP_CONSTANT);
    ck_assert(chunk.code[10] == OP_GET_GLOBAL);
    ck_assert(chunk.code[12] == OP_GET_LOCAL);
    ck_assert(chunk.code[14] == OP_ADD);
    ck_assert(chunk.code[15] == OP_GET_LOCAL);
    ck_assert(chunk.code[17] == OP_PRINT);
    ck_assert(chunk.code[18] == OP_TRUE);
    ck_assert(chunk.code[19] == OP_POPN);
    ck_assert(chunk.code[21] == OP_RETURN);

    ck_assert(chunk.constants.count == 4); // v, 27, foo, "bar"
    ck_assert(chunk.constants.values[0].type == VAL_OBJ);
    ck_assert(memcmp(AS_CSTRING(chunk.constants.values[0]), "v", 1) == 0);

    ck_assert(chunk.constants.values[1].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(chunk.constants.values[1]) == 27);

    ck_assert(chunk.constants.values[2].type == VAL_OBJ);
    ck_assert(memcmp(AS_CSTRING(chunk.constants.values[2]), "foo", 4) == 0);

    ck_assert(chunk.constants.values[3].type == VAL_OBJ);
    ck_assert(memcmp(AS_CSTRING(chunk.constants.values[3]), "bar", 4) == 0);

    free_chunk(&chunk);
}

START_TEST(test_vm)
{
    init_vm();
    const char *test_cases[] = {
        "print 1+2; print 3-1; print 4/2; print 10*10; print 1 == 1; print 2 != 4;",
        "print 2<4; print 4>2; print 4>=4; print 8<=9; print (!true);",
        "print false; print true; print nil;",
        "var foo = \"foo\"; { var bar = \"bar\"; var foobar = foo + bar; print foobar;}",
        "var foo = 10; var result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
        "var counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "if (false or true) { print \"yep\"; }",
        "if (!false and true) { print \"yep\"; }",
        "for(var i = 0; i < 5; i = i + 1) { print i;}",
        NULL,
    };
    for (int i = 0; test_cases[i] != NULL; i++) {
        ck_assert(interpret(test_cases[i]) == INTERPRET_OK);
    }

    const char *compilation_fail_cases[] = {
        "var;",
        "var foo = 1",
        "{var foo = foo;}",
        // "}{",
        "if true ){}",
        NULL,
    };
    for (int i = 0; compilation_fail_cases[i] != NULL; i++) {
        ck_assert(interpret(compilation_fail_cases[i]) == INTERPRET_COMPILE_ERROR);
    }

    const char *runtime_fail_cases[] = {
        NULL,
    };
    for (int i = 0; runtime_fail_cases[i] != NULL; i++) {
        ck_assert(interpret(runtime_fail_cases[i]) == INTERPRET_RUNTRIME_ERROR);
    }

    free_vm();
}

START_TEST(test_value)
{
    ck_assert(hash_value(BOOL_VAL(true)) == 3);
    ck_assert(hash_value(BOOL_VAL(false)) == 5);
    ck_assert(hash_value(NIL_VAL) == 7);
    ck_assert(hash_value(EMPTY_VAL) == 0);
    ck_assert(hash_value(NUMBER_VAL(9)) == 1076101120);

    ck_assert(values_equal(NUMBER_VAL(100), NUMBER_VAL(100)));
    ck_assert(!values_equal(NUMBER_VAL(100), NUMBER_VAL(200)));
    ck_assert(values_equal(BOOL_VAL(true), BOOL_VAL(true)));
    ck_assert(!values_equal(BOOL_VAL(true), BOOL_VAL(false)));

    value_array_t a;
    init_value_array_t(&a);
    write_value_array_t(&a, NUMBER_VAL(9));
    write_value_array_t(&a, BOOL_VAL(false));
    write_value_array_t(&a, NIL_VAL);
    write_value_array_t(&a, EMPTY_VAL);
    for (int i = 0; i < a.count; i++) {
        print_value(a.values[i]);
    }
    free_value_array_t(&a);
}

START_TEST(test_table)
{
    table_t t;
    init_table_t(&t);

    ck_assert(set_table_t(&t, NUMBER_VAL(1), NUMBER_VAL(10)));
    ck_assert(set_table_t(&t, NUMBER_VAL(2), NUMBER_VAL(20)));
    ck_assert(set_table_t(&t, NUMBER_VAL(3), NUMBER_VAL(30)));
    ck_assert(delete_table_t(&t, NUMBER_VAL(3)));
    ck_assert(!delete_table_t(&t, NUMBER_VAL(3)));

    table_t tcopy;
    init_table_t(&tcopy);
    add_all_table_t(&t, &tcopy);

    for (int i = 0; i < t.count; i++) {
        value_t from_t;
        value_t from_tcopy;
        ck_assert(get_table_t(&t, NUMBER_VAL(1), &from_t));
        ck_assert(get_table_t(&tcopy, NUMBER_VAL(1), &from_tcopy));
        ck_assert(values_equal(from_t, from_tcopy));
    }

    free_table_t(&t);
    free_table_t(&tcopy);
}

START_TEST(test_object)
{
    obj_string_t *str = copy_string("foobar", 7);
    ck_assert(memcmp(str->chars, "foobar", str->length) == 0);
    print_object(OBJ_VAL(str));

    obj_string_t *p1 = copy_string("foo", 3);
    obj_string_t *p2 = copy_string("bar", 3);
    obj_string_t *combined = concatenate_string(p1, p2);
    ck_assert(combined->length == (p1->length + p2->length));
    ck_assert(memcmp(combined->chars, "foobar", combined->length) == 0);

    FREE(obj_string_t, str);
    FREE(obj_string_t, p1);
    FREE(obj_string_t, p2);
    //FREE(obj_string_t, combined);
}

int main(void)
{
    int number_failed;
    Suite *s = suite_create("clox");
    SRunner *sr = srunner_create(s);

    TCase *tc = tcase_create("chunk");
    tcase_add_test(tc, test_chunk);
    suite_add_tcase(s, tc);

    tc = tcase_create("scanner");
    tcase_add_test(tc, test_scanner);
    suite_add_tcase(s, tc);

    tc = tcase_create("compiler");
    tcase_add_test(tc, test_compiler);
    suite_add_tcase(s, tc);

    tc = tcase_create("vm");
    tcase_add_test(tc, test_vm);
    suite_add_tcase(s, tc);

    tc = tcase_create("value");
    tcase_add_test(tc, test_value);
    suite_add_tcase(s, tc);

    tc = tcase_create("table");
    tcase_add_test(tc, test_table);
    suite_add_tcase(s, tc);

    tc = tcase_create("object");
    tcase_add_test(tc, test_object);
    suite_add_tcase(s, tc);

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
