DATE_STRING := $(shell date -I)
PREP_DIR := ${PWD}/../imx-bootlets
ELFTOSB := /opt/freescale/ltib/usr/bin/elftosb2
export ELFTOSB

BUILD_DIR ?= .

%.db: %.db.in FORCE
	@sed -e 's:@@BUILD_DIR@@:${BUILD_DIR}:g' $< > $@.tmp || rm -vf $@.tmp; \
	[ -s $@ ] && cmp -s $@ $@.tmp && (echo "Unchanged: $@";rm -f $@.tmp) || (echo "Updated: $@"; mv $@.tmp $@)

u-boot.sb:	u-boot.db $(PREP_DIR)/power_prep/power_prep $(PREP_DIR)/boot_prep/boot_prep $(BUILD_DIR)/u-boot
	$(ELFTOSB) -V -d -z -c $< -o $@

u-boot_ivt.sb:	u-boot_ivt.db $(PREP_DIR)/power_prep/power_prep $(PREP_DIR)/boot_prep/boot_prep $(BUILD_DIR)/u-boot
	$(ELFTOSB) -V -d -z -f imx28 -c $< -o $@

$(PREP_DIR)/boot_prep/boot_prep $(PREP_DIR)/power_prep/power_prep prep_clean prep_distclean: override CFLAGS=
$(PREP_DIR)/boot_prep/boot_prep $(PREP_DIR)/power_prep/power_prep prep_clean prep_distclean: override BOARD=TX28_KARO
$(PREP_DIR)/boot_prep/boot_prep $(PREP_DIR)/power_prep/power_prep:
	$(MAKE) -C $(PREP_DIR) build_prep

clean: prep_clean
distclean: prep_distclean

.PHONY: prep_clean prep_distclean
prep_clean:
	$(MAKE) -C $(PREP_DIR) clean

prep_distclean:
	$(MAKE) -C $(PREP_DIR) clean

.PHONY: FORCE
FORCE:
