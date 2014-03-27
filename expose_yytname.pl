#!/usr/bin/env perl

# Exporse yytname

while (<>) {
    s/static const char \*const yytname/const char \*const yytname/o;
    print;
}


