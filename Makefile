qemu-net: qemu-net.c
	@gcc $(<) -o $(@) -W -Wall -Wno-unused-parameter -pedantic
	@sudo chown root $(@)
	@sudo chmod u+s $(@)

clean: qemu-net
	@rm -f qemu-net

test:
	./qemu-net delete tap0
	./qemu-net create tap0 $(shell whoami) $(shell groups | cut -f 1 -d ' ')
