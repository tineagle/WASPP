all: test

test: main.o
	clang++ --std=c++11 -lglfw -lvulkan main.o -o test

main.o: main.cpp
	clang++ -c --std=c++11 main.cpp

clean:
	rm -f main.o test
