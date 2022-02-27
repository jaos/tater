#!/usr/bin/env python3
"""lox"""
# pylint: disable=line-too-long,too-many-arguments,multiple-statements,too-many-lines
import enum
import time
import typing


class TokenType(enum.Enum):
    """Token types"""
    # Single-character tokens.
    LEFT_PAREN = enum.auto()
    RIGHT_PAREN = enum.auto()
    LEFT_BRACE = enum.auto()
    RIGHT_BRACE = enum.auto()

    COMMA = enum.auto()
    DOT = enum.auto()
    MINUS = enum.auto()
    MINUS_EQUAL = enum.auto()
    MINUS_MINUS = enum.auto()
    PLUS = enum.auto()
    PLUS_EQUAL = enum.auto()
    PLUS_PLUS = enum.auto()
    SEMICOLON = enum.auto()
    SLASH = enum.auto()
    SLASH_EQUAL = enum.auto()
    STAR = enum.auto()
    STAR_EQUAL = enum.auto()
    QUESTION = enum.auto()
    COLON = enum.auto()

    # One or two character tokens.
    BANG = enum.auto()
    BANG_EQUAL = enum.auto()
    EQUAL = enum.auto()
    EQUAL_EQUAL = enum.auto()
    GREATER = enum.auto()
    GREATER_EQUAL = enum.auto()
    LESS = enum.auto()
    LESS_EQUAL = enum.auto()

    # Literals.
    IDENTIFIER = enum.auto()
    STRING = enum.auto()
    NUMBER = enum.auto()

    # Keywords.
    AND = enum.auto()
    BREAK = enum.auto()
    CLASS = enum.auto()
    CONTINUE = enum.auto()
    ELSE = enum.auto()
    FALSE = enum.auto()
    FUN = enum.auto()
    FOR = enum.auto()
    IF = enum.auto()
    NIL = enum.auto()
    OR = enum.auto()

    PRINT = enum.auto()
    RETURN = enum.auto()
    SUPER = enum.auto()
    THIS = enum.auto()
    TRUE = enum.auto()
    VAR = enum.auto()
    WHILE = enum.auto()

    EOF = enum.auto()


KEYWORDS = {
    'and':     TokenType.AND,
    'break':   TokenType.BREAK,
    'class':   TokenType.CLASS,
    'continue':TokenType.CONTINUE,
    'else':    TokenType.ELSE,
    'false':   TokenType.FALSE,
    'for':     TokenType.FOR,
    'fun':     TokenType.FUN,
    'if':      TokenType.IF,
    'nil':     TokenType.NIL,
    'or':      TokenType.OR,
    'print':   TokenType.PRINT,
    'return':  TokenType.RETURN,
    'super':   TokenType.SUPER,
    'this':    TokenType.THIS,
    'true':    TokenType.TRUE,
    'var':     TokenType.VAR,
    'while':   TokenType.WHILE,
}


class Token:
    """Token"""
    def __init__(self, token_type, lexeme, literal, line, column) -> None:
        self.type = token_type
        self.lexeme = lexeme
        self.literal = literal
        self.line = line
        self.column = column

    def __str__(self) -> str:
        return f"{self.type} {self.lexeme} {self.literal}"

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({self.type}, {self.lexeme!r}, {self.literal!r}, {self.line!r}, {self.column!r})"

    def __eq__(self, other) -> bool:
        if not isinstance(other, self.__class__):
            return False
        return (
            self.type == other.type
            and self.lexeme == other.lexeme
            and self.literal == other.literal
            and self.line == other.line
            and self.column == other.column
        )


class Scanner:
    """Scanner"""

    class ScannerError(RuntimeError):
        """Scanner error"""
        def __init__(self, line, offset, *args, **kwargs) -> None:
            super().__init__(*args, **kwargs)
            self.scanner_line = line
            self.scanner_offset = offset

    def __init__(self, source) -> None:
        self.source = source
        self.tokens:typing.List[Token] = []
        self.start = 0
        self.current = 0
        self.line = 1

        while not self.is_at_end:
            self.start = self.current
            self.scan_token()
        self.tokens.append(Token(TokenType.EOF, "", None, self.line, -1))

    is_at_end = property(lambda self: self.current >= len(self.source))
    next_is_at_end = property(lambda self: (self.current + 1) >= len(self.source))
    peek = property(lambda self: self.source[self.current] if not self.is_at_end else '\0')
    peek_next = property(lambda self: self.source[self.current + 1] if not self.next_is_at_end else '\0')

    def advance(self) -> str:
        """advance to the next character"""
        c = self.source[self.current]
        self.current += 1
        return c

    def match(self, expected) -> bool:
        """match current character"""
        if self.is_at_end: return False
        if self.source[self.current] != expected: return False
        self.current += 1
        return True

    def scan_error(self, msg, fatal=False) -> None:
        """scanner error handling"""
        lines = self.source.split('\n')
        line = lines[self.line - 1]
        print(f"Error: {msg} at line {self.line}, column {self.start}")
        print(f"\t{line}")
        if fatal:
            raise self.ScannerError(line, self.start, msg)

    def string(self) -> None:
        """scan a string"""
        while self.peek != '"' and not self.is_at_end:
            if self.peek == '\n': self.line += 1
            self.advance()

        if self.is_at_end:
            self.scan_error("Unterminated string", True)

        self.advance()

        value = self.source[self.start + 1: self.current - 1]
        self.add_token(TokenType.STRING, value)

    def number(self) -> None:
        """scan a number"""
        while self.peek.isdigit(): self.advance()
        if self.peek == '.' and self.peek_next.isdigit(): self.advance()
        while self.peek.isdigit(): self.advance()
        value = self.source[self.start:self.current]
        value = float(value) if '.' in value else int(value)
        self.add_token(TokenType.NUMBER, value)

    def identifier(self) -> None:
        """scan an identifier"""
        while self.peek.isalnum() or self.peek == '_': self.advance()
        value = self.source[self.start:self.current]
        if value in KEYWORDS:
            self.add_token(KEYWORDS[value])
        else:
            self.add_token(TokenType.IDENTIFIER)

    def add_token(self, token_type:TokenType, literal=None) -> None:
        """add a scanned token"""
        text = self.source[self.start:self.current]
        self.tokens.append(Token(token_type, text, literal, self.line, self.start))

    def scan_token(self) -> None:  # noqa:C901 # too-complex
        """scan for a token"""
        # pylint: disable=too-many-statements,too-many-branches
        c = self.advance()
        if c == '(': self.add_token(TokenType.LEFT_PAREN)
        elif c == ')': self.add_token(TokenType.RIGHT_PAREN)
        elif c == '{': self.add_token(TokenType.LEFT_BRACE)
        elif c == '}': self.add_token(TokenType.RIGHT_BRACE)
        elif c == ',': self.add_token(TokenType.COMMA)
        elif c == '?': self.add_token(TokenType.QUESTION)
        elif c == ':': self.add_token(TokenType.COLON)
        elif c == '.': self.add_token(TokenType.DOT)
        elif c == '-':
            if self.match('='):
                self.add_token(TokenType.MINUS_EQUAL)
            elif self.match('-'):
                self.add_token(TokenType.MINUS_MINUS)
            else:
                self.add_token(TokenType.MINUS)
        elif c == '+':
            if self.match('='):
                self.add_token(TokenType.PLUS_EQUAL)
            elif self.match('+'):
                self.add_token(TokenType.PLUS_PLUS)
            else:
                self.add_token(TokenType.PLUS)
        elif c == ';': self.add_token(TokenType.SEMICOLON)
        elif c == '*':
            if self.match('='):
                self.add_token(TokenType.STAR_EQUAL)
            else:
                self.add_token(TokenType.STAR)
        elif c == '!': self.add_token(TokenType.BANG_EQUAL if self.match('=') else TokenType.BANG)
        elif c == '=': self.add_token(TokenType.EQUAL_EQUAL if self.match('=') else TokenType.EQUAL)
        elif c == '<': self.add_token(TokenType.LESS_EQUAL if self.match('=') else TokenType.LESS)
        elif c == '>': self.add_token(TokenType.GREATER_EQUAL if self.match('=') else TokenType.GREATER)
        elif c == '/':
            if self.match('/'):
                while self.peek != '\n' and not self.is_at_end: self.advance()
            elif self.match('='):
                self.add_token(TokenType.SLASH_EQUAL)
            else:
                self.add_token(TokenType.SLASH)
        elif c in (' ', '\t', '\r'): pass
        elif c == '\n': self.line += 1
        elif c == '"': self.string()
        else:
            if c.isdigit():
                self.number()
            elif c.isidentifier():
                self.identifier()
            else:
                self.scan_error(f"Unexpected character {c}")


