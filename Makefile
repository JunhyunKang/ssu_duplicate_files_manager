CC = gcc

SFINDER = ssu_sfinder


all : $(SFINDER)

$(SFINDER) : ssu_sfinder.c
	$(CC) -o $@ $^ ssu_find-sha1.c -lcrypto -lpthread


clean :
	rm -rf *.o
	rm -rf $(SFINDER)

