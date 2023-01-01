#include <unistd.h>
#include <check.h>

#include "common.h"

START_TEST(test_chunk)
{
    vm_t_init(); // now chunk and vm are tied together
    chunk_t chunk;
    chunk_t_init(&chunk);

    chunk_t_write(&chunk, OP_RETURN, 1);

    const int line = 1;
    chunk_t_write(&chunk, OP_CONSTANT, line);
    chunk_t_write(&chunk, (uint8_t)1, line);

    const int index = 656666;
    chunk_t_write(&chunk, (uint8_t)(index & 0xff), line);
    chunk_t_write(&chunk, (uint8_t)((index >> 8) & 0xff), line);
    chunk_t_write(&chunk, (uint8_t)((index >> 16) & 0xff), line);

    chunk_t_add_constant(&chunk, (value_t){.type=VAL_NUMBER, .as={.number=9}});

    int rline = chunk_t_get_line(&chunk, 2);
    ck_assert_int_eq(rline, 1);

    chunk_t_free(&chunk);
    vm_t_free();
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
    scanner_t_init(source);
    for (int idx = 0;results[idx].type != TOKEN_EOF;idx++) {
        token_t t = scanner_t_scan_token();
        const char *token_type_str __unused__ = token_type_t_to_str(t.type);
        ck_assert_msg(t.type == results[idx].type, "Expected %d, got %d\n", results[idx].type, t.type);
        ck_assert(memcmp(t.start, results[idx].start, t.length) == 0);
        ck_assert(t.length == results[idx].length);
        ck_assert(t.line == results[idx].line);
    }


    source = "fun f() { f(\"too\", \"many\"); }";
    scanner_t_init(source);
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
        token_t t = scanner_t_scan_token();
        const char *token_type_str __unused__ = token_type_t_to_str(t.type);

        ck_assert_msg(t.type == results2[idx].type, "Expected %d, got %d\n", results2[idx].type, t.type);
        ck_assert(memcmp(t.start, results2[idx].start, t.length) == 0);
        ck_assert(t.length == results2[idx].length);
        ck_assert(t.line == results2[idx].line);
    }

    source = "fun p() { return 1;}; for(var i = 0; i <= 3; i = i + 1) { !(true and false); false or true; if (i == 2) { print(i); } else {print(p()); continue;}}";
    scanner_t_init(source);
    token_t next = scanner_t_scan_token();
    do {
        const char *token_type_str __unused__ = token_type_t_to_str(next.type);
        next = scanner_t_scan_token();
    } while (next.type != TOKEN_EOF);

    const char *invalid_sources[] = {
        "~",
        NULL,
    };
    for (int i = 0; invalid_sources[i] != NULL; i++) {
        bool found_invalid_token = false;
        scanner_t_init(invalid_sources[i]);
        token_t maybe_next = scanner_t_scan_token();
        do {
            if (maybe_next.type == TOKEN_ERROR) {
                found_invalid_token = true;
            }
            maybe_next = scanner_t_scan_token();
        } while (maybe_next.type != TOKEN_EOF);
        ck_assert_msg(found_invalid_token, "Failed to find invalid token in \"%s\"", invalid_sources[i]);
    }
}

