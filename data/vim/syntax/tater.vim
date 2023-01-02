" Vim syntax file
" https://vimhelp.org/syntax.txt.html
" :help group-name
" Language: tater
" Maintainer: Jason Woodward
" Latest Revision: 2023-01-01

if exists("b:current_syntax")
  finish
endif

" keywords
syn keyword totLanguageKeywords     and assert break case type continue default else
syn keyword totLanguageKeywords     exit false for fn if nil or print return super
syn keyword totLanguageKeywords     switch true while error init

syn keyword totConditional          else if switch and or assert
syn keyword totRepeat               for while
syn keyword totBool                 true false
syn keyword totOperator             is

syn keyword totType                 type
syn keyword totType                 self
syn keyword totDeclVar              let nextgroup=totVarName skipwhite
syn keyword totStatement            print for while if return assert exit switch break continue
syn keyword totTypeStatement        type nextgroup=totTypeName skipwhite
syn keyword totFnStatement          fn nextgroup=totFunctionName skipwhite

syn match totNumberLiteral          '\d\+'
syn match totNumberLiteral          '[-+]\d\+'
syn match totNumberLiteral          '[-+]\d\+\.\d*'

syn match totCommentExpression      "#.*$"
syn match totCommentExpression      "//.*$"
syn match totParam                  '\h\w*' contained skipwhite
syn match totShebang                "\%^#!.*" skipwhite

syn match totVarName                "\h\w*" contained skipwhite skipempty
syn match totTypeName               "\h\w*" contained nextgroup=totParamList nextgroup=totBlock
syn match totFunctionName           "\h\w*" display contained nextgroup=totParamList nextgroup=totBlock
syn match totSpaceError             display excludenl "\s\+$"

syn region totContainer             matchgroup=Delimiter start=/\[/ end=/\]/ contained contains=totTypeName contains=totVarName
syn region totContainer             matchgroup=Delimiter start="{" end="}" contained contains=totTypeName contains=totVarName

syn region totString                start='"' end='"' oneline
syn region totBlock                 start="{" end="}" fold transparent skipempty
syn region totParamList             start="(" end=")" contained contains=totParam skipwhite nextgroup=totBlock skipempty

hi def link totBool                 Boolean
hi def link totCommentExpression    Comment
hi def link totShebang              Comment
hi def link totConditional          Conditional
hi def link totNumberLiteral        Constant
hi def link totParam                Identifier
hi def link totSpaceError           Error
hi def link totFnStatement          Function
hi def link totFunctionName         Function
hi def link totLanguageKeywords     Statement
hi def link totNumberLiteral        Number
hi def link totOperator             Operator
hi def link totRepeat               Repeat
hi def link totStatement            Statement
hi def link totType                 StorageClass
hi def link totTypeName             StorageClass
hi def link totTypeStatement        StorageClass
hi def link totString               String
hi def link totDeclVar              Type
hi def link totVarName              Type
