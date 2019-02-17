# mk1.mk
# comment
var := value  # comment on same line (not highlighted right!)
-include somefile
# comment that \
  spans two lines
target: prereq
	shellcmd1
	shellcmd2
	shellcmd that \
	  spans multiple lines
# makefile comment
	# shell comment

