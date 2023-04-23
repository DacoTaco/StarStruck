all:
	$(MAKE) -C core
	$(MAKE) -C elfloader
	$(foreach dir, $(wildcard ./modules/*/), @$(MAKE) -C $(dir);)
	$(MAKE) -C kernel
	$(MAKE) -C tools/ppcloader

clean:
	$(MAKE) -C core clean
	$(MAKE) -C elfloader clean
	$(foreach dir, $(wildcard ./modules/*/), @$(MAKE) -C $(dir) clean;)
	$(MAKE) -C kernel clean
	$(MAKE) -C tools/ppcloader clean
	
run: all
	$(MAKE) -C tools/ppcloader run