START_TEST(test_compiler)
{
    vm_t_init();
    const char *source1 = "var v = 27; { var v = 1; var y = 2; var z = v + y; }";
    obj_function_t *func1 = compiler_t_compile(source1, false);
    ck_assert(func1->chunk.count == 17);
    ck_assert(func1->chunk.code[0] == OP_CONSTANT);
    ck_assert(func1->chunk.code[2] == OP_DEFINE_GLOBAL);
    ck_assert(func1->chunk.code[4] == OP_CONSTANT);
    ck_assert(func1->chunk.code[6] == OP_CONSTANT);
    ck_assert(func1->chunk.code[8] == OP_GET_LOCAL);
    ck_assert(func1->chunk.code[10] == OP_GET_LOCAL);
    ck_assert(func1->chunk.code[12] == OP_ADD);
    ck_assert(func1->chunk.code[13] == OP_POPN);
    ck_assert(func1->chunk.code[15] == OP_NIL);
    ck_assert(func1->chunk.code[16] == OP_RETURN);
    ck_assert(func1->chunk.constants.count == 4); // v, 27, 1, 2
    ck_assert(memcmp(AS_CSTRING(func1->chunk.constants.values[0]), "v", 1) == 0);
    ck_assert(func1->chunk.constants.values[1].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[1]) == 27);
    ck_assert(func1->chunk.constants.values[2].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[2]) == 1);
    ck_assert(func1->chunk.constants.values[3].type == VAL_NUMBER);
    ck_assert(AS_NUMBER(func1->chunk.constants.values[3]) == 2);
    vm_t_free();

    vm_t_init();
    const char *func_source = "fun a(x,y) { var sum = x + y; print(sum);}";
    obj_function_t *func2 = compiler_t_compile(func_source, false);
    ck_assert(func2->chunk.count == 6);
    ck_assert(func2->chunk.code[0] == OP_CLOSURE);
    ck_assert(func2->chunk.code[2] == OP_DEFINE_GLOBAL);
    ck_assert(func2->chunk.code[4] == OP_NIL);
    ck_assert(func2->chunk.code[5] == OP_RETURN);

    ck_assert(memcmp(AS_CSTRING(func2->chunk.constants.values[0]), "a", 1) == 0);
    obj_function_t *inner = AS_FUNCTION(func2->chunk.constants.values[1]);
    ck_assert(inner->chunk.count == 10);
    ck_assert(inner->chunk.code[0] == OP_GET_LOCAL);
    ck_assert(inner->chunk.code[2] == OP_GET_LOCAL);
    ck_assert(inner->chunk.code[4] == OP_ADD);
    ck_assert(inner->chunk.code[5] == OP_GET_LOCAL);
    ck_assert(inner->chunk.code[7] == OP_PRINT);
    ck_assert(inner->chunk.code[8] == OP_NIL);
    ck_assert(inner->chunk.code[9] == OP_RETURN);
    vm_t_free();

    const char *programs[] = {
        "for(var i = 0; i < 5; i = i + 1) { print i; var v = 1; v = v + 2; v = v / 3; v = v * 4;}",
        "var counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "var foo = 10; var result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
        NULL,
    };
    for (int p = 0; programs[p] != NULL; p++) {
        vm_t_init();
        obj_function_t *program_func __unused__ = compiler_t_compile(programs[p], false);
        vm_t_free();
    }
}

