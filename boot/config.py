import os
PATHS = {"hypervisor": "~/boot/nul/hypervisor",
         "bootdir":    os.path.expanduser("~/boot/"),
	}
PROGS = {"fwread" : "fw_peek %(node)d %(address)#8x %(count)#8x"}

