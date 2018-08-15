.PHONY:	clean	all

all:test

test:	threadpool.o	main.o	condition.o
	gcc	$^	-o	$@	-lpthread	-lrt

%.o:%.c
	gcc	-c	$^	-o	$@

clean:
	rm	-f	*.o	test
