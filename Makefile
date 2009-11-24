LIB =
ifdef MARKDOWN
LIB += -Ldiscount -lmarkdown
FLAGS += -DMARKDOWN
endif

all:
	cc -Os -Wall $(LIB) $(FLAGS) hashtable.c markup.c main.c -o ick

clean:
	rm ick