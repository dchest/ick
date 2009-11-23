all:
	cc -Os -Wall main.c hashtable.c markup.c -o ick

clean:
	rm ick