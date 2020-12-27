#
# Makefile for http stuff
#

include $(TOPDIR)/config.mk

LIB   = libhttpd.a
#OBJS += uip.o uip_arch.o uip_arp.o httpd.o fs.o
OBJS += uip.o uip_arch.o uip_arp.o httpd.o fsdata.o

all:    $(LIB)

#fs.c:   fsdata.c

#fsdata.c:
#	    ./vendors/makefsdatac $(DEVICE_VENDOR)
fsdata.c:
	    cp ./image/upgrade.html ./
		model_name=$(CONFIG_BOOT_WEBPAGE_PRODUCT_TAG); \
		model_version=$(CONFIG_BOOT_WEBPAGE_PRODUCT_VERSION); \
		model_tag="$$model_name $$model_version"; \
	    eval "sed -i 's/TP_NEED_MODIFIED_PRODUCT_NAME/$$model_tag/g' ./upgrade.html"
	    xxd -i ./upgrade.html | sed -e s/_.*upgrade_html/upgrade_html/ > ./fsdata.c
	    rm ./upgrade.html

$(LIB): $(START) $(OBJS)
	    $(AR) crv $@ $(OBJS)

#########################################################################

.depend: Makefile $(OBJS:.o=.c)
	     $(CC) -M $(CFLAGS) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################

