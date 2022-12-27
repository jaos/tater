#include <unistd.h>
#include <check.h>

#include "common.h"

START_TEST(test_chunk)
{
    init_vm(); // now chunk and vm are tied together
    chunk_t chunk;
    init_chunk(&chunk);

    write_chunk(&chunk, OP_RETURN, 1);

    const int line = 1;
    write_chunk(&chunk, OP_CONSTANT, line);
    write_chunk(&chunk, (uint8_t)1, line);

    const int index = 656666;
    write_chunk(&chunk, (uint8_t)(index & 0xff), line);
    write_chunk(&chunk, (uint8_t)((index >> 8) & 0xff), line);
    write_chunk(&chunk, (uint8_t)((index >> 16) & 0xff), line);

    add_constant(&chunk, (value_t){.type=VAL_NUMBER, .as={.number=9}});

    /*
    int rline = get_line(&chunk, 2);
    ck_assert_int_eq(rline, 1);
    */

    free_chunk(&chunk);
    free_vm();
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
        const char *token_type_str __unused__ = token_type_t_to_str(t.type);
        ck_assert_msg(t.type == results[idx].type, "Expected %d, got %d\n", results[idx].type, t.type);
        ck_assert(memcmp(t.start, results[idx].start, t.length) == 0);
        ck_assert(t.length == results[idx].length);
        ck_assert(t.line == results[idx].line);
    }


    source = "fun f() { f(\"too\", \"many\"); }";
    init_scanner(source);
    const token_t results2[] = {
        {.type=TOKEN_FUN, .start="fun", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="f", .length=1, .line=1},
        {.type=TOKEN_LEFT_PAREN, .start="(", .length=1, .line=1},
        {.type=TOKEN_RIGHT_PAREN, .start=")", .length=1, .line=1},
        {.type=TOKEN_LEFT_BRACE, .start="{", .length=1, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="f", .length=1, .line=1},
        {.type=TOKEN_LEFT_PAREN, .start="(", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"too\"", .length=5, .line=1},
        {.type=TOKEN_COMMA, .start=",", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"many\"", .length=6, .line=1},
        {.type=TOKEN_RIGHT_PAREN, .start=")", .length=1, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_RIGHT_BRACE, .start="}", .length=1, .line=1},
        {.type=TOKEN_EOF, .start="", .length=0, .line=1},
    };
    for (int idx = 0;results2[idx].type != TOKEN_EOF;idx++) {
        token_t t = scan_token();
        const char *token_type_str __unused__ = token_type_t_to_str(t.type);

        ck_assert_msg(t.type == results2[idx].type, "Expected %d, got %d\n", results2[idx].type, t.type);
        ck_assert(memcmp(t.start, results2[idx].start, t.length) == 0);
        ck_assert(t.length == results2[idx].length);
        ck_assert(t.line == results2[idx].line);
    }

    source = "fun p() { return 1;}; for(var i = 0; i <= 3; i = i + 1) { !(true and false); false or true; if (i == 2) { print(i); } else {print(p()); continue;}}";
    init_scanner(source);
    token_t next = scan_token();
    do {
        const char *token_type_str __unused__ = token_type_t_to_str(next.type);
        next = scan_token();
    } while (next.type != TOKEN_EOF);

    const char *invalid_sources[] = {
        "~",
        NULL,
    };
    for (int i = 0; invalid_sources[i] != NULL; i++) {
        bool found_invalid_token = false;
        init_scanner(invalid_sources[i]);
        token_t maybe_next = scan_token();
        do {
            if (maybe_next.type == TOKEN_ERROR) {
                found_invalid_token = true;
            }
            maybe_next = scan_token();
        } while (maybe_next.type != TOKEN_EOF);
        ck_assert_msg(found_invalid_token, "Failed to find invalid token in \"%s\"", invalid_sources[i]);
    }
}

