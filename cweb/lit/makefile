# To prepare to use make, you must write a file called the makefile that
# describes the relationships among files in your program and provides commands
# for updating each file.
#
# Makefiles contain five kinds of things: explicit rules, implicit rules,
# variable definitions, directives, and comments.
#
# An implicit rule says when and how to remake a class of files based on their
# names. It describes how a target may depend on a file with a name similar to
# the target and gives a recipe to create or update such a target.
#

# %.tex is a target that matches any single file ending in .tex.
# "%.tex: %.w" means that any file ending in .tex depends on the same filename
# ending in .w to be present. The following line, starting with a tab, is the
# rule to use whenever you are generating a file of the form %.tex.

# Here is a table of automatic variables:
#
# $* - The stem with which an implicit rule matches. The target %.tex corresponds
# to the prerequisites of the explicit rule melem. then the stem is lextern.tex
# and lelem.tex The stem is useful for constructing names of related files.
#
%.tex: %.w
	cweave $*

%.c: %.w
	ctangle $*

# $@ - The file name of the target of the rule.
#
# $< - The name of the first prerequisite. If the target got its recipe from an
# implicitrule, this will be the first prerequisite added by the implicit rule.
# 
lit: lit.c lcommon.c
	gcc -g -o $@ $<

lelem: lelem.c lextern.c
	gcc -g -o $@ $<

# A simple makefile consists of “rules” with the following shape:
# target ... : prerequisites ...
#	recipe
#	...
#
# An explicit rule says when and how to remake one or more files, called the
# rule’s targets. It lists the other files that the targets depend on, called
# the prerequisites of the target, and may also give a recipe to use to create
# or update the targets.
#
mlit: lcommon.tex lit.tex
	pdftex lcommon.tex
	pdftex lit.tex

melem: lextern.tex lelem.tex
	pdftex lextern.tex
	pdftex lelem.tex

f: func.tex
	pdftex func.tex

# This rule containing the delete command associated with
# the target ‘clean’ does not have prerequisites.
#
clean:
	rm -f \
	func.tex \
	lit lcommon.tex lit.tex \
	lelem lextern.tex lelem.tex \
	*~ *.c *.idx *.log *.o *.scn *.toc
