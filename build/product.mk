#
# Basic targets for special product
#

LUAC?=$(PLATFORM_DIR)/staging_dir/host/bin/luac
LUA_PATH?=$(IMAGE_ROOTFS_DIR)/usr/lib/lua

ifeq ($(REGION), EU)
     TMP_PRODUCT_NAME=c6u_eu
else ifeq ($(REGION), RU)
     TMP_PRODUCT_NAME=c6u_ru
else ifeq ($(REGION), US)
     TMP_PRODUCT_NAME=c6u_us
else ifeq ($(REGION), KR)
     TMP_PRODUCT_NAME=c6_kr
else
    $(error "REGION setting error!")
endif


ifneq ($(strip $(CLOUD_SW_VER)),)
  SOFTWARE_VERSION:="V$(CLOUD_SW_VER)P1"
else
  SOFTWARE_VERSION?="V1.0.0P1"
endif

PRODUCT_NAME?=c6v3
HARDWARE_VERSION?="v3"
CFG_VERSION?="cfg_ver:c6v3.0.0V1.0.0P1"
CHIP_NAME?=MT7663

# include $(SDK_BUILD_DIR)/targets/94908HND/94908HND
#include $(CONFIG_DIR)/sdk.config


SSTRIP:=$(TOOLCHAIN_ROOT_DIR)/usr/bin/$(TOOLCHAIN_PREFIX)-strip

export TOOLCHAIN_PREFIX
export CONFIG_DIR


toolchain_relocate:
	@if test -d $(TOOLCHAIN_ROOT_DIR); \
	then \
		cp $(TOOLCHAIN_DIR)/libstdc++.a $(TOOLCHAIN_ROOT_DIR)/usr/mipsel-buildroot-linux-uclibc/lib/; \
		cp $(TOOLCHAIN_DIR)/libstdc++.so $(TOOLCHAIN_ROOT_DIR)/usr/mipsel-buildroot-linux-uclibc/lib/; \
		echo "relocate toolchain directory path..."; \
		$(TOOLCHAIN_DIR)/relocate_toolchain_library.sh $(TOOLCHAIN_ROOT_DIR) /opt/buildroot-gcc463; \
	fi; 
 

