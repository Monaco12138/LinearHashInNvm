exce.o : main.cpp pml_hash_new.cpp pml_hash.h
	g++ -Wall main.cpp pml_hash_new.cpp -o exce.o -lpmem

.PHONY : clean
	
clean:
	rm exce.o
