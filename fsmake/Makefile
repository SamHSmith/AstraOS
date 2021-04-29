CFLAGS = -g #-Ofast
CFLAGS += -lcrypto

fsmake: src/main.c
	$(CC) $(CFLAGS) $? $(LDFLAGS) -o $@

clean:
	rm -drf fsmake