rootfs_prep:
	@[ ! -d $(IMAGE_ROOTFS_DIR) ] || rm -rf $(IMAGE_ROOTFS_DIR)
	@install -d $(IMAGE_ROOTFS_DIR)

	@# copy kernel modules
	@[ -d "$(IMAGE_ROOTFS_DIR)/lib/modules/$(KERNELVERSION)" ] || mkdir -p $(IMAGE_ROOTFS_DIR)/lib/modules/$(KERNELVERSION)

	@# copy iplatform rootfs
	#@echo "copy IPF rootfs..."
	@cp -af $(IPF_ROOTFS_DIR)/* $(IMAGE_ROOTFS_DIR)

	@# copy SDK modules
	@echo "copy SDK modules ..."
	@-find $(SDK_KERNEL_DIR)/modules -name 'CVS' -o -name '.svn' -o -name '.#*' -o -name '*~' -o -name '.gitignore' | xargs rm -rf
	cp -af $(SDK_KERNEL_DIR)/modules/lib $(IMAGE_ROOTFS_DIR)/
	# remove the linked source folder,to avoid beyond compare find the source codes... by liuqu 20190829
	find $(IMAGE_ROOTFS_DIR)/lib/modules/$(SDK_LINUX_VERSION)/ -type l |xargs rm -f

	@# copy SDK apps
	@echo "copy SDK apps ..."
	@mkdir -p $(IMAGE_ROOTFS_DIR)/usr/bin
	@cp -f $(SDK_APPS_DIR)/hw_nat/hw_nat $(IMAGE_ROOTFS_DIR)/usr/bin
	@cp -f $(SDK_APPS_DIR)/../regs/regs $(IMAGE_ROOTFS_DIR)/usr/bin
	@cp -f $(SDK_APPS_DIR)/switch/switch $(IMAGE_ROOTFS_DIR)/usr/bin

	@# copy wireless apps
	@mkdir -p $(IMAGE_ROOTFS_DIR)/usr/sbin
	@mkdir -p $(IMAGE_ROOTFS_DIR)/sbin
	@cp -f $(SDK_APPS_DIR)/../wireless_tools/iwconfig $(IMAGE_ROOTFS_DIR)/usr/sbin
	@cp -f $(SDK_APPS_DIR)/../wireless_tools/iwpriv $(IMAGE_ROOTFS_DIR)/usr/sbin
	@cp -f $(SDK_APPS_DIR)/../wireless_tools/iwlist $(IMAGE_ROOTFS_DIR)/usr/sbin
	@cp -f $(SDK_APPS_DIR)/../ated_iwpriv/ated_iwpriv $(IMAGE_ROOTFS_DIR)/sbin
	
	@# remove redundance
	@echo "remove redundance..."
	@for mod in `ls $(IMAGE_ROOTFS_DIR)/lib/modules/iplatform/*.ko` ; \
	do \
	  find $(IMAGE_ROOTFS_DIR)/lib/modules/$(SDK_LINUX_VERSION)/ -type f -name `basename $$mod` | xargs rm -f;\
	done \

	@# copy board rootfs
	@echo "copy board rootfs..."
	@cp -af $(CONFIG_DIR)/filesystems/ $(CONFIG_DIR)/filesystems_tmp/ 
	@-find $(CONFIG_DIR)/filesystems_tmp/ -name 'CVS' -o -name '.svn' -o -name '.#*' -o -name '*~' -o -name '.gitignore' | xargs rm -rf
	@cp -af $(CONFIG_DIR)/filesystems_tmp/* $(IMAGE_ROOTFS_DIR)
	@rm -rf $(CONFIG_DIR)/filesystems_tmp/ 


	@cp -auf $(CONFIG_DIR)/wireless/filesystems/* $(IMAGE_ROOTFS_DIR)

	@# copy REGION rootfs
	@if [ -d $(CONFIG_DIR)/REGION/$(REGION)/filesystems ]; then \
		cp -af $(CONFIG_DIR)/REGION/$(REGION)/filesystems/* $(IMAGE_ROOTFS_DIR); \
	fi

	@if [ $(REGION) = "RU" ]; \
	then \
        	$(BUILD_DIR)/../utils/format3G4G/convert3gjson $(IMAGE_ROOTFS_DIR)/www/webpages/data/location.json $(IMAGE_ROOTFS_DIR)/etc/3g4g.json; \
        	gzip $(IMAGE_ROOTFS_DIR)/etc/3g4g.json && mv $(IMAGE_ROOTFS_DIR)/etc/3g4g.json.gz $(IMAGE_ROOTFS_DIR)/etc/3g4g.gz; \
	fi
			
	@# make firmware
	@echo "make firmware..."
	@-find $(IMAGE_ROOTFS_DIR) -name 'CVS' -o -name '.svn' -o -name '.#*' -o -name '*~' -o -name '.gitignore' | xargs rm -rf

shrink_locale:
	@if [ $(REGION) = "RU" ]; \
	then \
		echo "REGION:RU"; \
		find $(IMAGE_ROOTFS_DIR)/www/webpages/locale/  -mindepth 1 -maxdepth 1 -type d |grep -v "en_US" |grep -v "ru_RU" |grep -v "uk_UA" |xargs rm -rf; \
	else \
		echo "REGION: $(REGION)"; \
	fi;
	@if [ $(REGION) = "KR" ]; \
	then \
		echo "REGION:KR"; \
		find $(IMAGE_ROOTFS_DIR)/www/webpages/locale/  -mindepth 1 -maxdepth 1 -type d |grep -v "en_US" |grep -v "ko_KR" |xargs rm -rf; \
	else \
		echo "REGION: $(REGION)"; \
	fi;
	echo "shrink locale done..." 

timestamp_webpages:
	@# rename webpages by grunt for webpages cache
	rm -rf $(IMAGE_ROOTFS_DIR)/../webpages
	# for portal jpg link file
	-mv -f $(IMAGE_ROOTFS_DIR)/www/webpages/themes/green/img/portal_back.jpg $(IMAGE_ROOTFS_DIR)/www/webpages/themes/green/img/portal_logo.jpg $(IMAGE_ROOTFS_DIR)/
	mv -f $(IMAGE_ROOTFS_DIR)/www/webpages $(IMAGE_ROOTFS_DIR)/../
	cp -af $(BUILD_DIR)/hostTools/change_pages_name/* $(IMAGE_ROOTFS_DIR)/../webpages/
	cd $(IMAGE_ROOTFS_DIR)/../webpages/; ./node_modules/grunt/bin/grunt
	mv -f $(IMAGE_ROOTFS_DIR)/../webpages/build/web $(IMAGE_ROOTFS_DIR)/www/webpages
	echo -en `[ -f $(IMAGE_ROOTFS_DIR)/www/webpages/app.*.manifest ] && basename $(IMAGE_ROOTFS_DIR)/www/webpages/app.*.manifest |awk -F '.' '{print $$2}'` > $(IMAGE_ROOTFS_DIR)/etc/webpage_time

shrink_webpages:timestamp_webpages shrink_locale
	@echo "shrink webpages done..."

prepare_partition_files:
	@echo "build default config"
	@# prepare partition files
	@openssl zlib -e -in $(CONFIG_DIR)/defaultconfig.xml | openssl aes-256-cbc -e -K 2EB38F7EC41D4B8E1422805BCD5F740BC3B95BE163E39D67579EB344427F7836 -iv 360028C9064242F81074F4C127D299F6 > $(CONFIG_DIR)/defaultconfig.bin
	@openssl zlib -e -in $(CONFIG_DIR)/apdefconfig.xml | openssl aes-256-cbc -e -K 2EB38F7EC41D4B8E1422805BCD5F740BC3B95BE163E39D67579EB344427F7836 -iv 360028C9064242F81074F4C127D299F6 > $(CONFIG_DIR)/apdefconfig.bin
	@openssl zlib -e -in $(CONFIG_DIR)/profile.xml | openssl aes-256-cbc -e -K 2EB38F7EC41D4B8E1422805BCD5F740BC3B95BE163E39D67579EB344427F7836 -iv 360028C9064242F81074F4C127D299F6 > $(CONFIG_DIR)/profile.bin

	#@[ -d $(IMAGE_ROOTFS_DIR)/etc/partition_config ] || mkdir -p $(IMAGE_ROOTFS_DIR)/etc/partition_config
	#@$(HOSTTOOLS_DIR)/gather -p $(CONFIG_DIR)/partition.conf -i $(CONFIG_DIR)/ -o $(IMAGE_ROOTFS_DIR)/etc/partition_config/ -s $(SOFTWARE_VERSION)
	#@echo $(CFG_VERSION) >> $(IMAGE_ROOTFS_DIR)/etc/partition_config/soft-version
	#@cp -pf $(IMAGE_ROOTFS_DIR)/etc/partition_config/soft-version $(IMAGE_DIR)/
	#@mv -f $(BUILD_DIR)/flashname $(IMAGE_DIR)/
	@echo "prepare partitions done..."	

compile_lua:
	@echo "compile lua files"


mksquashfs:
	echo "make squashfs..."
	$(CONFIG_DIR)/mksquashfs4.2 $(IMAGE_DIR)/rootfs $(IMAGE_DIR)/rootfs_$(PRODUCT_NAME) -noappend -always-use-fragments -all-root -comp xz -b 1024k		
	@echo make $@ done.

firmware:rootfs_prep shrink_webpages prepare_partition_files compile_lua mksquashfs

copy_partition_files:
	cp -f $(SDK_BIN_DIR)/linux.7z $(IMAGE_DIR)/vmlinuz	
	cp $(CONFIG_DIR)/mac.bin $(IMAGE_DIR)/mac.bin
	cp $(CONFIG_DIR)/pin.bin $(IMAGE_DIR)/pin.bin
	cp $(CONFIG_DIR)/productinfo.bin $(IMAGE_DIR)/productinfo.bin
	cp $(CONFIG_DIR)/supportlist.bin $(IMAGE_DIR)/supportlist.bin
	cp $(CONFIG_DIR)/profile.xml $(IMAGE_DIR)/profile.xml
	cp $(CONFIG_DIR)/profile.bin $(IMAGE_DIR)/profile.bin
	cp $(CONFIG_DIR)/defaultconfig.xml $(IMAGE_DIR)/defaultconfig.xml
	cp $(CONFIG_DIR)/defaultconfig.bin $(IMAGE_DIR)/defaultconfig.bin
	cp $(CONFIG_DIR)/apdefconfig.xml $(IMAGE_DIR)/apdefconfig.xml
	cp $(CONFIG_DIR)/apdefconfig.bin $(IMAGE_DIR)/apdefconfig.bin
	

image:
#image:copy_partition_files
	cp $(CONFIG_DIR)/partition.conf $(CONFIG_DIR)/partition.conf.tmp
	sed -ie 's:TOP_DIR:$(BUILD_DIR)/..:g' $(CONFIG_DIR)/partition.conf.tmp
	sed -ie 's:PRODUCT_NAME:$(PRODUCT_NAME):g' $(CONFIG_DIR)/partition.conf.tmp
	cd $(IMAGE_DIR) && $(CONFIG_DIR)/make_flash_cloud_v2 -p $(CONFIG_DIR)/partition.conf.tmp -o "$(PRODUCT_NAME)" -c $(CONFIG_DIR)/cloudinfo.conf -s "V1.0.2P1" -d -m

	

kernel.config:
	@cp -f $(CONFIG_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config

powerfile:
	@cp -f $(CONFIG_DIR)/powerfile/*.c $(SDK_BUILD_DIR)/bcmdrivers/broadcom/net/wl/impl51/4365/src/wl/clm/src/
#	@cp -f $(CONFIG_DIR)/powerfile/*.h $(SDK_BUILD_DIR)/kernel/linux-4.1/include/linux/



kernel_menuconfig:
	@cp -f  $(CONFIG_DIR)/kernel.config  $(SDK_KERNEL_DIR)/.config
	cd $(SDK_KERNEL_DIR)/; $(MAKE) menuconfig
	@cp -f $(SDK_KERNEL_DIR)/.config $(CONFIG_DIR)/kernel.config

kernel_build:
	@cp -f $(CONFIG_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config
	echo "original path is $(PATH)"
	cd $(SDK_KERNEL_DIR) && $(MAKE) dep	
	cd $(SDK_KERNEL_DIR) && $(MAKE) V=2 linux.7z SUPPLIER_TOOLS=$(SDK_ROOT_DIR)/host/toolchain

kernel_header_clean:
	[ ! -d "$(PLATFORM_DIR)/staging_dir/usr-$(PRODUCT_NAME)" ] || rm -rf $(PLATFORM_DIR)/staging_dir/usr-$(PRODUCT_NAME)
	
kernel_header:kernel_header_clean
	cd $(SDK_KERNEL_DIR) && $(MAKE) INSTALL_HDR_PATH=$(PLATFORM_DIR)/staging_dir/usr-$(PRODUCT_NAME) headers_install

kernel_modules:
	[ ! -d "$(SDK_KERNEL_DIR)/modules" ] || rm -rf $(SDK_KERNEL_DIR)/modules
	mkdir -p $(SDK_KERNEL_DIR)/modules
	@cp -f $(CONFIG_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config
	cd $(SDK_KERNEL_DIR) && $(MAKE) modules
	cd $(SDK_KERNEL_DIR) && $(MAKE) DEPMOD=true modules_install INSTALL_MOD_PATH=$(SDK_KERNEL_DIR)/modules

wireless_tools:
	$(MAKE) -C $(SDK_APPS_DIR)/../$@ PREFIX=. CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar RANLIB=$(TOOLPREFIX)ranlib

ated_iwpriv:
	$(MAKE) -C $(SDK_APPS_DIR)/../$@ CC=$(TOOLPREFIX)gcc WIRELESS_TOOLS=../wireless_tools CHIP_NAME=$(CHIP_NAME)

wireless_apps: wireless_tools ated_iwpriv
	@echo build wireless apps

sdk_apps: wireless_apps
	#hw_nat
	cd $(SDK_APPS_DIR)/hw_nat && $(MAKE) CC=$(TOOLPREFIX)gcc LINUXDIR=$(SDK_KERNEL_DIR)
	#regs
	cd $(SDK_APPS_DIR)/../regs && $(MAKE) CC=$(TOOLPREFIX)gcc
	#switch
	cd $(SDK_APPS_DIR)/switch && $(MAKE) CC=$(TOOLPREFIX)gcc LINUXDIR=$(SDK_KERNEL_DIR) CONFIG_RALINK_MT7621=y

sdk: kernel_build kernel_header kernel_modules sdk_apps

sdk_config:sdk.config
	@cp -f $(CONFIG_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config
	echo enter sdk menuconfig

sdk_menuconfig: sdk_config
	echo copy sdk menuconfig;\
 	$(MAKE) -C $(SDK_BUILD_DIR)  menuconfig

sdk_clean: 
	$(MAKE) -C $(SDK_BUILD_DIR) clean


fs-uboot:uboot_clean
	@cp -f $(CONFIG_DIR)/uboot.config $(UBOOT_BUILD_DIR)/.config

	@echo mainuboot: making ...
	cd $(UBOOT_BUILD_DIR) && make silentconfig CONFIG_CROSS_COMPILER_PATH=$(UBOOT_TOOLCHAIN_COMPILER_PATH)
	cd $(UBOOT_BUILD_DIR) && make CONFIG_CROSS_COMPILER_PATH=$(UBOOT_TOOLCHAIN_COMPILER_PATH)
	@cp $(UBOOT_BUILD_DIR)/uboot.bin $(IMAGE_DIR)/uboot.bin




uboot:fs-uboot
	@echo uboot: make.. done



uboot_clean:
	@echo uboot: clean ...
	$(MAKE) -C $(UBOOT_BUILD_DIR) clean;