class Expr:
    """Expression"""
    def __repr__(self):
        return f"{self.__class__.__name__}({', '.join(f'{k}={v!r}' for k,v in self.__dict__.items())})"

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        return self.__dict__ == other.__dict__

    def __hash__(self):
        return hash(repr(self))


class Stmt:
    """Statement"""
    def __repr__(self):
        return f"{self.__class__.__name__}({', '.join(f'{k}={v!r}' for k,v in self.__dict__.items())})"

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        return self.__dict__ == other.__dict__


class Binary(Expr):
    """Binary expression"""
    def __init__(self, left:Expr, operator:Token, right:Expr):
        self.left = left
        self.operator = operator
        self.right = right


class Conditional(Expr):
    """Conditional expression"""
    def __init__(self, expression:Expr, left:Expr, right:Expr):
        self.expression = expression
        self.left = left
        self.right = right


class Assign(Expr):
    """Assignment expression"""
    def __init__(self, name:Token, value:Expr):
        self.name = name
        self.value = value


class Call(Expr):
    """Call expression"""
    def __init__(self, callee:Expr, paren:Token, arguments:typing.List[Expr]):
        self.callee = callee
        self.paren = paren
        self.arguments = arguments


class Get(Expr):
    """Get on object expression"""
    def __init__(self, obj:Expr, name:Token):
        self.object = obj
        self.name = name


class Grouping(Expr):
    """Grouping expression"""
    def __init__(self, expression:Expr):
        self.expression = expression


class Literal(Expr):
    """Literal expression"""
    def __init__(self, value:typing.Union[str, int, float]):
        self.value = value


class Logical(Expr):
    """Logical expression"""
    def __init__(self, left:Expr, operator:Token, right:Expr):
        self.left = left
        self.operator = operator
        self.right = right


class Set(Expr):
    """Set on object expression"""
    def __init__(self, obj:Expr, name:Token, value:Expr):
        self.object = obj
        self.name = name
        self.value = value


class Super(Expr):
    """Super on object expression"""
    def __init__(self, keyword:Token, method:Token):
        self.keyword = keyword
        self.method = method


class This(Expr):
    """This in object method expression"""
    def __init__(self, keyword:Token):
        self.keyword = keyword


class Unary(Expr):
    """Unary expression"""
    def __init__(self, operator:Token, right:Expr):
        self.operator = operator
        self.right = right


class Variable(Expr):
    """Variable expression"""
    def __init__(self, name:Token):
        self.name = name


class Block(Stmt):
    """Block statement"""
    def __init__(self, statements:typing.List[Stmt]):
        self.statements = statements


class FunctionType(enum.Enum):
    """function types"""
    FUNCTION = "function"
    METHOD = "method"

    def __str__(self):
        return self.value


class Function(Stmt):
    """Function statement"""
    def __init__(self, name:Token, params:typing.List[Token], body:typing.List[Stmt]):
        self.name = name
        self.params = params
        self.body = body


class Class(Stmt):
    """Class statement"""
    def __init__(self, name:Token, superclass:Variable, methods:typing.List[Function]):
        self.name = name
        self.superclass = superclass
        self.methods = methods


class Expression(Stmt):
    """Expression statement"""
    def __init__(self, expression:Expr):
        self.expression = expression


class ForExpression(Expression):
    """Special type for for loop increment expressions that get tagged at the end of a body of statements"""


class If(Stmt):
    """If statement"""
    def __init__(self, condition:Expr, then_branch:Stmt, else_branch:Stmt):
        self.condition = condition
        self.then_branch = then_branch
        self.else_branch = else_branch


class Print(Stmt):
    """Print statement"""
    def __init__(self, expression:Expr):
        self.expression = expression


class Return(Stmt):
    """Return statement"""
    def __init__(self, keyword:Token, value:Expr):
        self.keyword = keyword
        self.value = value


class Var(Stmt):
    """Var statement"""
    def __init__(self, name:Token, initializer:Expr):
        self.name = name
        self.initializer = initializer


class While(Stmt):
    """While statement"""
    def __init__(self, condition:Expr, body:Stmt):
        self.condition = condition
        self.body = body


class Break(Stmt):
    """Break statement"""


class Continue(Stmt):
    """Continue statement"""


