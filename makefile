build: clean
	gcc -o main.exe main.c

debild: clean
	gcc -o main.exe main.c -DDEBUG

clean: 
	@rm -vf ./main.exe file.bin 
