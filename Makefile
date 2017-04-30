test_prg: test_prg.cpp libmap.a goldchase.h serverdaemon.cpp
	g++ -g test_prg.cpp serverdaemon.cpp -o test_prg -L. -lmap -lpanel -lncurses -lrt -lpthread

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -g -c Screen.cpp

Map.o: Map.cpp
	g++ -g -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a test_prg /dev/shm/sem.mySem /dev/shm/TAG_mymap
