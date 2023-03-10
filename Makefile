minls: minls.c minutil.h
	gcc -o minls minls.c

# TEMP IN ORDER TO MAKE TEST HARNESS RUN
minget: minls.c minutil.h
	gcc -o minget minls.c

all: minls minget
	@echo done
