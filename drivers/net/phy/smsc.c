/*
 * SMSC PHY drivers
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Base code from drivers/net/phy/davicom.c
 *   Copyright 2010-2011 Freescale Semiconductor, Inc.
 *   author Andy Fleming
 *
 * Some code get from linux kenrel
 * Copyright (c) 2006 Herbert Valerio Riedel <hvr@gnu.org>
 */
#include <miiphy.h>
#include <errno.h>

#define MII_LAN83C185_CTRL_STATUS	17 /* Mode/Status Register */
#define MII_LAN83C185_EDPWRDOWN		(1 << 13) /* EDPWRDOWN */
#define MII_LAN83C185_ENERGYON		(1 << 1)  /* ENERGYON */

static int smsc_parse_status(struct phy_device *phydev)
{
	int bmcr;
	int aneg_exp;
	int mii_adv;
	int lpa;

	aneg_exp = phy_read(phydev, MDIO_DEVAD_NONE, MII_EXPANSION);
	if (aneg_exp < 0)
		return aneg_exp;

	if (aneg_exp & EXPANSION_MFAULTS) {
		/* second read to clear latched status */
		aneg_exp = phy_read(phydev, MDIO_DEVAD_NONE, MII_EXPANSION);
		if (aneg_exp & EXPANSION_MFAULTS)
			return -EIO;
	}

	bmcr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	if (bmcr & BMCR_ANENABLE) {
		lpa = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
		if (lpa < 0)
			return lpa;
		mii_adv = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
		if (mii_adv < 0)
			return mii_adv;
		lpa &= mii_adv;

		if (!(aneg_exp & EXPANSION_NWAY)) {
			/* parallel detection */
			phydev->duplex = DUPLEX_HALF;
			if (lpa & (LPA_100HALF | LPA_100FULL))
				phydev->speed = SPEED_100;
			else
				phydev->speed = SPEED_10;
		}

		if (lpa & (LPA_100FULL | LPA_100HALF)) {
			phydev->speed = SPEED_100;
			if (lpa & LPA_100FULL)
				phydev->duplex = DUPLEX_FULL;
			else
				phydev->duplex = DUPLEX_HALF;
		} else if (lpa & (LPA_10FULL | LPA_10HALF)) {
			phydev->speed = SPEED_10;
			if (lpa & LPA_10FULL)
				phydev->duplex = DUPLEX_FULL;
			else
				phydev->duplex = DUPLEX_HALF;
		} else {
			return -EINVAL;
		}
	} else {
		if (bmcr & BMCR_SPEED100)
			phydev->speed = SPEED_100;
		else
			phydev->speed = SPEED_10;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;
	}
	return 0;
}

static int smsc_startup(struct phy_device *phydev)
{
	int ret;

	if (!phydev->link) {
		/* Disable EDPD to wake up PHY */
		ret = phy_read(phydev, MDIO_DEVAD_NONE, MII_LAN83C185_CTRL_STATUS);

		if (ret < 0)
			return ret;

		if (ret & MII_LAN83C185_EDPWRDOWN) {
			ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_LAN83C185_CTRL_STATUS,
				ret & ~MII_LAN83C185_EDPWRDOWN);
			if (ret < 0)
				return ret;

			/* Sleep 64 ms to allow ~5 link test pulses to be sent */
			udelay(64 * 1000);
		}
	}

	ret = genphy_update_link(phydev);
	if (ret < 0)
		return ret;
	return smsc_parse_status(phydev);
}

static int smsc_mdix_setup(struct phy_device *phydev)
{
	int ret;
	static const char *mdix = "_mdi";
	char varname[strlen(phydev->bus->name) + strlen(mdix) + 1];
	const char *val;

	snprintf(varname, sizeof(varname), "%s%s", phydev->bus->name, mdix);

	val = getenv(varname);
	if (val) {
		if (strcasecmp(val, "auto") == 0) {
			ret = 0;
		} else if (strcasecmp(val, "mdix") == 0) {
			ret = 1;
		} else if (strcasecmp(val, "mdi") == 0) {
			ret = 2;
		} else {
			printf("Improper setting '%s' for %s\n", val, varname);
			printf("Expected one of: 'auto', 'mdi', 'mdix'\n");
			return -EINVAL;
		}
	} else {
		ret = 0;
	}
	return ret;
}

static int smsc_config(struct phy_device *phydev)
{
	int mdix = smsc_mdix_setup(phydev);

	if (mdix < 0) {
		return mdix;
	} else if (mdix) {
		int ret = phy_write(phydev, MDIO_DEVAD_NONE, 0x1b,
				(1 << 15) | ((mdix & 1) << 13));
		if (ret) {
			printf("Failed to setup MDI/MDIX mode: %d\n", ret);
			return ret;
		}
	}
	return genphy_config_aneg(phydev);
}

static struct phy_driver lan8700_driver = {
	.name = "SMSC LAN8700",
	.uid = 0x0007c0c0,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &smsc_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver lan911x_driver = {
	.name = "SMSC LAN911x Internal PHY",
	.uid = 0x0007c0d0,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &smsc_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver lan8710_driver = {
	.name = "SMSC LAN8710/LAN8720",
	.uid = 0x0007c0f0,
	.mask = 0xffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &smsc_config,
	.startup = &smsc_startup,
	.shutdown = &genphy_shutdown,
};

int phy_smsc_init(void)
{
	phy_register(&lan8710_driver);
	phy_register(&lan911x_driver);
	phy_register(&lan8700_driver);

	return 0;
}
