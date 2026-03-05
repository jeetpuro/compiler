compiler: lex.yy.c parser.tab.o main.cc
		g++ -g -w -ocompiler parser.tab.o lex.yy.c main.cc -std=c++14
parser.tab.o: parser.tab.cc
		g++ -g -w -c parser.tab.cc -std=c++14
parser.tab.cc: parser.yy
		bison parser.yy
lex.yy.c: lexer.flex parser.tab.cc
		flex lexer.flex
tree: 
		dot -Tpdf tree.dot -otree.pdf
tree_semantic:
		dot -Tpdf tree_semantic.dot -otree_semantic.pdf
clean:
		rm -f parser.tab.* lex.yy.c* compiler stack.hh position.hh location.hh tree.dot tree.pdf tree_semantic.dot tree_semantic.pdf
		rm -R compiler.dSYM

test_syntax: compiler
	@echo "Running all syntax error tests..."
	@echo "=================================="
	@for file in syntax_errors/*.cpm; do \
		echo ""; \
		echo ">> Testing: $$file"; \
		echo "----------------------------------"; \
		./compiler $$file; \
		echo "----------------------------------"; \
	done
	@echo ""
	@echo "=================================="
	@echo "All syntax error tests completed!"

test_semantic: compiler
	@echo "Running all semantic error tests..."
	@echo "=================================="
	@for file in semantic_errors/*.cpm; do \
		echo ""; \
		echo ">> Testing: $$file"; \
		echo "----------------------------------"; \
		./compiler $$file; \
		echo "----------------------------------"; \
	done
	@echo ""
	@echo "=================================="
	@echo "All semantic error tests completed!"