#!/usr/bin/env python3
"""lox"""
# pylint: disable=line-too-long,too-many-arguments,multiple-statements,too-many-lines
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


class Scanner:
    """Scanner"""

    class ScannerError(RuntimeError):
        """Scanner error"""
        def __init__(self, line, offset, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.scanner_line = line
            self.scanner_offset = offset

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
        line = lines[self.line - 1]
        print(f"Error: {msg} at line {self.line}, column {self.start}")
        print(f"\t{line}")
        if fatal:
            raise self.ScannerError(line, self.start, msg)

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


class Parser:
    """
    program        → declaration* EOF ;
    declaration    → varDecl | statement ;
    varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;
    statement      → exprStmt | printStmt | block ;
    exprStmt       → expression ";" ;
    printStmt      → "print" expression ";" ;
    block          → "{" declaration* "}" ;

    expression     → assignment ;
    assignment     → IDENTIFIER "=" assignment
                   | conditional ;
    conditional    → equality ( "?" expression ":" conditional )? ;
    equality       → comparison ( ( "!=" | "==" ) comparison )* ;
    comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
    term           → factor ( ( "-" | "+" ) factor )* ;
    factor         → unary ( ( "/" | "*" ) unary )* ;
    unary          → ( "!" | "-" ) unary | primary ;
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

    def parse(self) -> typing.List[Stmt]:
        """main entry point to start the parsing"""
        statements = []
        while not self.is_at_end:
            stmt = self.declaration()
            if stmt:
                statements.append(stmt)
        return statements

    def declaration(self) -> typing.Optional[Stmt]:
        """declaration    → varDecl | statement ;"""
        try:
            if self.match(TokenType.VAR): return self.var_declaration()
            return self.statement()
        except self.ParserError as exc:
            print(exc)
            self.synchronize()
            return None

    def var_declaration(self):
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
        if self.match(TokenType.PRINT): return self.print_statement()
        if self.match(TokenType.LEFT_BRACE): return Block(self.block())  # instantiate Block here so as to reuse block() for functions etc
        return self.expression_statement()

    def block(self) -> typing.List[Stmt]:
        """block          → "{" declaration* "}" ;"""
        statements:typing.List[Stmt] = []

        while not self.check(TokenType.RIGHT_BRACE) and not self.is_at_end:
            statements.append(self.declaration())

        self.consume(TokenType.RIGHT_BRACE, "Expect '}' after block.")
        return statements

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

    def primary(self) -> typing.Optional[Expr]:
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
        # NOTE we could consider redefinition an error here
        self.values[name] = value

    def get(self, token:Token) -> typing.Any:
        """return a value by name"""
        try:
            return self.values[token.lexeme]
        except KeyError as exc:
            if self.enclosing:
                return self.enclosing.get(token)
            raise self.RuntimeError(f"Undefined variable {token.lexeme}") from exc

    def assign(self, token:Token, value:typing.Any) -> None:
        """Assign a value"""
        if token.lexeme not in self.values:
            if self.enclosing:
                self.enclosing.assign(token, value)
                return
            raise self.RuntimeError(f"Undefined variable {token.lexeme}")
        self.values[token.lexeme] = value

    def __repr__(self):
        return str(self.values)


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
        self.environment = Environment()

    def interpret(self, statements:typing.List[Stmt]):
        """Interpret statements"""
        try:
            for statement in statements:
                self.eval(statement)
        except self.RuntimeError as exc:
            print(exc)

    def eval(self, expression:typing.Union[Expr, Stmt]) -> typing.Any:
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
            elif value is True:
                print("true")
            elif value is False:
                print("false")
            else:
                print(value)
            return None
        if isinstance(expression, Variable):
            return self.environment.get(expression.name)  # passing Token
        if isinstance(expression, Assign):
            value = self.eval(expression.value)
            self.environment.assign(expression.name, value)
            return value
        if isinstance(expression, Binary):
            return self._eval_binary(expression)
        if isinstance(expression, Stmt):
            return self._eval_statement(expression)

        return None

    def _eval_binary(self, expression:Expr) -> typing.Any:
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
        if expression.operator.type == TokenType.MINUS:
            self.check_numbers(expression.operator, left, right)
            return left - right
        if expression.operator.type == TokenType.PLUS:
            # self.check_numbers(expression.operator, left, right)
            if not all(isinstance(_, (str, int, float)) for _ in (left, right)):
                raise self.RuntimeError(expression.operator, "Operands not supported for operator")
            # NOTE if adding and one is a string, all are strings
            if len(set((type(left), type(right)))) > 1 and any(isinstance(_, str) for _ in (left, right)):
                left = str(left)
                right = str(right)
            return left + right
        if expression.operator.type == TokenType.SLASH:
            self.check_numbers(expression.operator, left, right)
            if right == 0:
                raise self.RuntimeError(expression.operator, "Divide by zero")
            return left // right
        if expression.operator.type == TokenType.STAR:
            self.check_numbers(expression.operator, left, right)
            return left * right

        return None

    def _eval_statement(self, stmt:Stmt) -> typing.Any:
        """evaluate a statement"""
        if isinstance(stmt, Var):
            value = self.eval(stmt.initializer) if stmt.initializer else None
            self.environment.define(stmt.name.lexeme, value)
            return None
        if isinstance(stmt, Block):
            self.execute_block(stmt.statements, Environment(self.environment))
            return None

        return None

    def execute_block(self, statements:typing.List[Stmt], environment:Environment):
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
        ('"foo', Scanner.ScannerError),
    )

    for tc_input, result in scanner_testcases:
        if isinstance(result, type) and issubclass(result, RuntimeError):
            try:
                assert not Scanner(tc_input), "this should always fail"
            except RuntimeError:
                pass
        else:
            s = Scanner(tc_input)
            assert s.tokens == result, s.tokens

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
    )
    for tc_input, result in parser_expression_testcases:
        p = Parser(tc_input)
        r = p.expression()
        assert r == result, r

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
    )
    for tc_input, result in parser_statement_testcases:
        p = Parser(tc_input)
        r = p.parse()
        assert r == result, r

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
                assert r == result, r

    interpret_testcases = (
        (Parser(Scanner('print "Hello, world!";')), None),
        (Parser(Scanner('var foo = 2 * 2;')), None),
        (Parser(Scanner('var foo = "bar";')), None),
        (Parser(Scanner('var foo = "wehavevarassignment"; print foo;')), None),
        (Parser(Scanner('print true; print nil; print false; var name = "varname"; var foo = 2; var bar = foo; print name; print bar;')), None),
        (Parser(Scanner('var jason = 2 > 1 ? true : false; print jason;')), None),
        (Parser(Scanner('var jason; jason = 2 > 1 ? true : false; jason == true;')), lambda i: i.environment.values['jason'] is True),
        (Parser(Scanner('var a; var b = a = 2;print "a is " + a; print "b is " + b;')), lambda i: i.environment.values['a'] == 2 and i.environment.values['b'] == 2),
        (Parser(Scanner('var a = 10; {a = 11; var b = a;}')), lambda i: i.environment.values['a'] == 11 and 'b' not in i.environment.values),
        (Parser(Scanner('var a = 10; {var a = 11;}')), lambda i: i.environment.values['a'] == 10 and 'b' not in i.environment.values),
    )
    for tc_input, cb in interpret_testcases:
        i = Interpreter()
        i.interpret(tc_input.parse())
        if cb is not None:
            assert cb(i), tc_input.scanner.source


if __name__ == '__main__':
    tests()