class Parser:
    """
    program        → declaration* EOF ;
    declaration    → funDecl | varDecl | statement ;
    funDecl        → "fun" function ;
    function       → IDENTIFIER "(" parameters? ")" block ;
    parameters     → IDENTIFIER ( "," IDENTIFIER )* ;
    varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;
    statement      → exprStmt | forStmt | ifStmt | printStmt | returnStmt | whileStmt | breakStmt | continueStmt | block ;
    exprStmt       → expression ";" ;
    forStmt        → "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")" statement ;
    ifStmt         → "if" "(" expression ")" statement ( "else" statement )? ;
    printStmt      → "print" expression ";" ;
    returnStmt     → "return" expression? ";" ;
    whileStmt      → "while" "(" expression ")" statement ;
    breakStmt      → "break" ";" ;
    continueStmt   → "continue" ";" ;
    block          → "{" declaration* "}" ;

    expression     → assignment ;
    assignment     → IDENTIFIER "=" assignment | conditional ;
    conditional    → logic_or ( "?" expression ":" conditional )? ;
    logic_or       → logic_and ( "or" logic_and )* ;
    logic_and      → equality ( "and" equality )* ;
    equality       → comparison ( ( "!=" | "==" ) comparison )* ;
    comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
    term           → factor ( ( "-" | '--' | '-=' | "+" | '++' | '+=') factor )* ;
    factor         → unary ( ( "/" | '/=' | "*" | "*=" ) unary )* ;
    unary          → ( "!" | "-" ) unary | call ;
    call           → primary ( "(" arguments? ")" )* ;
    arguments      → expression ( "," expression )* ;
    primary        → NUMBER | STRING | "true" | "false" | "nil" | "(" expression ")" | IDENTIFIER
                   // Error productions...
                   | ( "!=" | "==" ) equality
                   | ( ">" | ">=" | "<" | "<=" ) comparison
                   | ( "+" ) term
                   | ( "/" | "*" ) factor ;
    """
    # pylint: disable=too-many-public-methods
    previous = property(lambda self: self.tokens[self.current - 1], doc="return the previous token")
    peek = property(lambda self: self.tokens[self.current], doc="return the current token")
    is_at_end = property(lambda self: self.peek.type == TokenType.EOF, doc="check if we are at the EOF token")

    class ParserError(RuntimeError):
        """Parser error"""

    def __init__(self, scanner:Scanner):
        """Take the scanner and parse

            This could just take the scanner tokens but having the full scanner means we might be
            able to get better error messages.
        """
        self.scanner = scanner
        self.tokens = scanner.tokens  # dumb ref
        self.current = 0
        self.has_error = False
        self.loop_depth = 0
        self.call_depth = 0

    def parse(self) -> typing.List[Stmt]:
        """main entry point to start the parsing"""
        statements:typing.List[Stmt] = []
        while not self.is_at_end:
            stmt:typing.Optional[Stmt] = self.declaration()
            if stmt:
                statements.append(stmt)
        return statements

    def declaration(self) -> typing.Union[Stmt, Expr, None]:
        """declaration    → varDecl | statement ;"""
        try:
            if self.match(TokenType.FUN): return self.function(FunctionType.FUNCTION)
            if self.match(TokenType.VAR): return self.var_declaration()
            return self.statement()
        except self.ParserError as exc:
            print(exc)
            self.synchronize()
            return None

    def function(self, kind:FunctionType) -> Stmt:
        """function       → IDENTIFIER "(" parameters? ")" block ;"""
        name:Token = self.consume(TokenType.IDENTIFIER, f"Expec {kind} name")

        self.consume(TokenType.LEFT_PAREN, f"Expect '(' after {kind} name.")
        parameters:typing.List[Token] = []
        if not self.check(TokenType.RIGHT_PAREN):
            parameters.append(self.consume(TokenType.IDENTIFIER, "Expect parameter name."))
            while self.match(TokenType.COMMA):
                parameters.append(self.consume(TokenType.IDENTIFIER, "Expect parameter name."))
                if len(parameters) >= 255:
                    self.error(self.peek, "Can't have more than 255 parameters.")
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after parameters.")

        self.consume(TokenType.LEFT_BRACE, "Expect '{' before " + str(kind) + " body.")
        self.call_depth += 1
        body:typing.List[Stmt] = self.block()
        self.call_depth -= 1
        return Function(name, parameters, body)

    def var_declaration(self) -> Expr:
        """varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;"""
        name:Token = self.consume(TokenType.IDENTIFIER, "Expect variable name.")
        initializer:Expr = self.expression() if self.match(TokenType.EQUAL) else None
        self.consume(TokenType.SEMICOLON, "Expect ';' after variable declaration.")
        return Var(name, initializer)

    def statement(self) -> Stmt:
        """
            statement      → exprStmt
                           | printStmt ;
        """
        # pylint: disable=too-many-return-statements
        if self.match(TokenType.FOR): return self.for_statement()
        if self.match(TokenType.IF): return self.if_statement()
        if self.match(TokenType.PRINT): return self.print_statement()
        if self.match(TokenType.RETURN): return self.return_statement()
        if self.match(TokenType.WHILE): return self.while_statement()
        if self.match(TokenType.BREAK): return self.break_statement()
        if self.match(TokenType.CONTINUE): return self.continue_statement()
        if self.match(TokenType.LEFT_BRACE): return Block(self.block())  # instantiate Block here so as to reuse block() for functions etc
        return self.expression_statement()

    def return_statement(self) -> Stmt:
        """returnStmt     → "return" expression? ";" ;"""
        if not self.call_depth:
            self.error(self.previous, "Must be inside a callable to use 'return'.")
        keyword:Token = self.previous
        value:typing.Optional[Expr] = None

        if not self.check(TokenType.SEMICOLON):
            value = self.expression()

        self.consume(TokenType.SEMICOLON, "Expect ';' after return value.")
        return Return(keyword, value)

    def break_statement(self) -> Stmt:
        """breakStmt      → "break" ";" ;"""
        if self.loop_depth == 0:
            self.error(self.previous, "Must be inside a loop to use 'break'.")
        self.consume(TokenType.SEMICOLON, "Expect ';' after break statement.")
        return Break()

    def continue_statement(self) -> Stmt:
        """continueStmt   → "continue" ";" ;"""
        if self.loop_depth == 0:
            self.error(self.previous, "Must be inside a loop to use 'continue'.")
        self.consume(TokenType.SEMICOLON, "Expect ';' after continue statement.")
        return Continue()

    def block(self) -> typing.List[Stmt]:
        """block          → "{" declaration* "}" ;"""
        statements:typing.List[Stmt] = []

        while not self.check(TokenType.RIGHT_BRACE) and not self.is_at_end:
            statements.append(self.declaration())

        self.consume(TokenType.RIGHT_BRACE, "Expect '}' after block.")
        return statements

    def for_statement(self) -> Stmt:
        """forStmt        → "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")" statement ;"""
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'for'.")
        initializer:Stmt = None
        if self.match(TokenType.SEMICOLON):
            pass
        elif self.match(TokenType.VAR):
            initializer = self.var_declaration()
        else:
            initializer = self.expression_statement()

        condition:Expr = None
        if not self.check(TokenType.SEMICOLON):
            condition = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after loop condition.")

        increment:Expr = None
        if not self.check(TokenType.RIGHT_PAREN):
            increment = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after for clauses.")

        try:
            self.loop_depth += 1
            body = self.statement()

            if increment:
                body = Block([body, ForExpression(increment)])

            if not condition:
                condition = Literal(True)

            body = While(condition, body)

            if initializer:
                body = Block([initializer, body])

            return body
        finally:
            self.loop_depth -= 1

    def if_statement(self) -> Stmt:
        """ifStmt         → "if" "(" expression ")" statement ( "else" statement )? ;"""
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'if'.")
        condition:Expr = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after if condition.")

        then_branch:Stmt = self.statement()
        else_branch:Stmt = None
        if self.match(TokenType.ELSE):
            else_branch = self.statement()
        return If(condition, then_branch, else_branch)

    def while_statement(self) -> Stmt:
        """whileStmt      → "while" "(" expression ")" statement ;"""
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'while'.")
        condition:Expr = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after condition.")
        try:
            self.loop_depth += 1
            body:Stmt = self.statement()
            return While(condition, body)
        finally:
            self.loop_depth -= 1

    def expression_statement(self) -> Stmt:
        """ exprStmt       → expression ";" ;"""
        expr:Expr = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after expression.")
        return Expression(expr)

    def print_statement(self) -> Stmt:
        """printStmt      → "print" expression ";" ;"""
        value:Expr = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after value.")
        return Print(value)

    def error(self, token:Token, msg:str) -> None:
        """Error handling"""
        self.has_error = True
        if token.type == TokenType.EOF:
            raise self.ParserError(f"Parser error on line {token.line}, unexpected EOF: {msg}")
        raise self.ParserError(f"Parser error on line {token.line} at {token.lexeme}: {msg}")

    def advance(self) -> Token:
        """advance to the next token"""
        if not self.is_at_end:
            self.current += 1
        return self.previous

    def check(self, token_type:TokenType) -> bool:
        """check the current token for a given TokenType"""
        if self.is_at_end: return False
        return self.peek.type == token_type

    def match(self, *types:TokenType) -> bool:
        """Match the current token for a list of TokenTypes"""
        for t in types:
            if self.check(t):
                self.advance()
                return True
        return False

    def expression(self) -> Expr:
        """expression     → equality ;"""
        expr:Expr = self.assignment()

        if self.match(TokenType.EQUAL):
            equals:Token = self.previous
            value:Expr = self.assignment()

            if isinstance(expr, Variable):
                name:Token = expr.name
                return Assign(name, value)

            self.error(equals, "Invalid assignment target.")

        return expr

    def assignment(self) -> Expr:
        """
            assignment     → IDENTIFIER "=" assignment
                           | conditional ;
        """
        return self.conditional()

    def conditional(self) -> Expr:
        """conditional    → equality ( "?" expression ":" conditional )? ; """
        expr:Expr = self.logic_or()
        if self.match(TokenType.QUESTION):
            then_branch:Expr = self.expression()
            self.consume(TokenType.COLON, "Expect ':' after then branch of conditional expression.")
            else_branch:Expr = self.conditional()
            expr = Conditional(expr, then_branch, else_branch)

        return expr

    def logic_or(self) -> Expr:
        """logic_or       → logic_and ( "or" logic_and )* ;"""
        expr:Expr = self.logic_and()
        while self.match(TokenType.OR):
            operator:Token = self.previous
            right:Expr = self.logic_and()
            expr = Logical(expr, operator, right)
        return expr

    def logic_and(self) -> Expr:
        """logic_and      → equality ( "and" equality )* ;"""
        expr:Expr = self.equality()
        while self.match(TokenType.AND):
            operator:Token = self.previous
            right:Expr = self.equality()
            expr = Logical(expr, operator, right)
        return expr

    def equality(self) -> Expr:
        """equality       → comparison ( ( "!=" | "==" ) comparison )* ;"""
        expr:Expr = self.comparison()

        while self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            operator:Token = self.previous
            right:Expr = self.comparison()
            expr = Binary(expr, operator, right)

        return expr

    def comparison(self) -> Expr:
        """comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;"""
        expr:Expr = self.term()

        while self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            operator:Token = self.previous
            right:Expr = self.term()
            expr = Binary(expr, operator, right)

        return expr

    def term(self) -> Expr:
        """term           → factor ( ( "-" | '--' | '-=' | "+" | '++' | '+=') factor )* ;"""
        expr:Expr = self.factor()

        while self.match(TokenType.MINUS_MINUS, TokenType.PLUS_PLUS):  # precedence order
            operator:Token = self.previous
            expr = Assign(expr.name, Binary(expr, operator, Literal(1)))

        while self.match(TokenType.MINUS_EQUAL, TokenType.PLUS_EQUAL):  # precedence order
            operator:Token = self.previous
            right:Expr = self.factor()
            expr = Assign(expr.name, Binary(expr, operator, right))

        while self.match(TokenType.MINUS, TokenType.PLUS):  # precedence order
            operator:Token = self.previous
            right:Expr = self.factor()
            expr = Binary(expr, operator, right)

        return expr

    def factor(self) -> Expr:
        """factor         → unary ( ( "/" | '/=' | "*" | "*=" ) unary )* ;"""
        expr:Expr = self.unary()

        while self.match(TokenType.SLASH_EQUAL, TokenType.STAR_EQUAL):
            operator:Token = self.previous
            right:Expr = self.unary()
            expr = Assign(expr.name, Binary(expr, operator, right))

        while self.match(TokenType.SLASH, TokenType.STAR):
            operator:Token = self.previous
            right:Expr = self.unary()
            expr = Binary(expr, operator, right)

        return expr

    def unary(self) -> Expr:
        """ unary          → ( "!" | "-" ) unary
                             primary ;
        """
        if self.match(TokenType.BANG, TokenType.MINUS):
            operator:Token = self.previous
            right:Expr = self.unary()
            return Unary(operator, right)
        return self.call()

    def call(self) -> Expr:
        """call           → primary ( "(" arguments? ")" )* ;"""
        expr:Expr = self.primary()
        while self.match(TokenType.LEFT_PAREN):
            expr = self.finish_call(expr)
        return expr

    def finish_call(self, callee:Expr) -> Expr:
        """..."""
        arguments:typing.List[Expr] = []
        if not self.check(TokenType.RIGHT_PAREN):
            arguments.append(self.expression())
            while self.match(TokenType.COMMA):
                arguments.append(self.expression())
        if len(arguments) > 255:
            self.error(self.peek, "Can't have more than 255 arguments.")
        paren:Token = self.consume(TokenType.RIGHT_PAREN, "Expect ')' after arguments.")
        return Call(callee, paren, arguments)

    def primary(self) -> typing.Optional[Expr]:  # noqa:C901 # too-complex
        """primary        → NUMBER | STRING | "true" | "false" | "nil"
                          | "(" expression ")" ;
        """
        # pylint: disable=too-many-return-statements
        if self.match(TokenType.FALSE): return Literal(False)
        if self.match(TokenType.TRUE): return Literal(True)
        if self.match(TokenType.NIL): return Literal(None)

        if self.match(TokenType.NUMBER, TokenType.STRING):
            return Literal(self.previous.literal)

        if self.match(TokenType.LEFT_PAREN):
            expr:Expr = self.expression()
            self.consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.")
            return Grouping(expr)

        if self.match(TokenType.IDENTIFIER):
            return Variable(self.previous)

        # handle error productions here
        if self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            self.error(self.peek, "Missing left-hand operand.")
            self.equality()
            return None

        if self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            self.error(self.peek, "Missing left-hand operand.")
            self.comparison()
            return None

        if self.match(TokenType.PLUS):
            self.error(self.peek, "Missing left-hand operand.")
            self.term()
            return None

        if self.match(TokenType.SLASH, TokenType.STAR):
            self.error(self.peek, "Missing left-hand operand.")
            self.factor()
            return None

        return self.error(self.peek, "Expression expected")

    def consume(self, token_type:TokenType, msg:str) -> None:
        """check for next expected token, raise error msg if not"""
        if self.check(token_type): return self.advance()
        return self.error(self.peek, msg)

    SYNCHRONIZE_TOKEN_TYPES = (
        TokenType.CLASS,
        TokenType.FUN,
        TokenType.VAR,
        TokenType.FOR,
        TokenType.IF,
        TokenType.WHILE,
        TokenType.PRINT,
        TokenType.RETURN
    )

    def synchronize(self) -> None:
        """Synchronization step for the parser"""
        self.advance()
        while not self.is_at_end:
            if self.previous.type == TokenType.SEMICOLON: return
            if self.peek.type in self.__class__.SYNCHRONIZE_TOKEN_TYPES: return
            self.advance()


