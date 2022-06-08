@ The next few sections contain stuff from the file |"lcommon.w"| that must
be included in both |"lit.w"| and |"log.w"|. It appears in
file |"lcommon.h"|, which is also included in |"lcommon.w"| to propagate
possible changes from this \.{COMMON} interface consistently.

@i lextern.h

@ \.{LIT} has a fairly straightforward outline.  It operates in
three phases: First it inputs the source file and stores cross-reference
data, then it inputs the source once again and produces the \TEX/ output
file, finally it sorts and outputs the index.
        
@c
void main(void)
{
        printf("Hi Herbert and Renate.\n");
}

@* Index.
