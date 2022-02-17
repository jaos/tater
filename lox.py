#!/usr/bin/env python3
"""lox"""
# pylint: disable=line-too-long,too-many-arguments,multiple-statements
import enum
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
    PLUS = enum.auto()
    SEMICOLON = enum.auto()
    SLASH = enum.auto()
    STAR = enum.auto()
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
    CLASS = enum.auto()
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
    'class':   TokenType.CLASS,
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
    def __init__(self, token_type, lexeme, literal, line, column):
        self.type = token_type
        self.lexeme = lexeme
        self.literal = literal
        self.line = line
        self.column = column

    def __str__(self):
        return f"{self.type} {self.lexeme} {self.literal}"

    def __repr__(self):
        return f"{self.__class__.__name__}({self.type}, {self.lexeme!r}, {self.literal!r}, {self.line!r}, {self.column!r})"

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        return (
            self.type == other.type
            and self.lexeme == other.lexeme
            and self.literal == other.literal
            and self.line == other.line
            and self.column == other.column
        )


class ScannerError(RuntimeError):
    """Scanner error"""
    def __init__(self, line, offset, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.scanner_line = line
        self.scanner_offset = offset


class Scanner:
    """Scanner"""
    def __init__(self, source):
        self.source = source
        self.tokens = []
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

    def advance(self):
        """advance to the next character"""
        c = self.source[self.current]
        self.current += 1
        return c

    def match(self, expected):
        """match current character"""
        if self.is_at_end: return False
        if self.source[self.current] != expected: return False
        self.current += 1
        return True

    def scan_error(self, msg, fatal=False):
        """scanner error handling"""
        lines = self.source.split('\n')
        line = lines[self.line]
        print(f"Error: {msg} at line {self.line}, column {self.start}")
        print(f"\t{line}")
        if fatal:
            raise ScannerError(line, self.start, msg)

    def string(self):
        """scan a string"""
        while self.peek != '"' and not self.is_at_end:
            if self.peek == '\n': self.line += 1
            self.advance()

        if self.is_at_end:
            self.scan_error("Unterminated string", True)

        self.advance()

        value = self.source[self.start + 1: self.current - 1]
        self.add_token(TokenType.STRING, value)

    def number(self):
        """scan a number"""
        while self.peek.isdigit(): self.advance()
        if self.peek == '.' and self.peek_next.isdigit(): self.advance()
        while self.peek.isdigit(): self.advance()
        value = self.source[self.start:self.current]
        value = float(value) if '.' in value else int(value)
        self.add_token(TokenType.NUMBER, value)

    def identifier(self):
        """scan an identifier"""
        while self.peek.isidentifier(): self.advance()
        value = self.source[self.start:self.current]
        if value in KEYWORDS:
            self.add_token(KEYWORDS[value])
        else:
            self.add_token(TokenType.IDENTIFIER)

    def add_token(self, token_type:TokenType, literal=None):
        """add a scanned token"""
        text = self.source[self.start:self.current]
        self.tokens.append(Token(token_type, text, literal, self.line, self.start))

    def scan_token(self):
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
        elif c == '-': self.add_token(TokenType.MINUS)
        elif c == '+': self.add_token(TokenType.PLUS)
        elif c == ';': self.add_token(TokenType.SEMICOLON)
        elif c == '*': self.add_token(TokenType.STAR)
        elif c == '!': self.add_token(TokenType.BANG_EQUAL if self.match('=') else TokenType.BANG)
        elif c == '=': self.add_token(TokenType.EQUAL_EQUAL if self.match('=') else TokenType.EQUAL)
        elif c == '<': self.add_token(TokenType.LESS_EQUAL if self.match('=') else TokenType.LESS)
        elif c == '>': self.add_token(TokenType.GREATER_EQUAL if self.match('=') else TokenType.GREATER)
        elif c == '/':
            if self.match('/'):
                while self.peek != '\n' and not self.is_at_end: self.advance()
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


class ParserError(RuntimeError):
    """Parser error"""


class Parser:
    """
    expression     → conditional ;
    conditional    → equality ( "?" expression ":" conditional )? ;
    equality       → comparison ( ( "!=" | "==" ) comparison )* ;
    comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
    term           → factor ( ( "-" | "+" ) factor )* ;
    factor         → unary ( ( "/" | "*" ) unary )* ;
    unary          → ( "!" | "-" ) unary
                   | primary ;
    primary        → NUMBER | STRING | "true" | "false" | "nil"
                   | "(" expression ")" ;

                   // Error productions...
                   | ( "!=" | "==" ) equality
                   | ( ">" | ">=" | "<" | "<=" ) comparison
                   | ( "+" ) term
                   | ( "/" | "*" ) factor ;
    """
    previous = property(lambda self: self.tokens[self.current - 1], doc="return the previous token")
    peek = property(lambda self: self.tokens[self.current], doc="return the current token")
    is_at_end = property(lambda self: self.peek.type == TokenType.EOF, doc="check if we are at the EOF token")

    def __init__(self, scanner:Scanner):
        self.scanner = scanner
        self.tokens = scanner.tokens
        self.current = 0

    def parse(self):
        """main entry point to start the parsing"""
        try:
            return self.expression()
        except ParserError as exc:
            print(exc)
        return None

    def error(self, msg:str) -> None:
        """Error handling"""
        token = self.peek
        if token.type == TokenType.EOF:
            raise ParserError(f"Parser error on line {token.line}, unexpected EOF: {msg}")
        raise ParserError(f"Parser error on line {token.line} at {token.lexeme}: {msg}")

    def advance(self):
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
        return self.conditional()

    def conditional(self) -> Expr:
        """conditional    → equality ( "?" expression ":" conditional )? ; """
        expr:Expr = self.equality()
        if self.match(TokenType.QUESTION):
            then_branch:Expr = self.expression()
            self.consume(TokenType.COLON, "Expect ':' after then branch of conditional expression.")
            else_branch:Expr = self.conditional()
            expr = Conditional(expr, then_branch, else_branch)

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
        """term           → factor ( ( "-" | "+" ) factor )* ;"""
        expr:Expr = self.factor()

        while self.match(TokenType.MINUS, TokenType.PLUS):  # precedence order
            operator:Token = self.previous
            right:Expr = self.factor()
            expr = Binary(expr, operator, right)

        return expr

    def factor(self) -> Expr:
        """factor         → unary ( ( "/" | "*" ) unary )* ;"""
        expr:Expr = self.unary()

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
        return self.primary()

    def primary(self) -> Expr:
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

        # handle error productions here
        if self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            self.error("Missing left-hand operand.")
            self.equality()
            return None

        if self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            self.error("Missing left-hand operand.")
            self.comparison()
            return None

        if self.match(TokenType.PLUS):
            self.error("Missing left-hand operand.")
            self.term()
            return None

        if self.match(TokenType.SLASH, TokenType.STAR):
            self.error("Missing left-hand operand.")
            self.factor()
            return None

        return self.error("Expression expected")

    def consume(self, token_type:TokenType, msg:str):
        """check for next expected token, raise error msg if not"""
        if self.check(token_type): return self.advance()
        return self.error(msg)

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


class Interpreter:
    """Interpreter"""

    class RuntimeError(RuntimeError):
        """Interpreter runtime error"""
        def __init__(self, token, msg):
            super().__init__(msg)
            self.interpreter_token = token

        def __repr__(self):
            return f"{self.__class__.__name__}({self.interpreter_token!r}, {self!s}"

    def __init__(self):
        """init"""

    def evaluate(self, expression:typing.Union[Expr, Stmt]):
        """Evaluate an expression"""
        self.eval(expression)

    @classmethod
    def eval(cls, expression:typing.Union[Expr, Stmt]):
        """eval an expression or statement"""
        # pylint: disable=too-many-statements,too-many-return-statements
        if isinstance(expression, Literal):
            return expression.value
        if isinstance(expression, Grouping):
            return cls.eval(expression.expression)
        if isinstance(expression, Unary):
            right = cls.eval(expression.right)
            if expression.operator.type == TokenType.MINUS:
                cls.check_numbers(expression.operator, right)
                return -right
            if expression.operator.type == TokenType.BANG: return not right
        if isinstance(expression, Binary):
            right = cls.eval(expression.right)
            left = cls.eval(expression.left)
            if expression.operator.type == TokenType.BANG_EQUAL:
                cls.check_numbers(expression.operator, left, right)
                return left != right
            if expression.operator.type == TokenType.EQUAL_EQUAL:
                cls.check_numbers(expression.operator, left, right)
                return left == right
            if expression.operator.type == TokenType.GREATER:
                cls.check_numbers(expression.operator, left, right)
                return left > right
            if expression.operator.type == TokenType.GREATER_EQUAL:
                cls.check_numbers(expression.operator, left, right)
                return left >= right
            if expression.operator.type == TokenType.LESS:
                cls.check_numbers(expression.operator, left, right)
                return left < right
            if expression.operator.type == TokenType.LESS_EQUAL:
                cls.check_numbers(expression.operator, left, right)
                return left <= right
            if expression.operator.type == TokenType.MINUS:
                cls.check_numbers(expression.operator, left, right)
                return left - right
            if expression.operator.type == TokenType.PLUS:
                # cls.check_numbers(expression.operator, left, right)
                if not all(isinstance(_, (str, int, float)) for _ in (left, right)):
                    raise cls.RuntimeError(expression.operator, "Operands not supported for operator")
                # NOTE if adding and one is a string, all are strings
                if len(set((type(left), type(right)))) > 1 and any(isinstance(_, str) for _ in (left, right)):
                    left = str(left)
                    right = str(right)
                return left + right
            if expression.operator.type == TokenType.SLASH:
                cls.check_numbers(expression.operator, left, right)
                if right == 0:
                    raise cls.RuntimeError(expression.operator, "Divide by zero")
                return left // right
            if expression.operator.type == TokenType.STAR:
                cls.check_numbers(expression.operator, left, right)
                return left * right
        if isinstance(expression, Conditional):
            if cls.eval(expression.expression):
                return cls.eval(expression.left)
            return cls.eval(expression.right)

        return None

    @classmethod
    def check_numbers(cls, token:Token, *values:typing.List[typing.Any]):
        """check if value is numeric"""
        msg = "Operand must be a number" if len(values) == 1 else "Operands must be numbers"
        if not all(isinstance(_, (float, int)) for _ in values):
            raise cls.RuntimeError(token, msg)


def tests():
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
    )

    for tc_input, result in scanner_testcases:
        s = Scanner(tc_input)
        assert s.tokens == result, s.tokens

    parser_testcases = (
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
    )
    for tc_input, result in parser_testcases:
        p = Parser(tc_input)
        r = p.parse()
        assert r == result, r

    interpreter_testcases = (
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
        # (Parser(Scanner('!=')), None),
        # (Parser(Scanner('==')), None),
    )
    for tc_input, result in interpreter_testcases:
        r = Interpreter.eval(tc_input.parse())
        if result is None:
            assert r is None
        else:
            assert r == result, r

    # print("Success")


if __name__ == '__main__':
    tests()
