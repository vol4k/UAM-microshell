
microshell : src/microshell.o
	rm -rf build
	mkdir -p build
	gcc src/microshell.c -o build/microshell -lreadline
	rm src/microshell.o
	build/microshell
