all: sofun

sofun: sofun.c
	cc -g -o $@ $< -DDEBUG -lffi

clean:
	rm -f ./sofun
