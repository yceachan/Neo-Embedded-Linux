KERN_DIR ?= $(HOME)/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88
DTS_DIR   = arch/arm/boot/dts

# Default: build all registered dtbs
dtbs:
	$(MAKE) -C $(KERN_DIR) dtbs
	$(MAKE) sync
# Build a single dtb:  make 100ask_imx6ull-14x14.dtb
%.dtb: FORCE
	$(MAKE) -C $(KERN_DIR) $@

# Copy built dtb to NFS nfs for EVB:  make deploy-100ask_imx6ull-14x14
deploy : sync
	cp $(DTBS_DIR)/*.dtb $(HOME)/imx/prj/nfs/dtbs
	@echo "=> $*.dtb deployed to prj/nfs/"

DTBS_DIR  = dtbs
EVB_DTBS_DIR = $(HOME)/imx/prj/nfs/dtbs
# Sync (copy) all built dtbs into prj/dts/dtbs/
sync:
	@mkdir -p $(DTBS_DIR)
	cp $(KERN_DIR)/$(DTS_DIR)/100ask_imx6ull-14x14.dtb $(DTBS_DIR)/
	@echo "=> synced dtbs to $(DTBS_DIR)/"

clean:
	rm -rf $(DTBS_DIR)
	rm -rf $(EVB_DTBS_DIR)
	@echo "clean local dtbs in $(DTBS_DIR)/"

FORCE:
.PHONY: dtbs sync clean FORCE
