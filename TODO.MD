### TODOs

* ~~make todo list~~
* ~~redesign starlet.mk/common.mk/module.mk to use more of devkitarm's base_tools & base_rules~~
* ~~add nintendo syscall handling and let it call our syscall handler(swi)~~
* ~~add scheduler/threads~~
* go through everything from mini and clean up project to fit our needs
* verify nand irq code that is in comments
* implement memory manager
* implement syscalls (including stubbed mallocs)
* implement module loading
* implement building modules
* implement loading default modules on startup ( ES , FS, ...)
* implement ipc_enqueue_slow(IPC_DEV_SDHC, IPC_SDHC_DISCOVER, 0) that was left in comments
