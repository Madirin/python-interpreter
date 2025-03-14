CXX=g++
CXXFLAGS=-g -std=c++20
all: test_lexer

test_lexer: test_lexer.cpp lexer.cpp
		$(CXX) $(CXXFLAGS) lexer.cpp test_lexer.cpp -o test_lexer

clean:
		rm -f test_lexer