START_TEST(test_compiler)
{
    init_vm();
    const char *source1 = "var v = 27; { var v = 1; var y = 2; var z = v + y; }";
    obj_function_t *func1 = compile(source1);
    ck_assert(func1->chunk.count == 18);
    const op_code_t r1[] = {
        OP_CONSTANT,
        OP_NIL,
        OP_DEFINE_GLOBAL,
        OP_CONSTANT,
        OP_CONSTANT,
        OP_TRUE,
        OP_CONSTANT,
        OP_FALSE,
        OP_GET_LOCAL,
        OP_NIL,
        OP_GET_LOCAL,
        OP_TRUE,
        OP_ADD,
        OP_POP,
        OP_POP,
        OP_POP,
        OP_NIL,
        OP_RETURN,
        OP_CONSTANT,
    };
    for (int i = 0; i < func1->chunk.count; i++) {
        ck_assert_msg(func1->chunk.code[i] == r1[i], "Expected %d, got %d\n", r1[i], func1->chunk.code[i]);
        const char *op_str __unused__ = op_code_t_to_str(func1->chunk.code[i]);
    }

    ck_assert(func1->chunk.constants.count == 4); // v, 27, 1, 2
    ck_assert(memcmp(AS_CSTRING(func1->chunk.constants.values[0]), "v", 1) == 0);
    ck_assert(func1->chunk.constants.values[1].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[1]) == 27);
    ck_assert(func1->chunk.constants.values[2].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[2]) == 1);
    ck_assert(func1->chunk.constants.values[3].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[3]) == 2);

    free_vm();


    init_vm();
    const char *func_source = "fun a(x,y) { var sum = x + y; print(sum);}";
    obj_function_t *func2 = compile(func_source);
    ck_assert(func2->chunk.count == 6);
    const op_code_t r2[] = {
        OP_CLOSURE,
        OP_NIL,
        OP_DEFINE_GLOBAL,
        OP_CONSTANT,
        OP_NIL,
        OP_RETURN,
    };
    for (int i = 0; i < func2->chunk.count; i++) {
        ck_assert_msg(func2->chunk.code[i] == r2[i], "Expected %d, got %d\n", r2[i], func2->chunk.code[i]);
        const char *op_str __unused__ = op_code_t_to_str(func2->chunk.code[i]);
    }
    free_vm();

    const char *programs[] = {
        "for(var i = 0; i < 5; i = i + 1) { print i; var v = 1; v = v + 2; v = v / 3; v = v * 4;}",
        "var counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "var foo = 10; var result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
        NULL,
    };
    for (int p = 0; programs[p] != NULL; p++) {
        init_vm();
        obj_function_t *program_func = compile(programs[p]);
        for (int f = 0; f < program_func->chunk.count; f++) {
            const char *op_str __unused__ = op_code_t_to_str(program_func->chunk.code[f]);
        }
        free_vm();
    }
}

