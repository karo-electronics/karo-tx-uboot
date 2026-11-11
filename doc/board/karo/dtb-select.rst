.. SPDX-License-Identifier: GPL-2.0+

Runtime control DT selection (Ka-Ro i.MX6 modules)
===================================================

One U-Boot image per module. The FIT appended to U-Boot holds the control
device tree of the bare module first, then one device tree per carrier board
named ``<module DT>-<baseboard>``. Before relocation U-Boot reads the stored
environment, takes its ``baseboard`` variable and selects the matching FIT
configuration:

* ``baseboard`` names a carrier present in the FIT: that DT runs;
* ``baseboard`` names a DT the FIT does not carry: the bare module DT runs;
* ``baseboard`` is empty, the stored environment is missing or it fails its
  CRC: ``CONFIG_KARO_DEFAULT_BASEBOARD`` applies (empty on
  ``<module>_defconfig``, hence the bare module DT; ``mb7`` on
  ``<module>-mb7_defconfig``).

The mfg and noenv images ignore the stored environment and always run the
default. Every boot prints the outcome, for example::

    Control DT: Ka-Ro electronics TXUL-8013 Module on MB7 baseboard
      baseboard 'mb7' from stored environment

Switching carrier from the U-Boot prompt::

    => setenv baseboard mycarrier
    => saveenv
    => reset

Each module has two sets of defconfigs, like the i.MX93 modules. They carry
the same control device trees (``CONFIG_OF_LIST``) and differ only in the
default environment and the fallback carrier: ``<module>_defconfig`` takes
``<module>.env`` with ``baseboard`` empty and falls back to the bare module
DT, ``<module>-mb7_defconfig`` takes ``<module>-mb7.env`` with
``baseboard=mb7``. Each has ``_mfg`` and ``_noenv`` variants.

Adding a carrier board
----------------------

1. Write ``arch/arm/dts/<module DT>-<carrier>.dts``: include the module
   device tree ``<module DT>.dts`` and describe what the carrier adds
   (Ethernet PHY, USB power, display...).
   ``arch/arm/dts/imx6ull-txul-8013-mb7.dts`` is the model. An optional
   ``<module DT>-<carrier>-u-boot.dtsi`` carries the U-Boot-only fixups and
   includes ``<module DT>-u-boot.dtsi`` first, as
   ``imx6ull-txul-8013-mb7-u-boot.dtsi`` does; no pre-relocation tag is needed
   there, the selection itself runs on the module DT.
2. Add ``<module DT>-<carrier>.dtb`` to the per-family list in
   ``arch/arm/dts/Makefile``.
3. Append ``<module DT>-<carrier>`` to ``CONFIG_OF_LIST`` in the six
   defconfigs of the module (``<module>_defconfig``,
   ``<module>-mb7_defconfig`` and their ``_mfg`` and ``_noenv`` variants)
   and enable the drivers the carrier needs: one binary carries the drivers of
   every carrier it can run on.
4. With Yocto (meta-karo-nxp), add the carrier to ``KARO_BASEBOARDS`` of the
   machine instead of editing the defconfigs: the recipe rebuilds
   ``CONFIG_OF_LIST`` from it and takes ``KARO_BASEBOARD`` as the default
   carrier. ``KARO_UBOOT_BASEBOARDS`` narrows the FIT when the bootloader must
   not carry every carrier.
5. Keep ``fdt_file`` on the Linux device tree of the module; the carrier is
   the ``karo-<carrier>`` overlay listed in ``overlays_<carrier>``, applied at
   boot like on the i.MX8M and i.MX93 modules. NAND modules load no overlay
   files: their ``dtb`` partition holds the Linux device tree merged at build
   time with the overlays of ``KARO_BASEBOARD``, or the bare module device
   tree when ``KARO_BASEBOARD`` is empty (``board/karo/common/fdt.c``).

The selection engine is ``board/karo/common/dtb_select.c``. Its media
backend follows ``CONFIG_ENV_IS_IN_MMC`` or ``CONFIG_ENV_IS_IN_NAND``, so it
always reads the block that ``saveenv`` writes.
