GCCPARAMS = -Wextra -Wall
CC = gcc
OUT = neHosServer neHosClient

target: $(OUT)

neHosServer: server.c common.c sync.c
	$(CC) $(GCCPARAMS) $^ -o $@ 

neHosClient: client.c common.c
	$(CC) $(GCCPARAMS) $^ -o $@ 

clean:
	rm $(OUT) *.log