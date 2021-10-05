all:
	$(MAKE) -C core
	$(MAKE) -C elfloader
	$(MAKE) -C kernel
	$(MAKE) -C tools/ppcloader

clean:
	$(MAKE) -C core clean
	$(MAKE) -C elfloader clean
	$(MAKE) -C kernel clean
	$(MAKE) -C tools/ppcloader clean
	
run:
	$(MAKE) -C tools/ppcloader run