CFLAGS = -g #-Ofast
CFLAGS += -lcrypto

fsread: src/main.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $@

clean:
	rm -drf fsread
