GXX=g++

simplefs: shell.o fs.o disk.o
	$(GXX) shell.o fs.o disk.o -o simplefs

shell.o: shell.cc
	$(GXX) -Wall shell.cc -c -o shell.o -g

fs.o: fs.cc fs.h
	$(GXX) -Wall fs.cc -c -o fs.o -g

disk.o: disk.cc disk.h
	$(GXX) -Wall disk.cc -c -o disk.o -g

clean:
	rm -f simplefs disk.o fs.o shell.o

valgrind: simplefs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./simplefs image.20 20