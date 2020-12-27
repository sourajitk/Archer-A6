


# our make tool
#
SUBMAKE:=$(MAKE)

PRODUCT_NAME:=c6u

#
# path name of common dirs 
#
BUILD_DIR:=$(PWD)
PLATFORM_DIR:=$(BUILD_DIR)/../openwrt
IMAGE_DIR:=$(BUILD_DIR)/../image/$(PRODUCT_NAME)

#
# basic Vars for special product
#
include $(BUILD_DIR)/basic.config

export CONFIG_PRODUCT_NAME=$(PRODUCT_NAME)

platform:




platform: iplatform_prep sdk iplatform uboot

toolchain: 
	@if [ ! -d $(TOOLCHAIN_ROOT_DIR) ]; then \
		echo Uncompressing toolchain, please wait... && \
		tar -jxf $(TOOLCHAIN_DIR)/$(TOOLCHAIN_PACKAGE) -C $(TOOLCHAIN_DIR) ; \
		mkdir -p $(PLATFORM_DIR)/staging_dir/target-mipsel-openwrt-linux-uclibc-$(PRODUCT_NAME)/usr/include; \
		echo "#define log(proj_id, msg_id, ...)" > $(PLATFORM_DIR)/staging_dir/target-mipsel-openwrt-linux-uclibc-$(PRODUCT_NAME)/usr/include/log.h; \
		echo "#define LOG(msg_id, ...)" >> $(PLATFORM_DIR)/staging_dir/target-mipsel-openwrt-linux-uclibc-$(PRODUCT_NAME)/usr/include/log.h; \
		cp $(BUILD_DIR)/ibus.h $(BUILD_DIR)/ibus_type.h $(BUILD_DIR)/ubus_json.h $(PLATFORM_DIR)/staging_dir/target-mipsel-openwrt-linux-uclibc-$(PRODUCT_NAME)/usr/include/; \
	 fi
	@echo make $@ done.

toolchain_relocate:
	@if test -d $(TOOLCHAIN_ROOT_DIR); \
	then \
		cp $(TOOLCHAIN_DIR)/libstdc++.a $(TOOLCHAIN_ROOT_DIR)/usr/mipsel-buildroot-linux-uclibc/lib/; \
		cp $(TOOLCHAIN_DIR)/libstdc++.so $(TOOLCHAIN_ROOT_DIR)/usr/mipsel-buildroot-linux-uclibc/lib/; \
		echo "relocate toolchain directory path..."; \
		$(TOOLCHAIN_DIR)/relocate_toolchain_library.sh $(TOOLCHAIN_ROOT_DIR) /opt/buildroot-gcc463; \
	fi; 

toolchain_mtk_uboot:
	@if [ ! -d $(UBOOT_TOOLCHAIN_ROOT_DIR) ]; then \
		echo Uncompressing mtk Uboot toolchain, please wait... && \
		tar -jxf $(TOOLCHAIN_DIR)/$(UBOOT_TOOLCHAIN_PACKAGE) -C $(TOOLCHAIN_DIR) ; \
	 fi
	@echo make $@ done.


iplatform.config:
	@cp -f $(BUILD_DIR)/iplatform.config $(PLATFORM_DIR)/.config

sdk.config:
	@cp -f $(BUILD_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config

iplatform_prep:  toolchain toolchain_relocate toolchain_mtk_uboot iplatform.config iplatform_package/symlinks sdk.config


kernel_menuconfig:
	@cp -f  $(BUILD_DIR)/kernel.config  $(SDK_KERNEL_DIR)/.config
	cd $(SDK_KERNEL_DIR)/; $(MAKE) menuconfig
	@cp -f $(SDK_KERNEL_DIR)/.config $(BUILD_DIR)/kernel.config

kernel_build:
	@cp -f $(BUILD_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config
	echo "original path is $(PATH)"
	cd $(SDK_KERNEL_DIR) && $(MAKE) dep	
	cd $(SDK_KERNEL_DIR) && $(MAKE) V=2 linux.7z SUPPLIER_TOOLS=$(SDK_ROOT_DIR)/host/toolchain

kernel_modules:
	[ ! -d "$(SDK_KERNEL_DIR)/modules" ] || rm -rf $(SDK_KERNEL_DIR)/modules
	mkdir -p $(SDK_KERNEL_DIR)/modules
	@cp -f $(BUILD_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config
	cd $(SDK_KERNEL_DIR) && $(MAKE) modules
	cd $(SDK_KERNEL_DIR) && $(MAKE) DEPMOD=true modules_install INSTALL_MOD_PATH=$(SDK_KERNEL_DIR)/modules

sdk: kernel_build kernel_modules

fs-uboot:uboot_clean
	@cp -f $(BUILD_DIR)/uboot.config $(UBOOT_BUILD_DIR)/.config

	@echo mainuboot: making ...
	cd $(UBOOT_BUILD_DIR) && make silentconfig CONFIG_CROSS_COMPILER_PATH=$(UBOOT_TOOLCHAIN_COMPILER_PATH)
	cd $(UBOOT_BUILD_DIR) && make CONFIG_CROSS_COMPILER_PATH=$(UBOOT_TOOLCHAIN_COMPILER_PATH)
	@cp $(UBOOT_BUILD_DIR)/uboot.bin $(IMAGE_DIR)/uboot.bin



uboot:fs-uboot
	@echo uboot: make.. done


uboot_clean:
	@echo uboot: clean ...
	$(MAKE) -C $(UBOOT_BUILD_DIR) clean;

iplatform: iplatform_world

%::
	$(if $(filter iplatform_%,$@),\
		@$(SUBMAKE) -C $(PLATFORM_DIR) $(patsubst iplatform_%, %, $@),\
		@$(SUBMAKE) -C $(SDK_BUILD_DIR) $@ $(SDK_FLAGS))

.PHONY: toolchain iplatform_prep iplatform platform firmware sdk \
	iplatform.config sdk.config kernel.config hosttools $(EXT_PHONY)