class Environment:
    """Environment to store variables and values"""

    class RuntimeError(RuntimeError):
        """Environment runtime error"""

    def __init__(self, enclosing:'Environment' = None):
        """Optional enclosing environment scope to check on get/assign"""
        self.values = {}
        self.enclosing = enclosing

    def define(self, name:str, value:typing.Any) -> None:
        """bind a name to a value"""
        if name in self.values:
            raise self.RuntimeError(f"Var {name} is already defined.")
        self.values[name] = value

    def get(self, token:Token) -> typing.Any:
        """return a value by name"""
        try:
            return self.values[token.lexeme]
        except KeyError as exc:
            if self.enclosing:
                return self.enclosing.get(token)
            raise self.RuntimeError(f"Undefined variable {token.lexeme}") from exc

    def get_at(self, distance:int, token:Token) -> typing.Any:
        """return a value by distance and name"""
        return self.ancestor(distance).get(token)

    def ancestor(self, distance:int) -> 'Environment':
        """Return the ancestor environment at distance"""
        environment = self
        while distance > 0:
            environment = environment.enclosing
            distance -= 1
        return environment

    def assign(self, token:Token, value:typing.Any) -> None:
        """Assign a value"""
        if token.lexeme not in self.values:
            if self.enclosing:
                self.enclosing.assign(token, value)
                return
            raise self.RuntimeError(f"Undefined variable {token.lexeme}")
        self.values[token.lexeme] = value

    def assign_at(self, distance:int, token:Token, value:typing.Any) -> None:
        """Assign a value by distance"""
        self.ancestor(distance).assign(token, value)

    def __repr__(self):
        return str(self.values)


class Resolver:
    """Variable resolver that walks the Scanner tree"""

    class RuntimeError(RuntimeError):
        """Resolver runtime error"""

    class VariableStatus(enum.Enum):
        """Variable status"""
        DECLARED = enum.auto()
        DEFINED = enum.auto()
        USED = enum.auto()

    def __init__(self, interpreter:'Interpreter'):
        self.interpreter = interpreter
        self.scopes = []
        self.current_function:typing.Optional[FunctionType] = None

    def error(self, token:Token, msg):
        """error handling"""
        raise self.RuntimeError(f"{token.lexeme}: {msg}")

    def begin_scope(self):
        """begin a scope"""
        self.scopes.append({})

    def end_scope(self):
        """end a scope"""
        for k, v in self.scopes[-1].items():
            if v is not self.VariableStatus.USED:
                print(f"Warning: unused variable {k}")
        self.scopes.pop()

    def declare(self, name:Token):
        """declare a variable"""
        if not self.scopes: return
        if name.lexeme in self.scopes[-1]:
            self.error(name, "Already a variable with this name in this scope.")
        self.scopes[-1][name.lexeme] = self.VariableStatus.DECLARED

    def define(self, name:Token):
        """define a variable"""
        if not self.scopes: return
        self.scopes[-1][name.lexeme] = self.VariableStatus.DEFINED

    def _resolve_local(self, expr:Expr, token:Token, used:bool = False):
        """Resolve the scope depth of a variable"""
        for idx, scope in enumerate(self.scopes):
            if token.lexeme in scope:
                self.interpreter.resolve(expr, idx)
                if used:
                    scope[token.lexeme] = self.VariableStatus.USED
                return

    def expression(self, expr:Expr):
        """resolve variable use in an expression"""
        # pylint: disable=too-many-return-statements
        if isinstance(expr, Variable):
            if self.scopes and self.scopes[-1].get(expr.name.lexeme) == self.VariableStatus.DECLARED:
                self.error(expr.name, "Can't read local variable in its own initializer.")
            self._resolve_local(expr, expr.name, True)
            return None
        if isinstance(expr, Assign):
            self.expression(expr.value)
            self._resolve_local(expr, expr.name)
            return None
        if isinstance(expr, (Binary, Logical)):
            self.expression(expr.left)
            self.expression(expr.right)
            return None
        if isinstance(expr, Call):
            self.expression(expr.callee)
            for argument in expr.arguments:
                self.expression(argument)
            return None
        if isinstance(expr, Grouping):
            self.expression(expr.expression)
            return None
        if isinstance(expr, Unary):
            self.expression(expr.right)
            return None
        if isinstance(expr, Literal):
            return None

        return None

    def statement(self, stmt:Stmt):  # noqa: C901
        """resolve variable use in a statement"""
        # pylint: disable=too-many-return-statements
        if isinstance(stmt, Var):
            self.declare(stmt.name)
            if stmt.initializer is not None:
                self.expression(stmt.initializer)
            self.define(stmt.name)
            return None
        if isinstance(stmt, Function):
            self.declare(stmt.name)
            self.define(stmt.name)
            self.function(stmt, FunctionType.FUNCTION)
            return None
        if isinstance(stmt, Expression):
            self.expression(stmt.expression)
            return None
        if isinstance(stmt, If):
            self.expression(stmt.condition)
            self.statement(stmt.then_branch)
            if stmt.else_branch:
                self.statement(stmt.else_branch)
            return None
        if isinstance(stmt, Print):
            self.expression(stmt.expression)
            return None
        if isinstance(stmt, Return):
            if not self.current_function:
                self.error(stmt.keyword, "Can't return from top-level code.")
            self.expression(stmt.value)
            return None
        if isinstance(stmt, While):
            self.expression(stmt.condition)
            self.block(stmt.body)
            return None
        if isinstance(stmt, Block):
            self.block(stmt)
            return None

        return None

    def statements(self, statements:typing.List[Stmt]):
        """resolve variable use in statements"""
        for stmt in statements:
            self.statement(stmt)

    def function(self, func:Function, func_type:FunctionType):
        """resolve variable use in functions"""
        enclosing_function = self.current_function
        self.current_function = func_type
        self.begin_scope()
        for param in func.params:
            self.declare(param)
            self.define(param)
        self.statements(func.body)
        self.end_scope()
        self.current_function = enclosing_function

    def block(self, block:Block):
        """resolve block statements"""
        self.begin_scope()
        self.statements(block.statements)
        self.end_scope()


