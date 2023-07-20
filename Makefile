#Makefile
all: mydbg

mygdb: mydbg.c silly_chall1
	gcc mydbg.c -o mydbg

clean:
	rm mydbg
