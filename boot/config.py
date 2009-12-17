import os
PATHS = {"hypervisor": "~/boot/nova/hypervisor",
         "bootdir":    os.path.expanduser("~/boot/"),
	}
PROGS = {"fwread" : "fw_peek %(node)d %(address)#8x %(count)#8x"}