class Interpreter:
    """Interpreter"""

    class RuntimeError(RuntimeError):
        """Interpreter runtime error"""
        def __init__(self, token, msg) -> None:
            super().__init__(msg)
            self.interpreter_token = token

        def __repr__(self) -> str:
            return f"{self.__class__.__name__}({self.interpreter_token!r}, {self!s}"

    class BreakException(Exception):
        """Break iteration"""

    class ContinueException(Exception):
        """Continue iteration"""

    class ReturnException(Exception):
        """Return statement value that breaks evaluation of a function"""
        def __init__(self, value):
            super().__init__(self.__class__.__name__)
            self.value = value

    class Callable:
        """Callable"""
        def __init__(self, name, arity:int, func:typing.Callable):
            self.name = name
            self.arity = arity
            self.func = func

        def __call__(self, interpreter:'Interpreter', arguments:typing.List[typing.Any]):
            return self.func(interpreter, *(arguments or []))

        def __str__(self):
            return f"<fn {self.name}>"

    class Function(Callable):
        """Function"""
        def __init__(self, declaration:Stmt, closure:Environment):
            super().__init__(name=declaration.name.lexeme, arity=len(declaration.params), func=None)
            self.declaration = declaration
            self.closure:Environment = closure

        def __call__(self, interpreter:'Interpreter', arguments:typing.List[typing.Any]):
            environment = Environment(self.closure)
            for param, argument in zip(self.declaration.params, arguments):
                environment.define(param.lexeme, argument)
            try:
                interpreter.execute_block(self.declaration.body, environment)
            except Interpreter.ReturnException as exc:
                return exc.value
            return None

    def __init__(self) -> None:
        """init"""
        self.globals = Environment()
        self.locals = {}
        self.environment = self.globals
        self.has_errors = False

        # builtins/ffi
        self.globals.define("clock", self.Callable("clock", arity=0, func=lambda _, *args: int(time.time())))
        self.globals.define("echo", self.Callable("echo", arity=1, func=lambda _, *args: args[0]))

    def resolve(self, expr:Expr, depth:int):
        """record the depth of an expression resolution"""
        self.locals[expr] = depth

    def interpret(self, statements:typing.List[Stmt]) -> None:
        """Interpret statements"""
        self.has_errors = False
        try:
            resolver = Resolver(self)
            resolver.statements(statements=statements)
            for statement in statements:
                self.eval(statement)
        except (self.RuntimeError, Environment.RuntimeError, Resolver.RuntimeError) as exc:
            self.has_errors = True
            print(f"Interpreter error {exc.__class__.__name__}: {exc}")

    def eval(self, expression:typing.Union[Expr, Stmt]) -> typing.Any:  # noqa:C901 # too-complex
        """eval an expression or statement"""
        # pylint: disable=too-many-statements,too-many-return-statements,too-many-branches
        if isinstance(expression, Literal):
            return expression.value
        if isinstance(expression, Grouping):
            return self.eval(expression.expression)
        if isinstance(expression, Unary):
            right = self.eval(expression.right)
            if expression.operator.type == TokenType.MINUS:
                self.check_numbers(expression.operator, right)
                return -right  # pylint: disable=invalid-unary-operand-type
            if expression.operator.type == TokenType.BANG: return not right
        if isinstance(expression, Conditional):
            if self.eval(expression.expression):
                return self.eval(expression.left)
            return self.eval(expression.right)
        if isinstance(expression, Expression):
            self.eval(expression.expression)
            return None
        if isinstance(expression, Print):
            value = self.eval(expression.expression)
            if value is None:
                print("nil")
            elif isinstance(value, bool):
                print("true" if value else "false")
            else:
                print(value)
            return None
        if isinstance(expression, Variable):
            # Instead of using the environment, check locals for resolved distance to fix mutable scope changes
            # return self.environment.get(expression.name)  # passing Token
            distance = self.locals.get(expression, None)
            if distance is not None:
                return self.environment.get_at(distance, expression.name)
            return self.globals.get(expression.name)
        if isinstance(expression, Assign):
            value = self.eval(expression.value)
            # Instead of using the environment, check locals for resolved distance to fix mutable scope changes
            # self.environment.assign(expression.name, value)
            distance = self.locals.get(expression, None)
            if distance is not None:
                self.environment.assign_at(distance, expression.name, value)
            else:
                self.globals.assign(expression.name, value)
            return value
        if isinstance(expression, Logical):
            return self._eval_logical(expression)
        if isinstance(expression, Binary):
            return self._eval_binary(expression)
        if isinstance(expression, Stmt):
            return self._eval_statement(expression)
        if isinstance(expression, Call):
            return self._eval_call(expression)

        return None

    def _eval_call(self, expr:Expr) -> typing.Any:
        """evaluate a call expression"""
        function:typing.Callable = self.eval(expr.callee)

        if not isinstance(function, self.Callable):
            raise self.RuntimeError(expr.paren, "Can only call functions and classes.")

        arguments:typing.List[typing.Any] = []
        for argument in expr.arguments:
            arguments.append(self.eval(argument))

        if function.arity != len(arguments):
            raise self.RuntimeError(expr, f"Call to {function} expected {function.arity} arguments but got {len(arguments)}.")
        return function(self, arguments)

    def _eval_logical(self, expression:Expr) -> typing.Any:
        """evaluate a logical expression"""
        left = self.eval(expression.left)
        if expression.operator.type == TokenType.OR and left: return left
        if expression.operator.type == TokenType.AND and not left: return left
        return self.eval(expression.right)

    def _eval_binary(self, expression:Expr) -> typing.Any:  # noqa:C901 # too-complex
        """evaluate a binary expression"""
        # pylint: disable=too-many-statements,too-many-return-statements,too-many-branches
        right = self.eval(expression.right)
        left = self.eval(expression.left)
        if expression.operator.type == TokenType.BANG_EQUAL:
            self.check_numbers(expression.operator, left, right)
            return left != right
        if expression.operator.type == TokenType.EQUAL_EQUAL:
            self.check_numbers(expression.operator, left, right)
            return left == right
        if expression.operator.type == TokenType.GREATER:
            self.check_numbers(expression.operator, left, right)
            return left > right
        if expression.operator.type == TokenType.GREATER_EQUAL:
            self.check_numbers(expression.operator, left, right)
            return left >= right
        if expression.operator.type == TokenType.LESS:
            self.check_numbers(expression.operator, left, right)
            return left < right
        if expression.operator.type == TokenType.LESS_EQUAL:
            self.check_numbers(expression.operator, left, right)
            return left <= right
        if expression.operator.type in (TokenType.MINUS, TokenType.MINUS_MINUS, TokenType.MINUS_EQUAL):
            self.check_numbers(expression.operator, left, right)
            return left - right
        if expression.operator.type in (TokenType.PLUS, TokenType.PLUS_PLUS, TokenType.PLUS_EQUAL):
            # self.check_numbers(expression.operator, left, right)
            if not all(isinstance(_, (str, int, float)) for _ in (left, right)):
                raise self.RuntimeError(expression.operator, "Operands not supported for operator")
            # NOTE if adding and one is a string, all are strings
            if len(set((type(left), type(right)))) > 1 and any(isinstance(_, str) for _ in (left, right)):
                left = str(left)
                right = str(right)
            return left + right
        if expression.operator.type in (TokenType.SLASH, TokenType.SLASH_EQUAL):
            self.check_numbers(expression.operator, left, right)
            if right == 0:
                raise self.RuntimeError(expression.operator, "Divide by zero")
            return left // right
        if expression.operator.type in (TokenType.STAR, TokenType.STAR_EQUAL):
            self.check_numbers(expression.operator, left, right)
            return left * right

        return None

    def _eval_statement(self, stmt:Stmt) -> typing.Any:  # noqa:C901 # too-complex
        """evaluate a statement"""
        if isinstance(stmt, Var):
            value = self.eval(stmt.initializer) if stmt.initializer else None
            self.environment.define(stmt.name.lexeme, value)
            return None
        if isinstance(stmt, Block):
            self.execute_block(stmt.statements, Environment(self.environment))
            return None
        if isinstance(stmt, If):
            if self.eval(stmt.condition):
                self.eval(stmt.then_branch)
            else:
                self.eval(stmt.else_branch)
            return None
        if isinstance(stmt, Function):
            function = self.Function(stmt, self.environment)
            self.environment.define(stmt.name.lexeme, function)
            return None
        if isinstance(stmt, Break):
            raise self.BreakException
        if isinstance(stmt, Continue):
            raise self.ContinueException
        if isinstance(stmt, Return):
            value:typing.Any = None
            if stmt.value is not None:
                value = self.eval(stmt.value)
            raise self.ReturnException(value)
        if isinstance(stmt, While):
            while self.eval(stmt.condition):
                try:
                    self.eval(stmt.body)
                except self.BreakException:
                    break
                except self.ContinueException:
                    # if this is a for loop, we need to evaluate the increment expression
                    if stmt.body.statements and isinstance(stmt.body.statements[-1], ForExpression):
                        self.eval(stmt.body.statements[-1])
            return None

        return None

    def execute_block(self, statements:typing.List[Stmt], environment:Environment) -> None:
        """execute a block of statements"""
        previous:Environment = self.environment
        try:
            self.environment = environment
            for stmt in statements:
                self.eval(stmt)
        finally:
            self.environment = previous

    @classmethod
    def check_numbers(cls, token:Token, *values:typing.List[typing.Any]):
        """check if value is numeric"""
        msg = "Operand must be a number" if len(values) == 1 else "Operands must be numbers"
        if not all(isinstance(_, (float, int)) for _ in values):
            raise cls.RuntimeError(token, msg)


