CFLAGS = -Wall -Wextra -Werror
PROGS = client server

all: $(PROGS)

clean:
	$(RM) $(PROGS)

.PHONY: all clean
