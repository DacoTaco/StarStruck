all:
	$(MAKE) -C IosCore
	$(MAKE) -C kernel
	$(MAKE) -C tools/ppcloader

clean:
	$(MAKE) -C IosCore clean
	$(MAKE) -C kernel clean
	$(MAKE) -C tools/ppcloader clean
	
run:
	$(MAKE) -C tools/ppcloader run