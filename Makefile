all: sofun

sofun: sofun.c
	cc -g -o $@ $< -lffi

debug: sofun.c
	cc -g -o ./sofun $< -DDEBUG -lffi

clean:
	rm -f ./sofun
