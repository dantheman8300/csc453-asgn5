
all: minls minget
	@echo done

minls: minls.c minutil.h minutil.c
	gcc -o minls minls.c minutil.c

minget: minget.c minutil.h minutil.c
	gcc -o minget minget.c minutil.c

clean: 
	rm minls minget