START_TEST(test_vm)
{
    const char *test_cases[] = {
        "switch(3) { case 0: print(0); case 1: print(1); case 2: print(2); default: true; }",
        "switch(3) { default: print(0); }",
        "switch(3) { case 3: print(3); }",
        "switch(3) { }",
        "var counter = 0; while (counter < 10) { break; print counter; counter = counter + 1;} assert(counter == 0);",
        "var counter = 0; for(var i = 0; i < 5; i++) { break; counter++;} assert(counter == 0);",
        "var counter = 0; for(var i = 0; i < 5; i++) { counter++; for(var y = 0; y < 3; y++) { break; } } assert(counter == 5);",
        "var counter = 0; var extra = 0; while (counter < 10) { counter = counter + 1; continue; extra++; print \"never reached\";} assert(extra == 0);",
        "var extra = 0; for(var i =0; i < 5; i++) { continue; extra++; print \"never reached\";} assert(extra == 0);",
        "class Foo {} class Bar {} var f = Foo(); print(is(f, Foo)); print(is(f, Bar)); print(has_field(f, \"nosuch\")); f.name = \"foo\"; print(has_field(f, \"name\"));",

        "print(sys_version());",

        "fun t1() { var i = 2; fun inner() { return i;} return inner;}"
        "for (var i = 0; i < 10;i++) { var f = t1(); var f2 = t1(); var f3 = t1(); continue;}" // popn
        "for (var i = 0; i < 10;i++) { var f = t1(); var f2 = t1(); var f3 = t1(); break;}", // popn

        "is(2, \"not a type\");",
        "assert(is(\"foo\", str));",
        "assert(!is(2, str));",
        "assert(is(list(), list));",
        "assert(!is(nil, list));",
        "assert(is(nil, nil));",
        "assert(is(2, number));",
        "assert(is(2.0, number));",
        "assert(!is(nil, number));",
        "assert(!is(\"foo\", number));",
        "assert(!is(\"foo\", number));",
        "assert(number(true) == 1);",
        "assert(number(false) == 0);",
        "assert(number(nil) == 0);",
        "assert(number(9) == 9);",
        "assert(number(\"1.1\") == 1.1);",
        "assert(number(\"-5\") == -5);",

        "class Foo {} var f = Foo();"
        "assert(!get_field(f, \"name\"));"
        "assert(set_field(f, \"name\", \"foo\"));"
        "assert(get_field(f, \"name\"));",

        "assert(\"foo\".len() == 3);",
        "var s = \"foo\"; var f = s.len; assert(f() == 3);",
        "var a = str() + str() + str();"
        "assert(a.len() == 0);",
        "assert(str(1) == \"1\");",
        "assert(str(true) == \"true\");",
        "assert(str(nil) == \"nil\");",
        "assert(\"foo\".substr(0,2) == \"fo\");",
        "assert(\"foo\".substr(-2,2) == \"oo\");",
        "var a = \"foobar\"; assert(a[0] == \"f\"); assert(a[-1] == \"r\");",

        "var a = list(1,2,3); assert(a.len() == 3); a.clear(); assert(a.len() == 0); a.append(45); assert(a.len() == 1);",
        "var a = list(1,2,3,4,5); while (a.len() !=0){ a.remove(-1);} assert(a.len() == 0);",
        "var a = list(); a.remove(0); assert(a.len() == 0);",
        "var a = list(1,2,3,4,5); a.remove(2); assert(a.len() == 4); assert(a.get(2) == 4); a.remove(-1); assert(a.get(-1) == 4);",
        "var a = list(1,2,3); assert(a[0] == 1); assert(a[2] == 3); assert(a[-1] == 3);",
        "var a = [1, \"two\", 3, \"four\"]; assert(a[0] == 1); assert(a[-1] == \"four\");",

        "var a = [{\"name\": \"foo\", \"counter\": 11}, {\"name\": \"bar\", \"counter\": 22}];"
        "assert(a[0][\"counter\"] == 11); assert(a[1][\"counter\"] == 22); assert(a[0][\"name\"] + a[1][\"name\"] == \"foobar\");",

        "var m = map(\"one\", 1, \"two\", 2); assert(m.len() == 2); assert(m.keys().len() == 2); assert(m.values().len() == 2);"
        "assert(m.get(\"one\") == 1); assert(m.get(\"two\") == 2); assert(m[\"two\"] == 2); assert(m.get(\"nosuch\") == nil); assert(m[\"nosuch\"] == nil);",
        "map(1, \"one\").len();",
        "var a = map({1:2, \"two\": \"two\"}); assert(a[1] == 2); assert(a[\"two\"] == \"two\");",

        "var a = map(); for (var i = 0; i < 255; i++) { a.set(\"testcase\" + str(i), str(i * 255)); }"
        "for (var i = 0; i < 255; i++) { assert(a[\"testcase\" + str(i)] == str(i * 255)); }",

        "class Animal {} class Dog < Animal {} class Cat < Animal {}"
        "var m = {Cat:[], Dog:[]}; m[Cat].append(Cat()); assert(m[Cat].len() == 1); m[Animal] = [Dog(), Cat()]; assert(m[Animal].len() == 2);",

        "var a = map(\"one\", 1, \"two\", 2); a.set(\"three\", 3); print(a); assert(is(a, map));"
        "var counter = 0; while (true) {"
            "var key = a.keys().get(counter);"
            "print(\"key is \" + key + \", value is \" + str(a.get(key)));"
            "counter++;"
            "if (counter >= a.keys().len()) {"
                "break;"
            "}"
        "}",

        "var counter = 1; while (counter < 10) { counter = counter + 1;} assert(counter == 10);",

        "var s1 = 0; var s2 = 0; var s3 = 0;"
        "fun outer(){"
            "var x = 100; "
            "fun middle() { "
                "fun inner() {"
                    "s3 = 3;"
                    "return x + 3;"
                "}"
                "s2 = 2;"
                "x = x + 2;"
                "return inner;"
            "}"
            "s1 = 1;"
            "x = x + 1;"
            "return middle;"
        "} var mid = outer(); var in = mid(); assert(in() == 106); assert(s1 == 1); assert(s2 == 2); assert(s3 == 3);",
        "for(var i = 0; i < 5; i++) { print i;}",
        "var a = 1; a++; assert(a == 2);"
        "a += 10; assert(a == 12);"
        "a /= 6; assert(a == 2);"
        "a *= 6; assert(a == 12);"
        "a -= 0; assert(a == 12);"
        "a += 0; assert(a == 12);"
        "a *= 0; assert(a == 0);",
        "var foo = \"one\"; foo += \" bar\";",

        "var p = list(1, 2, 3); assert(p.len() == 3);",
        "class Foo {init(name, list) { this.name = name; this.list = list;} len() { return this.list.len();}}"
        "var f = Foo(\"jason\", list(1,2,3));"
        "assert(f.len() == 3);"
        "f.list.append(\"one\");"
        "assert(f.len() == 4);"
        "var call = f.list.append; call(200);"
        "assert(f.len() == 5);",

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

        // https://github.com/munificent/craftingvm_t_interpreters/issues/888
        "fun returnArg(arg){ return arg;}"
        "fun returnFunCallWithArg(func, arg){return returnArg(func)(arg);}"
        "fun printArg(arg){print arg;}"
        "returnFunCallWithArg(printArg, \"hello world\");",

        // OP_CLOSE_UPVALUE https://github.com/munificent/craftingvm_t_interpreters/issues/746
        "var f1; var f2; { var i = 1; fun f() { print i; } f1 = f; } { var j = 2; fun f() { print j; } f2 = f; } f1(); f2();",

        // trigger gc
        "var x = 1; var y = 2; var z = x + y;"
        "fun lots_of_stuff() { var a = \"asdfasdfasdfasdfasdafsdfasdfasdfafs\"; var b = \"asdfsdfasdfasfdafsdfas\"; return a+b;}"
        "for (var i = 0; i < 1000; i = i + 1) { var r = lots_of_stuff(); print(r); print(x+y+z);}",

        "class Brioche {} print Brioche; print Brioche();", // chapter 27
        "class Pair {} var pair = Pair(); pair.first = 1; pair.second = 2; print pair.first + pair.second;", // chapter 27

        "class Brunch { bacon() {} eggs() {} } var brunch = Brunch(); var eggs = brunch.eggs; eggs();", // chapter 28

        "class Scone { topping(first, second) { print \"scone with \" + first + \" and \" + second; }}"
        "var scone = Scone(); scone.topping(\"berries\", \"cream\");", // chapter 28

        "class Person { say_name() {print this.name;} }"
        "var me = Person(); me.name = \"test\"; var method = me.say_name; method();", // chapter 28

        "class Nested { method() { fun function() { print this; } function(); } } Nested().method();", // chapter 28

        "class Brunch { init(food, drink) {} } Brunch(\"eggs\", \"coffee\");", // chapter 28

        "class CoffeeMaker { "
            "init(coffee) { this.coffee = coffee; }"
            "brew() { print \"enjoy \" + this.coffee; this.coffee = nil; }"
        "}"
        "var maker = CoffeeMaker(\"coffee and chicory\");"
        "maker.brew();", // chapter 28

        "class Oops { init() { fun f() { print \"not a method\"; } this.field = f; } } var oops = Oops(); oops.field();", // chapter 28

        "class Doughnut { cook() { print(\"Dunk in the fryer.\"); } }"
        "class Cruller < Doughnut { finish() { print(\"Glaze with icing.\"); } }"
        "var c = Cruller(); c.cook(); c.finish();", // chapter 29

        "class A { method() { print(\"A method\");}}"
        "class B < A { method() { print(\"B method\");} test() { super.method(); }}"
        "class C < B {}"
        "C().test();", // chapter 29

        "class A { method() { print \"A\"; } }"
        "class B < A { method() { var closure = super.method; closure(); } }" // prints "A"
        "B().method();", // chapter 29

        "class Doughnut {"
            "cook() { print(\"Dunk in the fryer.\"); this.finish(\"sprinkles\"); }"
            "finish(ingredient) { print(\"Finish with \" + ingredient); }"
        "}"
        "class Cruller < Doughnut {"
            "finish(ingredient) {"
                // no sprinkles, always icing
                "super.finish(\"icing\");"
            "}"
        "}", // chapter 29

        NULL,
    };
    for (int i = 0; test_cases[i] != NULL; i++) {
        vm_t_init();
        ck_assert_msg(vm_t_interpret(test_cases[i]) == INTERPRET_OK, "test case failed for \"%s\"\n", test_cases[i]);
        vm_t_free();
    }


    const char *exit_ok_tests[] = {
        "exit;",
        "exit(0);",
        "fun finish_and_quit() { print(\"working\"); exit(0); } finish_and_quit();",
        NULL,
    };
    for (int i = 0; exit_ok_tests[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(exit_ok_tests[i]);
        ck_assert_msg(rv == INTERPRET_EXIT_OK, "test case failed for \"%s\"\n", exit_ok_tests[i]);
        vm_t_free();
    }

    const char *exit_tests[] = {
        "exit(1);",
        "exit(-1);",
        "assert(1 == 2);",
        "fun finish_and_fail() { print(\"working\"); exit(-1); } finish_and_fail();",
        NULL,
    };
    for (int i = 0; exit_tests[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(exit_tests[i]);
        ck_assert_msg(rv == INTERPRET_EXIT, "test case failed for \"%s\"\n", exit_tests[i]);
        vm_t_free();
    }

    const char *compilation_fail_cases[] = {
        "continue;",

        "var;",
        "var foo = 1",
        "{var foo = foo;}",
        // "}{",
        "if true ){}",
        " 1 = 3;",
        "{ var a = 1; var a = 2;}",
        "print this;", // chapter 28
        "fun not_a_method() { print this;}", // chapter 28
        "class CannotReturnFromInitializer { init() { return 1; } } CannotReturnFromInitializer(); ", // chapter 28
        "class Foo < Foo {}", // chapter 29
        "class NoSuperClass { method() { super.method();}}", // chapter 29
        "fun NotClass() { super.NotClass(); }", // chapter 29
        "switch(3) { var statement_not_allowed_here = true; case 0: print(0); case 1: print(1); case 2: print(2); default: true; }",
        "switch(3) { default: true; case 3: print(\"cannot have case after default\"); }",
        "{ break;}",
        NULL,
    };
    for (int i = 0; compilation_fail_cases[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(compilation_fail_cases[i]);
        ck_assert_msg(rv == INTERPRET_COMPILE_ERROR, "Unexpected success for \"%s\"\n", compilation_fail_cases[i]);
        vm_t_free();
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
        "class OnlyOneArgInit { init(one) {} } var i = OnlyOneArgInit(1, 2);", // chapter 28
        "class NoArgInit {} var i = NoArgInit(1, 2);", // chapter 28
        "var NotClass = \"so not a class\"; class OhNo < NotClass {}", // chapter 29
        "var a = 1; a = a / 0;", // divbyzero
        "var f = 1; f.foo = 1;", // only instances have property
        "var f = 1; f.foo(1);", // only instances have methods
        "class Foo {} var f = Foo(); f.nosuchproperty();",
        "class Foo {} var f = Foo(); var invalid = f.nosuchproperty;",
        "is();",
        "has_field();",
        "has_field(2, \"not a type\");",
        "set_field();",
        "set_field(true);",
        "set_field(true, true);",
        "set_field(true, true, true);",
        "class Foo {} var f = Foo(); set_field();",
        "class Foo {} var f = Foo(); set_field(f);",
        "class Foo {} var f = Foo(); set_field(f, \"fieldnoval\");",
        "class Foo {} var f = Foo(); get_field(f);",
        "class Foo {} var f = Foo(); get_field();",
        "var a = list(\"one\", 2, \"three\"); a.get(3);",
        "var a = \"foo\"; var f = a.nosuchmethod; f();",
        "number(list());",
        "number();",
        "\"foo\".substr();",
        "\"foo\".substr(-10,1);",
        "\"foo\".substr(0);",
        "\"foo\".substr(0,10);",
        "\"foo\".len(1);",
        "list().len(1);",
        "list().get();",
        "list().get(true);",
        "list().clear(true);",
        "list().append();",
        "list(1,2,3).remove(true);",
        "list(1,2,3).remove();",
        "list(1).remove(2);",
        "true.nosuchpropertyonanoninstance;",
        "map(1);",
        "map(\"one\", 1).len(1);",
        NULL,
    };
    for (int i = 0; runtime_fail_cases[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(runtime_fail_cases[i]);
        // in case we hose up a bad compile
        ck_assert(rv != INTERPRET_COMPILE_ERROR);
        ck_assert_msg(rv == INTERPRET_RUNTIME_ERROR, "Unexpected success for \"%s\"\n", runtime_fail_cases[i]);
        vm_t_free();
    }
}

static bool native_getpid(const int, const value_t*)
{
    vm_push(NUMBER_VAL((double)getpid()));
    return true;
}

START_TEST(test_native)
{
    vm_t_init();

    vm_define_native("getpid", native_getpid, 0);
    ck_assert(native_getpid(0, NULL));
    value_t v = vm_pop();
    ck_assert_int_gt(AS_NUMBER(v), 0);
    ck_assert(vm_t_interpret("print(getpid());") == INTERPRET_OK);

    const obj_string_t *name = obj_string_t_copy_from("getpid", 6);
    vm_push(OBJ_VAL(name));
    obj_native_t *native_fn = obj_native_t_allocate(native_getpid, name, 0);
    vm_push(OBJ_VAL(native_fn));
    obj_type_t_to_str(((obj_t*)native_fn)->type); // debugging

    vm_t_free();
}

START_TEST(test_value)
{
    vm_t_init();

    ck_assert(value_t_hash(BOOL_VAL(true)) == 3);
    ck_assert(value_t_hash(BOOL_VAL(false)) == 5);
    ck_assert(value_t_hash(NIL_VAL) == 7);
    ck_assert(value_t_hash(EMPTY_VAL) == 0);
    ck_assert(value_t_hash(NUMBER_VAL(9)) == 1076101120);

    value_type_t_to_str(VAL_BOOL);
    value_type_t_to_str(VAL_NIL);
    value_type_t_to_str(VAL_NUMBER);
    value_type_t_to_str(VAL_OBJ);
    value_type_t_to_str(VAL_EMPTY);

    ck_assert(value_t_equal(NUMBER_VAL(100), NUMBER_VAL(100)));
    ck_assert(!value_t_equal(NUMBER_VAL(100), NUMBER_VAL(200)));
    ck_assert(value_t_equal(BOOL_VAL(true), BOOL_VAL(true)));
    ck_assert(!value_t_equal(BOOL_VAL(true), BOOL_VAL(false)));
    ck_assert(value_t_equal(NIL_VAL, NIL_VAL));
    ck_assert(value_t_equal(EMPTY_VAL, EMPTY_VAL));

    value_t o = OBJ_VAL(obj_string_t_copy_from("test_value", 10));
    vm_push(OBJ_VAL(&o));
    ck_assert(value_t_equal(o, o));
    ck_assert(value_t_hash(o));
    vm_pop();

    value_array_t a;
    value_array_t_init(&a);
    value_array_t_add(&a, NUMBER_VAL(9));
    value_array_t_add(&a, BOOL_VAL(false));
    value_array_t_add(&a, NIL_VAL);
    value_array_t_add(&a, EMPTY_VAL);
    for (int i = 0; i < a.count; i++) {
        value_t_print(a.values[i]);
    }
    value_array_t_free(&a);

    vm_t_free();
}

START_TEST(test_table)
{
    vm_t_init();

    table_t t;
    table_t_init(&t);


    obj_string_t *key1 = obj_string_t_copy_from("test_table1", 11);
    vm_push(OBJ_VAL(key1));
    obj_string_t *key2 = obj_string_t_copy_from("test_table2", 11);
    vm_push(OBJ_VAL(key2));
    obj_string_t *key3 = obj_string_t_copy_from("test_table3", 11);
    vm_push(OBJ_VAL(key3));

    ck_assert(strncmp(key1->chars, key2->chars, key1->length) != 0);
    ck_assert(strncmp(key1->chars, key3->chars, key1->length) != 0);

    ck_assert(!table_t_delete(&t, OBJ_VAL(key1)));
    ck_assert(!table_t_delete(&t, OBJ_VAL(key2)));
    ck_assert(!table_t_delete(&t, OBJ_VAL(key3)));

    ck_assert(table_t_set(&t, OBJ_VAL(key1), NUMBER_VAL(10)));
    ck_assert(table_t_set(&t, OBJ_VAL(key2), NUMBER_VAL(20)));
    ck_assert(table_t_set(&t, OBJ_VAL(key3), NUMBER_VAL(30)));
    ck_assert(table_t_delete(&t, OBJ_VAL(key3)));
    ck_assert(!table_t_delete(&t, OBJ_VAL(key3)));
    value_t v;
    ck_assert(table_t_get(&t, OBJ_VAL(key1), &v));
    ck_assert(table_t_get(&t, OBJ_VAL(key2), &v));
    ck_assert(!table_t_get(&t, OBJ_VAL(key3), &v));

    table_t tcopy;
    table_t_init(&tcopy);
    table_t_copy_to(&t, &tcopy);

    for (int i = 0; i < t.capacity; i++) {
        value_t from_t;
        value_t from_tcopy;
        ck_assert(table_t_get(&t, OBJ_VAL(key1), &from_t));
        ck_assert(table_t_get(&tcopy, OBJ_VAL(key1), &from_tcopy));
        ck_assert(value_t_equal(from_t, from_tcopy));
    }

    vm_pop();
    vm_pop();
    vm_pop();

    table_t_free(&t);
    table_t_free(&tcopy);


    table_t big;
    table_t_init(&big);
    for (int i = 0; i < 8192; i++) {
        char buffer[255];
        int wrote = snprintf(buffer, 255, "item%dforhash", i);
        value_t key = OBJ_VAL(obj_string_t_copy_from(buffer, wrote));
        ck_assert(table_t_set(&big, key, NUMBER_VAL(i)));
    }
    for (int i = 0; i < 8192; i++) {
        char buffer[255];
        int wrote = snprintf(buffer, 255, "item%dforhash", i);
        value_t key = OBJ_VAL(obj_string_t_copy_from(buffer, wrote));
        value_t rv;
        ck_assert(table_t_get(&big, key, &rv));
        ck_assert(AS_NUMBER(rv) == i);
    }
    table_t bigcopy;
    table_t_init(&bigcopy);
    table_t_copy_to(&big, &bigcopy);
    for (int i = 0; i < 8192; i++) {
        char buffer[255];
        int wrote = snprintf(buffer, 255, "item%dforhash", i);
        value_t key = OBJ_VAL(obj_string_t_copy_from(buffer, wrote));
        value_t from_big;
        value_t from_bigcopy;
        ck_assert(table_t_get(&big, key, &from_big));
        ck_assert(table_t_get(&bigcopy, key, &from_bigcopy));
        ck_assert(value_t_equal(from_big, from_bigcopy));
    }
    table_t_free(&big);
    table_t_free(&bigcopy);

    vm_t_free();
}

START_TEST(test_object)
{
    vm_t_init();

    obj_string_t *str = obj_string_t_copy_from("foobar", 6);
    vm_push(OBJ_VAL(str));
    ck_assert(str->length == 6);
    ck_assert_msg(strncmp(str->chars, "foobar", str->length) == 0, "comparing [%s] failed\n", str->chars);

    obj_string_t *p1 = obj_string_t_copy_from("foo", 3);
    vm_push(OBJ_VAL(p1));
    obj_string_t *p2 = obj_string_t_copy_from("bar", 3);
    vm_push(OBJ_VAL(p2));

    obj_function_t *function = obj_function_t_allocate();
    vm_push(OBJ_VAL(function));
    obj_closure_t *closure = obj_closure_t_allocate(function);
    vm_push(OBJ_VAL(closure));

    obj_string_t *cls_name = obj_string_t_copy_from("TestObjectTestCase", 18);
    vm_push(OBJ_VAL(cls_name));
    obj_class_t *cls = obj_class_t_allocate(cls_name);
    vm_push(OBJ_VAL(cls));
    obj_instance_t *instance = obj_instance_t_allocate(cls);
    vm_push(OBJ_VAL(instance));


    obj_list_t *list = obj_list_t_allocate();
    vm_push(OBJ_VAL(list));

    // debugging
    obj_type_t_to_str(((obj_t*)cls_name)->type);
    obj_type_t_to_str(((obj_t*)cls)->type);
    obj_type_t_to_str(((obj_t*)instance)->type);
    obj_type_t_to_str(((obj_t*)function)->type);
    obj_type_t_to_str(((obj_t*)closure)->type);

    // FREE(obj_string_t, str); // no free b/c of gc might be running
    // FREE(obj_string_t, p1); // no free b/c of gc might be running
    // FREE(obj_string_t, p2); // no free b/c of gc might be running

    vm_t_free();
}

START_TEST(test_debug)
{
    vm_t_init();
    vm_toggle_gc_stress();

    chunk_t chunk;
    chunk_t_init(&chunk);

    chunk_t_write(&chunk, OP_CLOSE_UPVALUE, 1);
    chunk_t_disassemble_instruction(&chunk, 0); // simple, +1

    chunk_t_write(&chunk, OP_SET_UPVALUE, 1); //byte +2
    chunk_t_write(&chunk, 1, 1);
    chunk_t_disassemble_instruction(&chunk, 1);

    int i = chunk_t_add_constant(&chunk, NUMBER_VAL(0));
    chunk_t_write(&chunk, OP_GET_SUPER, 1); // const +2
    chunk_t_write(&chunk, i, 1);
    chunk_t_disassemble_instruction(&chunk, 3);

    chunk_t_free(&chunk);

    for (op_code_t op = OP_CONSTANT; op < INVALID_OPCODE; op++) {
        const char *op_str __unused__ = op_code_t_to_str(op);
    }
    for (token_type_t t = TOKEN_LEFT_PAREN; t <= TOKEN_EOF; t++) {
        const char *op_str __unused__ = token_type_t_to_str(t);
    }

    // trigger gc
    const char *program =
        "var x = 1; var y = 2; var z = x + y; var z2 = z - 1; var t = true; var f = false; var invalid = !true;"
        "var alist = list(\"one\", \"two\", 3); assert(alist.len() == 3); assert(str(alist.get(-1)) == \"3\"); str(alist);"
        "class Point { init(x,y) { this.x = x; this.y = y;} dostuff() { z = z + x + y; }}"
        "class SubPoint < Point { init(x,y) { super.init(x/2,y*10); }} var sp= SubPoint(10, 20); assert(sp.x == 5);"
        "var f1; { var i = 100; fun inner() { print i;} f1 = inner;}"
        "var f2 = Point(-900, -900).dostuff;"
        "var alistlen = alist.len;"
        "fun lots_of_stuff() { "
            "var a = \"asdfasdfasdfasdfasdafsdfasdfasdfafs\";"
            "var b = \"asdfsdfasdfasfdafsdfas\";"
            "{print(a+b);}"
            "{ assert(alistlen() == 3);}"
            "var p = Point(100, 200);"
            "{str(p); str(100); str(alistlen); str(true); str(nil); str(Point); var c = Point(1,2).dostuff; str(c);}"
            "{ p.dostuff(); print(p.x + p.y); p.x < p.y; p.x > p.y; print(a + b); f1(); f2();}"
            "return p;"
        "}"
        "{str(lots_of_stuff);}"
        "for (var i = 0; i < 1000; i = i + 1) {"
            "var r = lots_of_stuff();"
            "print(r);"
        "}";
    ck_assert(vm_t_interpret(program) == INTERPRET_OK);

    vm_toggle_gc_trace();
    vm_toggle_stack_trace();
    const char *programs_with_tracing[] = {
        // https://github.com/munificent/craftingvm_t_interpreters/issues/888
        "fun returnArg(arg){ return arg;}"
        "fun returnFunCallWithArg(func, arg){return returnArg(func)(arg);}"
        "fun printArg(arg){print arg;}"
        "returnFunCallWithArg(printArg, \"hello world\");",

        // OP_CLOSE_UPVALUE https://github.com/munificent/craftingvm_t_interpreters/issues/746
        "var f1; var f2; { var i = 1; fun f() { print i; } f1 = f; } { var j = 2; fun f() { print j; } f2 = f; } f1(); f2();",

        "class A { method() { print \"A\"; } }"
        "class B < A { method() { var closure = super.method; closure(); } }" // prints "A"
        "B().method();", // chapter 29

        "class Doughnut {"
            "cook() { print(\"Dunk in the fryer.\"); this.finish(\"sprinkles\"); }"
            "finish(ingredient) { print(\"Finish with \" + ingredient); }"
        "}"
        "class Cruller < Doughnut {"
            "finish(ingredient) {"
                // no sprinkles, always icing
                "super.finish(\"icing\");"
            "}"
        "}", // chapter 29
        NULL,
    };
    for (int p = 0; programs_with_tracing[p] != NULL; p++) {
        ck_assert(vm_t_interpret(programs_with_tracing[p]) == INTERPRET_OK);
    }
    vm_t_free();
}

START_TEST(test_env) {
    vm_t_init();
    vm_toggle_gc_stress();
    vm_toggle_gc_trace();
    vm_toggle_stack_trace();
    vm_inherit_env();

    const char *args[] = {
        "testprog",
        "--foo",
        "--bar",
    };
    vm_set_argc_argv(3, args);

    const char *program = ""
    "assert(argc == 3);"
    "assert(argv.len() == 3);"
    "assert(argv.get(0) == \"testprog\");"
    "assert(argv.get(1) == \"--foo\");"
    "assert(argv.get(2) == \"--bar\");"
    "assert(env.get(\"HOME\") != nil);"
    "assert(env.get(\"NOSUCHENVVARIABLETESTING\") == nil);"
    "";
    ck_assert_msg(vm_t_interpret(program) == INTERPRET_OK, "Failed to interpret: %s", program);
    vm_t_free();
}

int main(const int argc, const char *argv[])
{
    const char *suite_name = "clox";

    int number_failed;
    Suite *s = suite_create(suite_name);
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

    tc = tcase_create("env");
    tcase_add_test(tc, test_env);
    suite_add_tcase(s, tc);

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (suite_tcase(s, argv[i])) {
                srunner_run(sr, suite_name, argv[i], CK_VERBOSE);
            } else {
                fprintf(stderr, "No such test case %s, ignoring.\n", argv[i]);
            }
        }
    } else {
        srunner_run_all(sr, CK_VERBOSE);
    }
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
