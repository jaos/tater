/*
 * Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <check.h>

#include "common.h"

START_TEST(test_chunk)
{
    vm_t_init();
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
    const char *source = "let foo = \"foo\"; { let bar = \"bar\"; let foobar = foo + bar; print foobar;}";
    const token_t results[] = {
        {.type=TOKEN_LET, .start="let", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="foo", .length=3, .line=1},
        {.type=TOKEN_EQUAL, .start="=", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"foo\"", .length=5, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_LEFT_BRACE, .start="{", .length=1, .line=1},
        {.type=TOKEN_LET, .start="let", .length=3, .line=1},
        {.type=TOKEN_IDENTIFIER, .start="bar", .length=3, .line=1},
        {.type=TOKEN_EQUAL, .start="=", .length=1, .line=1},
        {.type=TOKEN_STRING, .start="\"bar\"", .length=5, .line=1},
        {.type=TOKEN_SEMICOLON, .start=";", .length=1, .line=1},
        {.type=TOKEN_LET, .start="let", .length=3, .line=1},
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


    source = "fn f() { f(\"too\", \"many\"); }";
    scanner_t_init(source);
    const token_t results2[] = {
        {.type=TOKEN_FN, .start="fn", .length=2, .line=1},
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

    source = "fn p() { return 1;}; for(let i = 0; i <= 3; i = i + 1) { !(true and false); false or true; if (i == 2) { print(i); } else {print(p()); continue;}}";
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
    const char *source1 = "let v = 27; { let v = 1; let y = 2; let z = v + y; }";
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
    const char *func_source = "fn a(x,y) { let sum = x + y; print(sum);}";
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
        "for(let i = 0; i < 5; i = i + 1) { print i; let v = 1; v = v + 2; v = v / 3; v = v * 4;}",
        "let counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "let foo = 10; let result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
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
        "let counter = 0; while (counter < 10) { break; print counter; counter = counter + 1;} assert(counter == 0);",
        "let counter = 0; for(let i = 0; i < 5; i++) { break; counter++;} assert(counter == 0);",
        "let counter = 0; for(let i = 0; i < 5; i++) { counter++; for(let y = 0; y < 3; y++) { break; } } assert(counter == 5);",
        "let counter = 0; let extra = 0; while (counter < 10) { counter = counter + 1; continue; extra++; print \"never reached\";} assert(extra == 0);",
        "let extra = 0; for(let i =0; i < 5; i++) { continue; extra++; print \"never reached\";} assert(extra == 0);",
        "type Foo {} type Bar {} let f = Foo(); print(is(f, Foo)); print(is(f, Bar)); print(has_field(f, \"nosuch\")); f.name = \"foo\"; print(has_field(f, \"name\"));",

        "print(sys_version());",

        "fn t1() { let i = 2; fn inner() { return i;} return inner;}"
        "for (let i = 0; i < 10;i++) { let f = t1(); let f2 = t1(); let f3 = t1(); continue;}" // popn
        "for (let i = 0; i < 10;i++) { let f = t1(); let f2 = t1(); let f3 = t1(); break;}", // popn

        "type Foo {"
            "let counter1 = 0;"
            "let counter2 = 0;"
            "fn doit1() {"
                "self.counter1 += 1;"
                "print(self.counter1);"
            "}"
            "fn doit2() {"
                "self.counter2++;"
                "print(self.counter2);"
            "}"
            "fn doit() {"
                "self.doit1(); self.doit2();"
            "}"
        "}"
        "type Bar(Foo) {};"
        "let f = Bar(); f.doit(); f.doit(); f.doit(); f.doit(); assert(f.counter1 == 4); assert(f.counter2 == 4);",

        "type Animals { let Cat = \"cat\"; let Dog = \"dog\"; let Bird = \"bird\";}"
        "assert(Animals.Cat == \"cat\");",

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

        "type Foo {}; type Bar(Foo) {}; type Baz(Bar) {};"
        "let i1 = Baz(); let i2 = Bar(); let i3 = Foo();"
        "assert(is(i1, Foo) == true); assert(is(i1, Bar) == true); assert(is(i1, Baz) == true);"
        "assert(is(i2, Foo) == true); assert(is(i2, Bar) == true); assert(is(i2, Baz) == false);"
        "assert(is(i3, Foo) == true); assert(is(i3, Bar) == false); assert(is(i3, Baz) == false);",

        "type Foo {} let f = Foo();"
        "assert(!get_field(f, \"name\"));"
        "assert(set_field(f, \"name\", \"foo\"));"
        "assert(get_field(f, \"name\"));",

        "assert(\"foo\".len() == 3);",
        "let s = \"foo\"; let f = s.len; assert(f() == 3);",
        "let a = str() + str() + str();"
        "assert(a.len() == 0);",
        "assert(str(1) == \"1\");",
        "assert(str(true) == \"true\");",
        "assert(str(nil) == \"nil\");",
        "assert(\"foo\".substr(0,2) == \"fo\");",
        "assert(\"foo\".substr(-2,2) == \"oo\");",
        "let a = \"foobar\"; assert(a[0] == \"f\"); assert(a[-1] == \"r\");",

        "let a = list(1,2,3); assert(a.len() == 3); a.clear(); assert(a.len() == 0); a.append(45); assert(a.len() == 1);",
        "let a = list(1,2,3,4,5); while (a.len() !=0){ a.remove(-1);} assert(a.len() == 0);",
        "let a = list(); a.remove(0); assert(a.len() == 0);",
        "let a = list(1,2,3,4,5); a.remove(2); assert(a.len() == 4); assert(a.get(2) == 4); a.remove(-1); assert(a.get(-1) == 4);",
        "let a = list(1,2,3); assert(a[0] == 1); assert(a[2] == 3); assert(a[-1] == 3);",
        "let a = [1, \"two\", 3, \"four\"]; assert(a[0] == 1); assert(a[-1] == \"four\");",

        "let a = [{\"name\": \"foo\", \"counter\": 11}, {\"name\": \"bar\", \"counter\": 22}];"
        "assert(a[0][\"counter\"] == 11); assert(a[1][\"counter\"] == 22); assert(a[0][\"name\"] + a[1][\"name\"] == \"foobar\");",

        "let m = map(\"one\", 1, \"two\", 2); assert(m.len() == 2); assert(m.keys().len() == 2); assert(m.values().len() == 2);"
        "assert(m.get(\"one\") == 1); assert(m.get(\"two\") == 2); assert(m[\"two\"] == 2); assert(m.get(\"nosuch\") == nil); assert(m[\"nosuch\"] == nil);",
        "map(1, \"one\").len();",
        "let a = map({1:2, \"two\": \"two\"}); assert(a[1] == 2); assert(a[\"two\"] == \"two\");",

        "let a = map(); for (let i = 0; i < 255; i++) { a.set(\"testcase\" + str(i), str(i * 255)); }"
        "for (let i = 0; i < 255; i++) { assert(a[\"testcase\" + str(i)] == str(i * 255)); }",

        "type Animal {} type Dog (Animal) {} type Cat (Animal) {}"
        "let m = {Cat:[], Dog:[]}; m[Cat].append(Cat()); assert(m[Cat].len() == 1); m[Animal] = [Dog(), Cat()]; assert(m[Animal].len() == 2);",

        "let a = map(\"one\", 1, \"two\", 2); a.set(\"three\", 3); print(a); assert(is(a, map));"
        "let counter = 0; while (true) {"
            "let key = a.keys().get(counter);"
            "print(\"key is \" + key + \", value is \" + str(a.get(key)));"
            "counter++;"
            "if (counter >= a.keys().len()) {"
                "break;"
            "}"
        "}",

        "let counter = 1; while (counter < 10) { counter = counter + 1;} assert(counter == 10);",

        "let s1 = 0; let s2 = 0; let s3 = 0;"
        "fn outer(){"
            "let x = 100; "
            "fn middle() { "
                "fn inner() {"
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
        "} let mid = outer(); let in = mid(); assert(in() == 106); assert(s1 == 1); assert(s2 == 2); assert(s3 == 3);",
        "for(let i = 0; i < 5; i++) { print i;}",
        "let a = 1; a++; assert(a == 2);"
        "a += 10; assert(a == 12);"
        "a /= 6; assert(a == 2);"
        "a *= 6; assert(a == 12);"
        "a -= 0; assert(a == 12);"
        "a += 0; assert(a == 12);"
        "a *= 0; assert(a == 0);",
        "let foo = \"one\"; foo += \" bar\";",

        "let p = list(1, 2, 3); assert(p.len() == 3);",
        "type Foo { fn init(name, list) { self.name = name; self.list = list;} fn len() { return self.list.len();}}"
        "let f = Foo(\"jason\", list(1,2,3));"
        "assert(f.len() == 3);"
        "f.list.append(\"one\");"
        "assert(f.len() == 4);"
        "let call = f.list.append; call(200);"
        "assert(f.len() == 5);",

        "print 1+2; print 3-1; print 4/2; print 10*10; print 1 == 1; print 2 != 4;",
        "print 2<4; print 4>2; print 4>=4; print 8<=9; print (!true);",
        "print false; print true; print nil;",
        "let foo = \"foo\"; { let bar = \"bar\"; let foobar = foo + bar; print foobar;}",
        "let foo = 10; let result = 0; if (foo > 10) { result = 1; } else { result = -1; }",
        "let counter = 0; while (counter < 10) { print counter; counter = counter + 1;}",
        "if (false or true) { print \"yep\"; }",
        "if (!false and true) { print \"yep\"; }",
        "for(let i = 0; i < 5; i = i + 1) { print i;}",
        "let counter = 0; for(1; counter < 5; counter = counter + 1) { print counter;}",
        "fn a() { print 1;} a();",
        "print clock();",
        "fn mk() {let l = \"local\"; fn inner() {print l;}return inner;} let closure = mk(); closure();",
        "fn outer() {let x = 1; x = 2;fn inner() {print x;} inner(); } outer();",
        "fn novalue() { return; } novalue();",

        "fn outer(){"
            "let x = 1; "
            "fn middle() { "
                "fn inner() {"
                    "print x;"
                "}"
                "print \"create inner closure\";"
                "return inner;"
            "}"
            "print \"return from outer\";"
            "return middle;"
        "} let mid = outer(); let in = mid(); in();",

        "let globalSet; "
        "let globalGet; "
        "fn main() { "
        "    let a = 1; let b = 100;"
        "    fn set() { a = 2; print b;} "
        "    fn get() { print a; b = 101;} "
        "    globalSet = set; "
        "    globalGet = get; "
        "} "
        "main(); "
        "globalSet(); "
        "globalGet();",

        "fn makeClosure() {\n"
        "    let a = \"data\";\n"
        "    fn f() { print a; }\n"
        "    return f;\n"
        "}\n"
        "{\n"
        "    let closure = makeClosure();\n"
        "    // GC here.\n"
        "    closure();\n"
        "}\n",

        // https://github.com/munificent/craftingvm_t_interpreters/issues/888
        "fn returnArg(arg){ return arg;}"
        "fn returnFunCallWithArg(func, arg){return returnArg(func)(arg);}"
        "fn printArg(arg){print arg;}"
        "returnFunCallWithArg(printArg, \"hello world\");",

        "let f1; let f2; { let i = 1; fn f() { print i; } f1 = f; } { let j = 2; fn f() { print j; } f2 = f; } f1(); f2();",

        // trigger gc
        "let x = 1; let y = 2; let z = x + y;"
        "fn lots_of_stuff() { let a = \"asdfasdfasdfasdfasdafsdfasdfasdfafs\"; let b = \"asdfsdfasdfasfdafsdfas\"; return a+b;}"
        "for (let i = 0; i < 1000; i = i + 1) { let r = lots_of_stuff(); print(r); print(x+y+z);}",

        "type Brioche {} print Brioche; print Brioche();",
        "type Pair {} let pair = Pair(); pair.first = 1; pair.second = 2; print pair.first + pair.second;",

        "type Meal { fn bacon() {} fn eggs() {} } let brunch = Meal(); let eggs = brunch.eggs; eggs();",

        "type Scone { fn topping(first, second) { print \"scone with \" + first + \" and \" + second; }}"
        "let scone = Scone(); scone.topping(\"berries\", \"cream\");",

        "type Person { fn say_name() {print self.name;} }"
        "let me = Person(); me.name = \"test\"; let method = me.say_name; method();",

        "type Nested { fn method() { fn function() { print self; } function(); } } Nested().method();",

        "type Brew { fn init(ingredient1, ingredient2) {} } Brew(\"grains\", \"hops\");",

        "type Beer { "
            "fn init(hops) { self.hops = hops; }"
            "fn brew() { print \"enjoy \" + self.hops; self.hops = nil; }"
        "}"
        "let maker = Beer(\"hops and grains\");"
        "maker.brew();",

        "type Oops { fn init() { fn f() { print \"not a method\"; } self.field = f; } } let oops = Oops(); oops.field();",

        "type PlainBagel { fn cook() { print(\"put it in the toaster.\"); } }"
        "type EverythingBagel (PlainBagel) { fn finish() { print(\"Glaze with icing.\"); } }"
        "let c = EverythingBagel(); c.cook(); c.finish();",

        "type A { fn method() { print(\"A method\");}}"
        "type B (A) { fn method() { print(\"B method\");} fn test() { super.method(); }}"
        "type C (B) {}"
        "C().test();",

        "type A { fn method() { print \"A\"; } }"
        "type B (A) { fn method() { let closure = super.method; closure(); } }" // prints "A"
        "B().method();",

        "type PlainBagel {"
            "fn cook() { print(\"put it in the toaster.\"); self.finish(\"cream cheese\"); }"
            "fn finish(ingredient) { print(\"Finish with \" + ingredient); }"
        "}"
        "type EverythingBagel (PlainBagel) {"
            "fn finish(ingredient) {"
                "super.finish(\"sea salt\");"
            "}"
        "}",

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
        "fn finish_and_quit() { print(\"working\"); exit(0); } finish_and_quit();",
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
        "fn finish_and_fail() { print(\"working\"); exit(-1); } finish_and_fail();",
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

        "let;",
        "let foo = 1",
        "{let foo = foo;}",
        // "}{",
        "if true ){}",
        " 1 = 3;",
        "{ let a = 1; let a = 2;}",
        "print self;",
        "fn not_a_method() { print self;}",
        "type CannotReturnFromInitializer { fn init() { return 1; } } CannotReturnFromInitializer(); ",
        "type Foo (Foo) {}",
        "type NoSuperClass { fn method() { super.method();}}",
        "fn NotClass() { super.NotClass(); }",
        "switch(3) { let statement_not_allowed_here = true; case 0: print(0); case 1: print(1); case 2: print(2); default: true; }",
        "switch(3) { default: true; case 3: print(\"cannot have case after default\"); }",
        "{ break;}",
        "type NoPropertiesOrMethods { oops_missing_fn() {}}",
        NULL,
    };
    for (int i = 0; compilation_fail_cases[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(compilation_fail_cases[i]);
        ck_assert_msg(rv == INTERPRET_COMPILE_ERROR, "Unexpected success for \"%s\"\n", compilation_fail_cases[i]);
        vm_t_free();
    }

    const char *runtime_fail_cases[] = {
        "fn no_args() {} no_args(1);", // arguments
        "fn has_args(v) {print(v);} has_args();", // arguments
        "let not_callable = 1; not_callable();", // cannot call
        "print(undefined_global);", // undefined
        "{ print(undefined_local);}",
        "let a = \"foo\"; a = -a;", // operand not a number
        "let a = \"foo\"; a = a + 1;", // operands must be same
        "a = 1;", // set global undefined variable
        "type OnlyOneArgInit { fn init(one) {} } let i = OnlyOneArgInit(1, 2);",
        "type NoArgInit {} let i = NoArgInit(1, 2);",
        "let NotClass = \"so not a type\"; type OhNo (NotClass) {}",
        "let a = 1; a = a / 0;", // divbyzero
        "let f = 1; f.foo = 1;", // only instances have property
        "let f = 1; f.foo(1);", // only instances have methods
        "type Foo {} let f = Foo(); f.nosuchproperty();",
        "type Foo {} let f = Foo(); let invalid = f.nosuchproperty;",
        "is();",
        "has_field();",
        "has_field(2, \"not a type\");",
        "set_field();",
        "set_field(true);",
        "set_field(true, true);",
        "set_field(true, true, true);",
        "type Foo {} let f = Foo(); set_field();",
        "type Foo {} let f = Foo(); set_field(f);",
        "type Foo {} let f = Foo(); set_field(f, \"fieldnoval\");",
        "type Foo {} let f = Foo(); get_field(f);",
        "type Foo {} let f = Foo(); get_field();",
        "let a = list(\"one\", 2, \"three\"); a.get(3);",
        "let a = \"foo\"; let f = a.nosuchmethod; f();",
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
        "type Animals { let Cat = \"cat\"; let Dog = \"dog\"; let Bird = \"bird\";} print(Animals.NoSuch);",
        "type Animals { let Cat = \"cat\"; let Dog = \"dog\"; let Bird = \"bird\";} Animals.Cat = 1;",
        NULL,
    };
    for (int i = 0; runtime_fail_cases[i] != NULL; i++) {
        vm_t_init();
        const vm_t_interpret_result_t rv = vm_t_interpret(runtime_fail_cases[i]);
        // in case we hose up a bad compile
        ck_assert_msg(rv != INTERPRET_COMPILE_ERROR, "Compile failed for: %s\n", runtime_fail_cases[i]);
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

    value_list_t a;
    value_list_t_init(&a);
    value_list_t_add(&a, NUMBER_VAL(9));
    value_list_t_add(&a, BOOL_VAL(false));
    value_list_t_add(&a, NIL_VAL);
    value_list_t_add(&a, EMPTY_VAL);
    for (int i = 0; i < a.count; i++) {
        value_t_print(stdout, a.values[i]);
    }
    value_list_t_free(&a);

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

    obj_string_t *typeobj_name = obj_string_t_copy_from("TestObjectTestCase", 18);
    vm_push(OBJ_VAL(typeobj_name));
    obj_typeobj_t *typeobj = obj_typeobj_t_allocate(typeobj_name);
    vm_push(OBJ_VAL(typeobj));
    obj_instance_t *instance = obj_instance_t_allocate(typeobj);
    vm_push(OBJ_VAL(instance));


    obj_list_t *list = obj_list_t_allocate();
    vm_push(OBJ_VAL(list));

    // debugging
    obj_type_t_to_str(((obj_t*)typeobj_name)->type);
    obj_type_t_to_str(((obj_t*)typeobj)->type);
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

    vm_toggle_gc_stress();
    vm_toggle_gc_trace();
    vm_toggle_stack_trace();

    // trigger gc
    const char *program =
        "let x = 1; let y = 2; let z = x + y; let z2 = z - 1; let t = true; let f = false; let invalid = !true;"
        "let alist = list(\"one\", \"two\", 3); assert(alist.len() == 3); assert(str(alist.get(-1)) == \"3\"); str(alist);"
        "type Point { fn init(x,y) { self.x = x; self.y = y;} fn dostuff() { z = z + x + y; }}"
        "type SubPoint (Point) { fn init(x,y) { super.init(x/2,y*10); }} let sp= SubPoint(10, 20); assert(sp.x == 5);"
        "let f1; { let i = 100; fn inner() { print i;} f1 = inner;}"
        "let f2 = Point(-900, -900).dostuff;"
        "let alistlen = alist.len;"
        "fn lots_of_stuff() { "
            "let a = \"asdfasdfasdfasdfasdafsdfasdfasdfafs\";"
            "let b = \"asdfsdfasdfasfdafsdfas\";"
            "{print(a+b);}"
            "{ assert(alistlen() == 3);}"
            "let p = Point(100, 200);"
            "{str(p); str(100); str(alistlen); str(true); str(nil); str(Point); let c = Point(1,2).dostuff; str(c);}"
            "{ p.dostuff(); print(p.x + p.y); p.x < p.y; p.x > p.y; print(a + b); f1(); f2();}"
            "return p;"
        "}"
        "{str(lots_of_stuff);}"
        "for (let i = 0; i < 1000; i = i + 1) {"
            "let r = lots_of_stuff();"
            "print(r);"
        "}";
    ck_assert(vm_t_interpret(program) == INTERPRET_OK);

    const char *programs_with_tracing[] = {
        // https://github.com/munificent/craftingvm_t_interpreters/issues/888
        "fn returnArg(arg){ return arg;}"
        "fn returnFunCallWithArg(func, arg){return returnArg(func)(arg);}"
        "fn printArg(arg){print arg;}"
        "returnFunCallWithArg(printArg, \"hello world\");",

        // OP_CLOSE_UPVALUE https://github.com/munificent/craftingvm_t_interpreters/issues/746
        "let f1; let f2; { let i = 1; fn f() { print i; } f1 = f; } { let j = 2; fn f() { print j; } f2 = f; } f1(); f2();",

        "type A { fn method() { print \"A\"; } }"
        "type B (A) { fn method() { let closure = super.method; closure(); } }" // prints "A"
        "B().method();",

        "type PlainBagel {"
            "fn cook() { print(\"put it in the toaster.\"); self.finish(\"cream cheese\"); }"
            "fn finish(ingredient) { print(\"Finish with \" + ingredient); }"
        "}"
        "type EverythingBagel (PlainBagel) {"
            "fn finish(ingredient) {"
                "super.finish(\"sea salt\");"
            "}"
        "}",
        NULL,
    };
    for (int p = 0; programs_with_tracing[p] != NULL; p++) {
        ck_assert_msg(vm_t_interpret(programs_with_tracing[p]) == INTERPRET_OK, "Failed: %s\n", programs_with_tracing[i]);
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
    const char *suite_name = "tater";

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
