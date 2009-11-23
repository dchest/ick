all:
	cc -Os -Wall main.c hashtable.c -o ick

clean:
	rm ick