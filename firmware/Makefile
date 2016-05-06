FIRMWARE  = fwbin/70015/bcm70015fw.bin \
            fwbin/70012/bcm70012fw.bin

install:
	if [ ! -d /lib/firmware ] ; \
		then install -d /lib/firmware ; \
	fi
	install -t /lib/firmware $(FIRMWARE)
