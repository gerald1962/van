\newcount\ind % current indentation in ems
\def\1{\global\advance\ind by 1 \hangindent\ind em} % indent one more notch

\def\defin#1{\global\advance\ind by 2 \1\&{#1 }} % begin `define' or `format'

\def\D{\defin{{\rm\#}define}} % macro definition

\D{x}

\noinx
