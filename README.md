# Little lisp - or Louis's lisp. A simple interpreter.

This is an experiment in writing a little scheme-like lisp. It implements some runtime features like tail call elimination, (non-hygienic) macros, and first-class continuations, but is lacking many other basic language features. It is mostly intended as a way for me to try out writing a simple garbage collector and continuation-passing style from scratch in C without depending on built-in features of higher-level languages.

An example of some actual llisp code is given in `stdlib.llisp` where the (extremely minimal) standard library is implemented. The language constructs implemented in C can be found in `globals.c`.

## Security

Although execution won't cause a stack overflow, there is no such guarantee for the parser. It is implemented as a simple recursive descent parser, and _will_ overflow the stack on input that's nested deeply enough. You shouldn't try to use this language for production-quality software since it's slow and idiosyncratic and missing a lot of features, but it's _definitely_ a bad idea to even parse untrusted user input.