START_TEST(test_vm)
{
    const char *test_cases[] = {
        // not in lox
        "switch(3) { case 0: print(0); case 1: print(1); case 2: print(2); default: true; }",
        "switch(3) { default: print(0); }",
        "switch(3) { case 3: print(3); }",
        "switch(3) { }",
        // "var counter = 0; while (counter < 10) { break; print counter; counter = counter + 1;}", // DOES NOT YET WORK
        "var counter = 0; while (counter < 10) { counter = counter + 1; continue; print \"never reached\";}",

        "print 1+2; print 3-1; print 4/2; print 10*10; print 1 == 1; print 2 != 4;",
        "print 2<4; print 4>2; print 4>=4; print 8<=9; print (!true);",
        "print false; print true; print nil;",
        "var foo = \"foo\"; { var bar = \"bar\"; var foobar = foo + bar; print foobar;}",
        "var foo = 10; var result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
        "var counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "if (false or true) { print \"yep\"; }",
        "if (!false and true) { print \"yep\"; }",
        "for(var i = 0; i < 5; i = i + 1) { print i;}",
        "var counter = 0; for(1; counter < 5; counter = counter + 1) { print counter;}",
        "fun a() { print 1;} a();", // chapter 24
        "print clock();", // chapter 24
        "fun mk() {var l = \"local\"; fun inner() {print l;}return inner;} var closure = mk(); closure();", // chapter 25
        "fun outer() {var x = 1; x = 2;fun inner() {print x;} inner(); } outer();", // chapter 25
        "fun novalue() { return; } novalue();",

        "fun outer(){"
            "var x = 1; "
            "fun middle() { "
                "fun inner() {"
                    "print x;"
                "}"
                "print \"create inner closure\";"
                "return inner;"
            "}"
            "print \"return from outer\";"
            "return middle;"
        "} var mid = outer(); var in = mid(); in();", // chapter 25

        "var globalSet; "
        "var globalGet; "
        "fun main() { "
        "    var a = 1; var b = 100;"
        "    fun set() { a = 2; print b;} "
        "    fun get() { print a; b = 101;} "
        "    globalSet = set; "
        "    globalGet = get; "
        "} "
        "main(); "
        "globalSet(); "
        "globalGet();", // chapter 25

        "fun makeClosure() {\n"
        "    var a = \"data\";\n"
        "    fun f() { print a; }\n"
        "    return f;\n"
        "}\n"
        "{\n"
        "    var closure = makeClosure();\n"
        "    // GC here.\n"
        "    closure();\n"
        "}\n", // chapter 26

        // https://github.com/munificent/craftinginterpreters/issues/888
        "fun returnArg(arg){ return arg;}"
        "fun returnFunCallWithArg(func, arg){return returnArg(func)(arg);}"
        "fun printArg(arg){print arg;}"
        "returnFunCallWithArg(printArg, \"hello world\");",

        // https://github.com/munificent/craftinginterpreters/issues/746
        "var f1; var f2; { var i = 1; fun f() { print i; } f1 = f; } { var j = 2; fun f() { print j; } f2 = f; } f1(); f2();",

        // trigger gc
        "fun lots_of_stuff() { var a = \"asdfasdfasdfasdfasdafsdfasdfasdfafs\"; var b = \"asdfsdfasdfasfdafsdfas\"; }"
        "for (var i = 0; i < 100; i = i + 1) { lots_of_stuff();}",

        "class Brioche {} print Brioche; print Brioche();", // chapter 27
        "class Pair {} var pair = Pair(); pair.first = 1; pair.second = 2; print pair.first + pair.second;", // chapter 27

        NULL,
    };
    for (int i = 0; test_cases[i] != NULL; i++) {
        init_vm();
        ck_assert_msg(interpret(test_cases[i]) == INTERPRET_OK,
            "test case failed for \"%s\"\n", test_cases[i]);
        free_vm();
    }

    const char *compilation_fail_cases[] = {
        // NOT IN LOX
        "continue;",

        "var;",
        "var foo = 1",
        "{var foo = foo;}",
        // "}{",
        "if true ){}",
        " 1 = 3;",
        "{ var a = 1; var a = 2;}",
        NULL,
    };
    for (int i = 0; compilation_fail_cases[i] != NULL; i++) {
        init_vm();
        ck_assert_msg(interpret(compilation_fail_cases[i]) == INTERPRET_COMPILE_ERROR,
            "Unexpected success for \"%s\"\n", compilation_fail_cases[i]);
        free_vm();
    }

    const char *runtime_fail_cases[] = {
        "fun no_args() {} no_args(1);", // arguments
        "fun has_args(v) {print(v);} has_args();", // arguments
        "var not_callable = 1; not_callable();", // cannot call
        "print(undefined_global);", // undefined
        "{ print(undefined_local);}",
        "var a = \"foo\"; a = -a;", // operand not a number
        "var a = \"foo\"; a = a + 1;", // operands must be same
        "a = 1;", // set global undefined variable
        NULL,
    };
    for (int i = 0; runtime_fail_cases[i] != NULL; i++) {
        init_vm();
        ck_assert_msg(interpret(runtime_fail_cases[i]) == INTERPRET_RUNTIME_ERROR,
            "Unexpected success for \"%s\"\n", runtime_fail_cases[i]);
        free_vm();
    }
}

static value_t native_getpid(int, value_t*)
{
    return NUMBER_VAL((double)getpid());
}

START_TEST(test_native)
{
    init_vm();

    define_native("getpid", native_getpid);
    ck_assert_int_gt(AS_NUMBER(native_getpid(0, NULL)), 0);
    ck_assert(interpret("print(getpid());") == INTERPRET_OK);

    obj_native_t *native_fn = new_obj_native_t(native_getpid);
    push(OBJ_VAL(native_fn));
    obj_type_t_to_str(((obj_t*)native_fn)->type); // debugging

    free_vm();
}

START_TEST(test_value)
{
    init_vm();

    ck_assert(hash_value(BOOL_VAL(true)) == 3);
    ck_assert(hash_value(BOOL_VAL(false)) == 5);
    ck_assert(hash_value(NIL_VAL) == 7);
    ck_assert(hash_value(EMPTY_VAL) == 0);
    ck_assert(hash_value(NUMBER_VAL(9)) == 1076101120);

    value_type_t_to_str(VAL_BOOL);
    value_type_t_to_str(VAL_NIL);
    value_type_t_to_str(VAL_NUMBER);
    value_type_t_to_str(VAL_OBJ);
    value_type_t_to_str(VAL_EMPTY);

    ck_assert(values_equal(NUMBER_VAL(100), NUMBER_VAL(100)));
    ck_assert(!values_equal(NUMBER_VAL(100), NUMBER_VAL(200)));
    ck_assert(values_equal(BOOL_VAL(true), BOOL_VAL(true)));
    ck_assert(!values_equal(BOOL_VAL(true), BOOL_VAL(false)));
    ck_assert(values_equal(NIL_VAL, NIL_VAL));
    ck_assert(values_equal(EMPTY_VAL, EMPTY_VAL));

    value_t o = OBJ_VAL(copy_string("test_value", 10));
    push(OBJ_VAL(&o));
    ck_assert(values_equal(o, o));
    ck_assert(hash_value(o));
    pop();

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

    free_vm();
}

START_TEST(test_table)
{
    init_vm();

    table_t t;
    init_table_t(&t);


    obj_string_t *key1 = copy_string("test_table1", 11);
    push(OBJ_VAL(key1));
    obj_string_t *key2 = copy_string("test_table2", 11);
    push(OBJ_VAL(key2));
    obj_string_t *key3 = copy_string("test_table3", 11);
    push(OBJ_VAL(key3));

    ck_assert(strncmp(key1->chars, key2->chars, key1->length) != 0);
    ck_assert(strncmp(key1->chars, key3->chars, key1->length) != 0);

    ck_assert(!delete_table_t(&t, key1));
    ck_assert(!delete_table_t(&t, key2));
    ck_assert(!delete_table_t(&t, key3));

    ck_assert(set_table_t(&t, key1, NUMBER_VAL(10)));
    ck_assert(set_table_t(&t, key2, NUMBER_VAL(20)));
    ck_assert(set_table_t(&t, key3, NUMBER_VAL(30)));
    ck_assert(delete_table_t(&t, key3));
    ck_assert(!delete_table_t(&t, key3));
    value_t v;
    ck_assert(!get_table_t(&t, key3, &v));

    table_t tcopy;
    init_table_t(&tcopy);
    add_all_table_t(&t, &tcopy);

    for (int i = 0; i < t.count; i++) {
        value_t from_t;
        value_t from_tcopy;
        ck_assert(get_table_t(&t, key1, &from_t));
        ck_assert(get_table_t(&tcopy, key1, &from_tcopy));
        ck_assert(values_equal(from_t, from_tcopy));
    }

    pop();
    pop();
    pop();

    free_table_t(&t);
    free_table_t(&tcopy);

    free_vm();
}

START_TEST(test_object)
{
    init_vm();

    obj_string_t *str = copy_string("foobar", 6);
    push(OBJ_VAL(str));
    ck_assert(str->length == 6);
    ck_assert_msg(strncmp(str->chars, "foobar", str->length) == 0, "comparing [%s] failed\n", str->chars);

    obj_string_t *p1 = copy_string("foo", 3);
    push(OBJ_VAL(p1));
    obj_string_t *p2 = copy_string("bar", 3);
    push(OBJ_VAL(p2));

    obj_function_t *function = new_obj_function_t();
    push(OBJ_VAL(function));
    obj_closure_t *closure = new_obj_closure_t(function);
    push(OBJ_VAL(closure));

    obj_string_t *cls_name = copy_string("TestObjectTestCase", 18);
    push(OBJ_VAL(cls_name));
    obj_class_t *cls = new_obj_class_t(cls_name);
    push(OBJ_VAL(cls));
    obj_instance_t *instance = new_obj_instance_t(cls);
    push(OBJ_VAL(instance));

    // debugging
    obj_type_t_to_str(((obj_t*)cls_name)->type);
    obj_type_t_to_str(((obj_t*)cls)->type);
    obj_type_t_to_str(((obj_t*)instance)->type);
    obj_type_t_to_str(((obj_t*)function)->type);
    obj_type_t_to_str(((obj_t*)closure)->type);

    // FAM
    // obj_string_t *combined = concatenate_string(p1, p2);
    // ck_assert(combined->length == (p1->length + p2->length));
    // ck_assert(memcmp(combined->chars, "foobar", combined->length) == 0);
    //FREE(obj_string_t, combined); // no free b/c of gc might be running

    // FREE(obj_string_t, str); // no free b/c of gc might be running
    // FREE(obj_string_t, p1); // no free b/c of gc might be running
    // FREE(obj_string_t, p2); // no free b/c of gc might be running

    free_vm();
}

START_TEST(test_debug)
{
    chunk_t chunk;
    init_chunk(&chunk);
    write_chunk(&chunk, OP_CLOSE_UPVALUE, 1);
    disassemble_instruction(&chunk, 0);
    free_chunk(&chunk);

    for (op_code_t op = OP_CONSTANT; op <= OP_RETURN; op++) {
        const char *op_str __unused__ = op_code_t_to_str(op);
    }
    for (token_type_t t = TOKEN_LEFT_PAREN; t <= TOKEN_EOF; t++) {
        const char *op_str __unused__ = token_type_t_to_str(t);
    }
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

    tc = tcase_create("native");
    tcase_add_test(tc, test_native);
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

    tc = tcase_create("debug");
    tcase_add_test(tc, test_debug);
    suite_add_tcase(s, tc);

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
