all: test_lexer

test_lexer: test_lexer.cpp lexer.cpp
		g++ -g lexer.cpp test_lexer.cpp -o test_lexer

clean:
		rm -f test_lexer
