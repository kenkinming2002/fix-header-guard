.PHONY: clean

all: fix-header-guard

fix-header-guard: fix-header-guard.o
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $<

clean:
	- rm -f fix-header-guard.o
	- rm -f fix-header-guard