def tests():  # noqa:C901 # too-complex
    """testcases"""

    scanner_testcases = (
        ('var foo = "two";', [Token(TokenType.VAR, 'var', None, 1, 0), Token(TokenType.IDENTIFIER, 'foo', None, 1, 4), Token(TokenType.EQUAL, '=', None, 1, 8), Token(TokenType.STRING, '"two"', 'two', 1, 10), Token(TokenType.SEMICOLON, ';', None, 1, 15), Token(TokenType.EOF, '', None, 1, -1)]),
        ('//blah', [Token(TokenType.EOF, '', None, 1, -1)]),
        ('"foo";', [Token(TokenType.STRING, '"foo"', 'foo', 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 5), Token(TokenType.EOF, '', None, 1, -1)]),
        ('var foo = 5.2;', [Token(TokenType.VAR, 'var', None, 1, 0), Token(TokenType.IDENTIFIER, 'foo', None, 1, 4), Token(TokenType.EQUAL, '=', None, 1, 8), Token(TokenType.NUMBER, '5.2', 5.2, 1, 10), Token(TokenType.SEMICOLON, ';', None, 1, 13), Token(TokenType.EOF, '', None, 1, -1)]),
        ('true;', [Token(TokenType.TRUE, 'true', None, 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 4), Token(TokenType.EOF, '', None, 1, -1)]),
        ('false;', [Token(TokenType.FALSE, 'false', None, 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 5), Token(TokenType.EOF, '', None, 1, -1)]),
        ('nil;', [Token(TokenType.NIL, 'nil', None, 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 3), Token(TokenType.EOF, '', None, 1, -1)]),
        ('"foo" + "bar"', [Token(TokenType.STRING, '"foo"', 'foo', 1, 0), Token(TokenType.PLUS, '+', None, 1, 6), Token(TokenType.STRING, '"bar"', 'bar', 1, 8), Token(TokenType.EOF, '', None, 1, -1)]),
        ('true ? 1 : 2', [Token(TokenType.TRUE, 'true', None, 1, 0), Token(TokenType.QUESTION, '?', None, 1, 5), Token(TokenType.NUMBER, '1', 1, 1, 7), Token(TokenType.COLON, ':', None, 1, 9), Token(TokenType.NUMBER, '2', 2, 1, 11), Token(TokenType.EOF, '', None, 1, -1)]),
        ('"foo', Scanner.ScannerError),
        ('var a; if (1 == 1) { a = "yes"; } else { a = "no";} ', [Token(TokenType.VAR, 'var', None, 1, 0), Token(TokenType.IDENTIFIER, 'a', None, 1, 4), Token(TokenType.SEMICOLON, ';', None, 1, 5), Token(TokenType.IF, 'if', None, 1, 7), Token(TokenType.LEFT_PAREN, '(', None, 1, 10), Token(TokenType.NUMBER, '1', 1, 1, 11), Token(TokenType.EQUAL_EQUAL, '==', None, 1, 13), Token(TokenType.NUMBER, '1', 1, 1, 16), Token(TokenType.RIGHT_PAREN, ')', None, 1, 17), Token(TokenType.LEFT_BRACE, '{', None, 1, 19), Token(TokenType.IDENTIFIER, 'a', None, 1, 21), Token(TokenType.EQUAL, '=', None, 1, 23), Token(TokenType.STRING, '"yes"', 'yes', 1, 25), Token(TokenType.SEMICOLON, ';', None, 1, 30), Token(TokenType.RIGHT_BRACE, '}', None, 1, 32), Token(TokenType.ELSE, 'else', None, 1, 34), Token(TokenType.LEFT_BRACE, '{', None, 1, 39), Token(TokenType.IDENTIFIER, 'a', None, 1, 41), Token(TokenType.EQUAL, '=', None, 1, 43), Token(TokenType.STRING, '"no"', 'no', 1, 45), Token(TokenType.SEMICOLON, ';', None, 1, 49), Token(TokenType.RIGHT_BRACE, '}', None, 1, 50), Token(TokenType.EOF, '', None, 1, -1)]),
        ('break;', [Token(TokenType.BREAK, 'break', None, 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 5), Token(TokenType.EOF, '', None, 1, -1)]),
        ('continue;', [Token(TokenType.CONTINUE, 'continue', None, 1, 0), Token(TokenType.SEMICOLON, ';', None, 1, 8), Token(TokenType.EOF, '', None, 1, -1)]),
        ('foo(a, b);', [Token(TokenType.IDENTIFIER, 'foo', None, 1, 0), Token(TokenType.LEFT_PAREN, '(', None, 1, 3), Token(TokenType.IDENTIFIER, 'a', None, 1, 4), Token(TokenType.COMMA, ',', None, 1, 5), Token(TokenType.IDENTIFIER, 'b', None, 1, 7), Token(TokenType.RIGHT_PAREN, ')', None, 1, 8), Token(TokenType.SEMICOLON, ';', None, 1, 9), Token(TokenType.EOF, '', None, 1, -1)]),
        ('fun sum(a, b){return a + b; } var total = sum(1, 2);', [Token(TokenType.FUN, 'fun', None, 1, 0), Token(TokenType.IDENTIFIER, 'sum', None, 1, 4), Token(TokenType.LEFT_PAREN, '(', None, 1, 7), Token(TokenType.IDENTIFIER, 'a', None, 1, 8), Token(TokenType.COMMA, ',', None, 1, 9), Token(TokenType.IDENTIFIER, 'b', None, 1, 11), Token(TokenType.RIGHT_PAREN, ')', None, 1, 12), Token(TokenType.LEFT_BRACE, '{', None, 1, 13), Token(TokenType.RETURN, 'return', None, 1, 14), Token(TokenType.IDENTIFIER, 'a', None, 1, 21), Token(TokenType.PLUS, '+', None, 1, 23), Token(TokenType.IDENTIFIER, 'b', None, 1, 25), Token(TokenType.SEMICOLON, ';', None, 1, 26), Token(TokenType.RIGHT_BRACE, '}', None, 1, 28), Token(TokenType.VAR, 'var', None, 1, 30), Token(TokenType.IDENTIFIER, 'total', None, 1, 34), Token(TokenType.EQUAL, '=', None, 1, 40), Token(TokenType.IDENTIFIER, 'sum', None, 1, 42), Token(TokenType.LEFT_PAREN, '(', None, 1, 45), Token(TokenType.NUMBER, '1', 1, 1, 46), Token(TokenType.COMMA, ',', None, 1, 47), Token(TokenType.NUMBER, '2', 2, 1, 49), Token(TokenType.RIGHT_PAREN, ')', None, 1, 50), Token(TokenType.SEMICOLON, ';', None, 1, 51), Token(TokenType.EOF, '', None, 1, -1)]),
    )

    for tc_input, result in scanner_testcases:
        if result is Scanner.ScannerError:
            try:
                assert not Scanner(tc_input), "this should always fail"
            except Scanner.ScannerError:
                pass
        else:
            s = Scanner(tc_input)
            assert s.tokens == result, (tc_input, s.tokens)

    parser_expression_testcases = (
        (Scanner('1-2+3'), Binary(left=Binary(left=Literal(value=1), operator=Token(TokenType.MINUS, '-', None, 1, 1), right=Literal(value=2)), operator=Token(TokenType.PLUS, '+', None, 1, 3), right=Literal(value=3))),
        (Scanner('1<6>3'), Binary(left=Binary(left=Literal(value=1), operator=Token(TokenType.LESS, '<', None, 1, 1), right=Literal(value=6)), operator=Token(TokenType.GREATER, '>', None, 1, 3), right=Literal(value=3))),
        (Scanner('!1'), Unary(operator=Token(TokenType.BANG, '!', None, 1, 0), right=Literal(value=1))),
        (Scanner('-11000'), Unary(operator=Token(TokenType.MINUS, '-', None, 1, 0), right=Literal(value=11000))),
        (Scanner('10*100*1000'), Binary(left=Binary(left=Literal(value=10), operator=Token(TokenType.STAR, '*', None, 1, 2), right=Literal(value=100)), operator=Token(TokenType.STAR, '*', None, 1, 6), right=Literal(value=1000))),
        (Scanner('10*100/(1000*200)'), Binary(left=Binary(left=Literal(value=10), operator=Token(TokenType.STAR, '*', None, 1, 2), right=Literal(value=100)), operator=Token(TokenType.SLASH, '/', None, 1, 6), right=Grouping(expression=Binary(left=Literal(value=1000), operator=Token(TokenType.STAR, '*', None, 1, 12), right=Literal(value=200))))),
        (Scanner('true'), Literal(value=True)),
        (Scanner('false'), Literal(value=False)),
        (Scanner('nil'), Literal(value=None)),
        (Scanner('!!nil'), Unary(operator=Token(TokenType.BANG, '!', None, 1, 0), right=Unary(operator=Token(TokenType.BANG, '!', None, 1, 1), right=Literal(value=None)))),
        (Scanner('!!true'), Unary(operator=Token(TokenType.BANG, '!', None, 1, 0), right=Unary(operator=Token(TokenType.BANG, '!', None, 1, 1), right=Literal(value=True)))),
        (Scanner('"foo" + "bar"'), Binary(left=Literal(value='foo'), operator=Token(TokenType.PLUS, '+', None, 1, 6), right=Literal(value='bar'))),
        (Scanner('1 == 0'), Binary(left=Literal(value=1), operator=Token(TokenType.EQUAL_EQUAL, '==', None, 1, 2), right=Literal(value=0))),
        (Scanner('1 != 0'), Binary(left=Literal(value=1), operator=Token(TokenType.BANG_EQUAL, '!=', None, 1, 2), right=Literal(value=0))),
        (Scanner('1 != "false"'), Binary(left=Literal(value=1), operator=Token(TokenType.BANG_EQUAL, '!=', None, 1, 2), right=Literal(value='false'))),
        (Scanner('true ? 1 : 2'), Conditional(expression=Literal(value=True), left=Literal(value=1), right=Literal(value=2))),
        (Scanner('false ? 1 : 2'), Conditional(expression=Literal(value=False), left=Literal(value=1), right=Literal(value=2))),
        (Scanner('foo(a, b);'), Call(callee=Variable(name=Token(TokenType.IDENTIFIER, 'foo', None, 1, 0)), paren=Token(TokenType.RIGHT_PAREN, ')', None, 1, 8), arguments=[Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4)), Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 7))])),
        (Scanner('foo(' + ", ".join(f'variable{_}' for _ in range(260)) + ');'), Parser.ParserError),
    )
    for tc_input, result in parser_expression_testcases:
        p = Parser(tc_input)
        if result is Parser.ParserError:
            try:
                assert not p.expression(), "this should always fail"
            except Parser.ParserError:
                pass
        else:
            r = p.expression()
            assert r == result, (tc_input, r)

    parser_statement_testcases = (
        (Scanner('print "Hello, world!";'), [Print(expression=Literal(value='Hello, world!'))]),
        (Scanner('print 2*3;'), [Print(expression=Binary(left=Literal(value=2), operator=Token(TokenType.STAR, '*', None, 1, 7), right=Literal(value=3)))]),
        (Scanner('var foo;'), [Var(name=Token(TokenType.IDENTIFIER, 'foo', None, 1, 4), initializer=None)]),
        (Scanner('var foo = "bar";'), [Var(name=Token(TokenType.IDENTIFIER, 'foo', None, 1, 4), initializer=Literal(value='bar'))]),
        (Scanner('var foo = 2*3;'), [Var(name=Token(TokenType.IDENTIFIER, 'foo', None, 1, 4), initializer=Binary(left=Literal(value=2), operator=Token(TokenType.STAR, '*', None, 1, 11), right=Literal(value=3)))]),
        (Scanner('foo;'), [Expression(expression=Variable(name=Token(TokenType.IDENTIFIER, 'foo', None, 1, 0)))]),
        (Scanner('var jason = 2 > 1 ? true : false; print jason;'), [Var(name=Token(TokenType.IDENTIFIER, 'jason', None, 1, 4), initializer=Conditional(expression=Binary(left=Literal(value=2), operator=Token(TokenType.GREATER, '>', None, 1, 14), right=Literal(value=1)), left=Literal(value=True), right=Literal(value=False))), Print(expression=Variable(name=Token(TokenType.IDENTIFIER, 'jason', None, 1, 40)))]),
        (Scanner('var a; a = 2;'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=None), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 7), value=Literal(value=2)))]),
        (Scanner('var a; var b = a = 2;print "a is " + a; print "b is " + b;'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=None), Var(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 11), initializer=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 15), value=Literal(value=2))), Print(expression=Binary(left=Literal(value='a is '), operator=Token(TokenType.PLUS, '+', None, 1, 35), right=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37)))), Print(expression=Binary(left=Literal(value='b is '), operator=Token(TokenType.PLUS, '+', None, 1, 54), right=Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 56))))]),
        (Scanner('{ var a; var b = a = 2;}'), [Block(statements=[Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 6), initializer=None), Var(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 13), initializer=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 17), value=Literal(value=2)))])]),
        (Scanner('var a = 11; var greater = false; if (a > 10) greater = true; else greater = false;'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=11)), Var(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 16), initializer=Literal(value=False)), If(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37)), operator=Token(TokenType.GREATER, '>', None, 1, 39), right=Literal(value=10)), then_branch=Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 45), value=Literal(value=True))), else_branch=Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 66), value=Literal(value=False))))]),
        (Scanner('var a = 11; var greater = false; if (a > 10 and !greater) greater = true; else print "fail";'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=11)), Var(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 16), initializer=Literal(value=False)), If(condition=Logical(left=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37)), operator=Token(TokenType.GREATER, '>', None, 1, 39), right=Literal(value=10)), operator=Token(TokenType.AND, 'and', None, 1, 44), right=Unary(operator=Token(TokenType.BANG, '!', None, 1, 48), right=Variable(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 49)))), then_branch=Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'greater', None, 1, 58), value=Literal(value=True))), else_branch=Print(expression=Literal(value='fail')))]),
        (Scanner('var a = nil or "yes";'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Logical(left=Literal(value=None), operator=Token(TokenType.OR, 'or', None, 1, 12), right=Literal(value='yes')))]),
        (Scanner('var a; if (1 == 1) { a = "yes"; } else { a = "no";};'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=None), If(condition=Binary(left=Literal(value=1), operator=Token(TokenType.EQUAL_EQUAL, '==', None, 1, 13), right=Literal(value=1)), then_branch=Block(statements=[Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 21), value=Literal(value='yes')))]), else_branch=Block(statements=[Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 41), value=Literal(value='no')))]))]),
        (Scanner('var a = 10; while (a > 0) { print a; a = a - 1;}'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=10)), While(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 19)), operator=Token(TokenType.GREATER, '>', None, 1, 21), right=Literal(value=0)), body=Block(statements=[Print(expression=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 34))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 41)), operator=Token(TokenType.MINUS, '-', None, 1, 43), right=Literal(value=1))))]))]),
        (Scanner('for (var a = 1; a < 10; a = a + 1) { print a;} '), [Block(statements=[Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 9), initializer=Literal(value=1)), While(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 16)), operator=Token(TokenType.LESS, '<', None, 1, 18), right=Literal(value=10)), body=Block(statements=[Block(statements=[Print(expression=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 43)))]), ForExpression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 24), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 28)), operator=Token(TokenType.PLUS, '+', None, 1, 30), right=Literal(value=1))))]))])]),
        (Scanner('var a = 0; var temp; for (var b = 1; b <= 10000; b = temp + b) { print a; temp = a; a = b;} '),  [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=0)), Var(name=Token(TokenType.IDENTIFIER, 'temp', None, 1, 15), initializer=None), Block(statements=[Var(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 30), initializer=Literal(value=1)), While(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 37)), operator=Token(TokenType.LESS_EQUAL, '<=', None, 1, 39), right=Literal(value=10000)), body=Block(statements=[Block(statements=[Print(expression=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 71))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'temp', None, 1, 74), value=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 81)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 84), value=Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 88))))]), ForExpression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 49), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'temp', None, 1, 53)), operator=Token(TokenType.PLUS, '+', None, 1, 58), right=Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 60)))))]))])]),
        (Scanner('break;'), []),  # error condition... outside of a block so empty statements returned
        (Scanner('var a = 0; while (a < 10) { a = a + 1;  if (a == 2) { break; } }'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=0)), While(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 18)), operator=Token(TokenType.LESS, '<', None, 1, 20), right=Literal(value=10)), body=Block(statements=[Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 28), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 32)), operator=Token(TokenType.PLUS, '+', None, 1, 34), right=Literal(value=1)))), If(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 44)), operator=Token(TokenType.EQUAL_EQUAL, '==', None, 1, 46), right=Literal(value=2)), then_branch=Block(statements=[Break()]), else_branch=None)]))]),
        (Scanner('for (var a = 0; a < 10; a = a + 1) { print a; if (a == 2) { break; } }'), [Block(statements=[Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 9), initializer=Literal(value=0)), While(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 16)), operator=Token(TokenType.LESS, '<', None, 1, 18), right=Literal(value=10)), body=Block(statements=[Block(statements=[Print(expression=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 43))), If(condition=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 50)), operator=Token(TokenType.EQUAL_EQUAL, '==', None, 1, 52), right=Literal(value=2)), then_branch=Block(statements=[Break()]), else_branch=None)]), ForExpression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 24), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 28)), operator=Token(TokenType.PLUS, '+', None, 1, 30), right=Literal(value=1))))]))])]),
        (Scanner('var a = 2; a++; a += 1; a *= 2; a--; a -= 1; a /= 2;'), [Var(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 4), initializer=Literal(value=2)), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 11), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 11)), operator=Token(TokenType.PLUS_PLUS, '++', None, 1, 12), right=Literal(value=1)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 16), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 16)), operator=Token(TokenType.PLUS_EQUAL, '+=', None, 1, 18), right=Literal(value=1)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 24), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 24)), operator=Token(TokenType.STAR_EQUAL, '*=', None, 1, 26), right=Literal(value=2)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 32), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 32)), operator=Token(TokenType.MINUS_MINUS, '--', None, 1, 33), right=Literal(value=1)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 37)), operator=Token(TokenType.MINUS_EQUAL, '-=', None, 1, 39), right=Literal(value=1)))), Expression(expression=Assign(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 45), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 45)), operator=Token(TokenType.SLASH_EQUAL, '/=', None, 1, 47), right=Literal(value=2))))]),
        (Scanner('fun sum(a, b){return a + b; } var total = sum(1, 2);'), [Function(name=Token(TokenType.IDENTIFIER, 'sum', None, 1, 4), params=[Token(TokenType.IDENTIFIER, 'a', None, 1, 8), Token(TokenType.IDENTIFIER, 'b', None, 1, 11)], body=[Return(keyword=Token(TokenType.RETURN, 'return', None, 1, 14), value=Binary(left=Variable(name=Token(TokenType.IDENTIFIER, 'a', None, 1, 21)), operator=Token(TokenType.PLUS, '+', None, 1, 23), right=Variable(name=Token(TokenType.IDENTIFIER, 'b', None, 1, 25))))]), Var(name=Token(TokenType.IDENTIFIER, 'total', None, 1, 34), initializer=Call(callee=Variable(name=Token(TokenType.IDENTIFIER, 'sum', None, 1, 42)), paren=Token(TokenType.RIGHT_PAREN, ')', None, 1, 50), arguments=[Literal(value=1), Literal(value=2)]))]),
    )
    for tc_input, result in parser_statement_testcases:
        p = Parser(tc_input)
        r = p.parse()
        assert r == result, (tc_input, r)

    interpreter_eval_testcases = (
        (Parser(Scanner('1')), 1),
        (Parser(Scanner('(1)')), 1),
        (Parser(Scanner('(-1)')), -1),
        (Parser(Scanner('true')), True),
        (Parser(Scanner('!!true')), True),
        (Parser(Scanner('false')), False),
        (Parser(Scanner('nil')), None),
        (Parser(Scanner('4*2')), 8),
        (Parser(Scanner('4/2')), 2),
        (Parser(Scanner('4-2')), 2),
        (Parser(Scanner('"foo"+"bar"')), "foobar"),
        (Parser(Scanner('1<2')), True),
        (Parser(Scanner('1<=2')), True),
        (Parser(Scanner('2<=2')), True),
        (Parser(Scanner('3<=2')), False),
        (Parser(Scanner('1>2')), False),
        (Parser(Scanner('1>=2')), False),
        (Parser(Scanner('2>=2')), True),
        (Parser(Scanner('1 == 0')), False),
        (Parser(Scanner('1 != 0')), True),
        (Parser(Scanner('1 == 1')), True),
        (Parser(Scanner('"foo" + 4')), "foo4"),  # yeah not sure about this
        (Parser(Scanner('true ? 1 : 2')), 1),
        (Parser(Scanner('false ? 1 : 2')), 2),
        (Parser(Scanner('!=')), Parser.ParserError),
        (Parser(Scanner('==')), Parser.ParserError),
        (Parser(Scanner('return;')), Parser.ParserError),  # must be inside a callable
    )
    for tc_input, result in interpreter_eval_testcases:
        if result is Parser.ParserError:
            try:
                expr = tc_input.expression()
                assert not expr, "this should always fail"
            except Parser.ParserError:
                pass
        else:
            r = Interpreter().eval(tc_input.expression())
            if result is None:
                assert r is None
            else:
                assert r == result, (tc_input, r)

    interpret_testcases = (
        (Parser(Scanner('print "Hello, world!";')), lambda i: not i.has_errors),
        (Parser(Scanner('var foo = 2 * 2;')), lambda i: not i.has_errors),
        (Parser(Scanner('var foo = "bar";')), lambda i: not i.has_errors),
        (Parser(Scanner('var foo = "wehavevarassignment"; print foo;')), lambda i: not i.has_errors),
        (Parser(Scanner('print true; print nil; print false; var name = "varname"; var foo = 2; var bar = foo; print name; print bar;')), None),
        (Parser(Scanner('var jason = 2 > 1 ? true : false; print jason;')), lambda i: not i.has_errors),
        (Parser(Scanner('var jason; jason = 2 > 1 ? true : false; jason == true;')), lambda i: i.environment.values['jason'] is True),
        (Parser(Scanner('var a; var b = a = 2;print "a is " + a; print "b is " + b;')), lambda i: i.environment.values['a'] == 2 and i.environment.values['b'] == 2),
        (Parser(Scanner('var a = 10; {a = 11; var b = a;}')), lambda i: i.environment.values['a'] == 11 and 'b' not in i.environment.values),
        (Parser(Scanner('var a = 10; {var a = 11;}')), lambda i: i.environment.values['a'] == 10 and 'b' not in i.environment.values),
        (Parser(Scanner('var a = 11; var greater = false; if (a > 10) greater = true; else greater = false;')), lambda i: i.environment.values['greater']),
        (Parser(Scanner('var a = 9; var greater = false; if (a > 10) greater = true; else greater = false;')), lambda i: not i.environment.values['greater']),
        (Parser(Scanner('var a = 11; var greater = false; if (a > 10 and !greater) greater = true; else print "fail";')), lambda i: i.environment.values['greater']),
        (Parser(Scanner('var a = 11; var b = a > 10 and 1 ? "yes" : "no";')), lambda i: i.environment.values['b'] == 'yes'),
        (Parser(Scanner('var a = 11; var b = a > 10 and 0 ? "yes" : "no";')), lambda i: i.environment.values['b'] == 'no'),
        (Parser(Scanner('var a = 10; while (a > 0) { a = a - 1;}')), lambda i: i.environment.values['a'] == 0),
        (Parser(Scanner('var a = 0; var temp; for (var b = 1; b <= 10000; b = temp + b) { temp = a; a = b;}')), lambda i: i.environment.values['a'] == 6765 and i.environment.values['temp'] == 4181),
        (Parser(Scanner('var a = 0; while (a < 10) { a = a + 1;  if (a == 2) { break; } }')), lambda i: i.environment.values['a'] == 2),
        (Parser(Scanner('var b = 0; for (var a = 0; a < 10; a = a + 1) { if (a == 2) { break; } b = a;}')), lambda i: i.environment.values['b'] == 1),
        (Parser(Scanner('var b = 0; for (var a = 0; a < 3; a = a + 1) { if (a == 2) { continue; } b = b + a; }')), lambda i: i.environment.values['b'] == 1),
        (Parser(Scanner('var a = 2; a++; a += 1; a *= 2; a--; a -= 1; a /= 2;')), lambda i: i.environment.values['a'] == 3),
        (Parser(Scanner('var c = clock(1);')), lambda i: i.has_errors),
        (Parser(Scanner('var c = clock();')), lambda i: isinstance(i.environment.values['c'], int) and i.environment.values['c'] >= int(time.time())),
        (Parser(Scanner('fun sum(a, b){return a + b; } var total = sum(1, 2);')), lambda i: isinstance(i.environment.values['sum'], Interpreter.Function) and i.environment.values['total'] == 3),
        (Parser(Scanner('fun fib(n) { if (n <= 1) return n; return fib(n - 2) + fib(n - 1); } var f = fib(10);')), lambda i: i.environment.values['f'] == 55),
        (Parser(Scanner('var a = 1; var b = a();')), lambda i: i.has_errors),
        (Parser(Scanner('fun counter() { var i = 0; fun inc() { i += 1; return i;} return inc;} var c = counter(); c(); c(); c();')), lambda i: i.environment.values['c'].closure.values['i'] == 3),
    )
    for tc_input, cb in interpret_testcases:
        i = Interpreter()
        i.interpret(tc_input.parse())
        if cb is not None:
            assert cb(i), tc_input.scanner.source


if __name__ == '__main__':
    tests()
