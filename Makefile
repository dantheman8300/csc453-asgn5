

minls: minls.c minutil.h
	gcc -o minls minls.c

# TEMP IN ORDER TO MAKE TEST HARNESS RUN
minget: minget.c minutil.h
	gcc -o minget minget.c

all: minls minget
	@echo done

clean: 
	rm minls minget
