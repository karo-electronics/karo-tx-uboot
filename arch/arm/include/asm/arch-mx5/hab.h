/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 *
*/

#ifndef __ARCH_MX5_HAB_H
#define __ARCH_MX5_HAB_H

#ifdef CONFIG_SECURE_BOOT

#include <linux/types.h>

int get_hab_status(void);

/* -------- start of HAB API updates ------------*/
/* The following are taken from HAB4 SIS */

/* Status definitions */
typedef enum hab_status {
	HAB_STS_ANY = 0x00,
	HAB_FAILURE = 0x33,
	HAB_WARNING = 0x69,
	HAB_SUCCESS = 0xf0
} hab_status_t;

/* Security Configuration definitions */
typedef enum hab_config {
	HAB_CFG_RETURN = 0x33, /**< Field Return IC */
	HAB_CFG_OPEN = 0xf0, /**< Non-secure IC */
	HAB_CFG_CLOSED = 0xcc /**< Secure IC */
} hab_config_t;

/* State definitions */
typedef enum hab_state {
	HAB_STATE_INITIAL = 0x33, /**< Initialising state (transitory) */
	HAB_STATE_CHECK = 0x55, /**< Check state (non-secure) */
	HAB_STATE_NONSECURE = 0x66, /**< Non-secure state */
	HAB_STATE_TRUSTED = 0x99, /**< Trusted state */
	HAB_STATE_SECURE = 0xaa, /**< Secure state */
	HAB_STATE_FAIL_SOFT = 0xcc, /**< Soft fail state */
	HAB_STATE_FAIL_HARD = 0xff, /**< Hard fail state (terminal) */
	HAB_STATE_NONE = 0xf0, /**< No security state machine */
	HAB_STATE_MAX
} hab_state_t;

typedef enum hab_target {
	HAB_TGT_MEMORY = 0x0f, /* Check memory white list */
	HAB_TGT_PERIPHERAL = 0xf0, /* Check peripheral white list*/
} hab_target_t;

enum HAB_FUNC_OFFSETS {
	HAB_RVT_HEADER,
	HAB_RVT_ENTRY,
	HAB_RVT_EXIT,
	HAB_RVT_CHECK_TARGET,
	HAB_RVT_AUTHENTICATE_IMAGE,
	HAB_RVT_RUN_DCD,
	HAB_RVT_RUN_CSF,
	HAB_RVT_ASSERT,
	HAB_RVT_REPORT_EVENT,
	HAB_RVT_REPORT_STATUS,
	HAB_RVT_FAILSAFE,
};

/* Function prototype description */
typedef hab_status_t hab_rvt_entry_t(void);

typedef hab_status_t hab_rvt_exit_t(void);

typedef hab_status_t hab_rvt_check_target_t(hab_target_t, const void *,
		size_t);

typedef hab_status_t hab_loader_callback_f_t(void**, size_t*, const void*);
typedef void *hab_rvt_authenticate_image_t(uint8_t, ptrdiff_t,
		void **, size_t *, hab_loader_callback_f_t);

typedef hab_status_t hab_rvt_assert_t(uint32_t, const void *,
		size_t);

typedef hab_status_t hab_rvt_report_event_t(hab_status_t, uint32_t,
		uint8_t* , size_t*);

typedef hab_status_t hab_rvt_report_status_t(enum hab_config *,
		enum hab_state *);

typedef void hapi_clock_init_t(void);

#define HAB_RVT_BASE			0x00000094

static inline void **hab_rvt_base(void)
{
	uint32_t *base = (void *)0x94;

	if ((*base & 0xff0000ff) != cpu_to_be32(0xdd000040)) {
		printf("Invalid RVT @ %p\n", base);
		hang();
	}
	return (void **)base;
}

#define HAB_CID_ROM 0 /**< ROM Caller ID */
#define HAB_CID_UBOOT 1 /**< UBOOT Caller ID*/
/* ----------- end of HAB API updates ------------*/

#define hab_rvt_entry_p						\
	((hab_rvt_entry_t *)hab_rvt_base()[HAB_RVT_ENTRY])

#define hab_rvt_exit_p						\
	((hab_rvt_exit_t *)hab_rvt_base()[HAB_RVT_EXIT])

#define hab_rvt_check_target_p					\
	((hab_rvt_check_target_t*)hab_rvt_base()[HAB_RVT_CHECK_TARGET])

#define hab_rvt_authenticate_image_p				\
	((hab_rvt_authenticate_image_t *)hab_rvt_base()[HAB_RVT_AUTHENTICATE_IMAGE])

#define hab_rvt_assert_p					\
	((hab_rvt_assert_t*)hab_rvt_base()[HAB_RVT_ASSERT])

#define hab_rvt_report_event_p					\
	((hab_rvt_report_event_t*)hab_rvt_base()[HAB_RVT_REPORT_EVENT])

#define hab_rvt_report_status_p					\
	((hab_rvt_report_status_t*)hab_rvt_base()[HAB_RVT_REPORT_STATUS])

#define HAB_FUNC(n, rt)							\
static inline rt hab_rvt_##n(void)					\
{									\
	if (hab_rvt_base() == NULL)					\
		return (rt)-1;						\
	return hab_rvt_##n##_p();					\
}									\

#define HAB_FUNC2(n, rt, t1, t2)					\
static inline rt hab_rvt_##n(t1 p1, t2 p2)				\
{									\
	if (hab_rvt_base() == NULL)					\
		return (rt)-1;						\
	return hab_rvt_##n##_p(p1, p2);					\
}

#define HAB_FUNC3(n, rt, t1, t2, t3)					\
static inline rt hab_rvt_##n(t1 p1, t2 p2, t3 p3)			\
{									\
	if (hab_rvt_base() == NULL)					\
		return (rt)-1;						\
	return hab_rvt_##n##_p(p1, p2, p3);				\
}

#define HAB_FUNC4(n, rt, t1, t2, t3, t4)				\
static inline rt hab_rvt_##n(t1 p1, t2 p2, t3 p3, t4 p4)		\
{									\
	if (hab_rvt_base() == NULL)					\
		return (rt)-1;						\
	return hab_rvt_##n##_p(p1, p2, p3, p4);				\
}

#define HAB_FUNC5(n, rt, t1, t2, t3, t4, t5)				\
static inline rt hab_rvt_##n(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)		\
{									\
	if (hab_rvt_base() == NULL)					\
		return (rt)-1;						\
	return hab_rvt_##n##_p(p1, p2, p3, p4, p5);			\
}

#else /* CONFIG_SECURE_BOOT */

static inline int get_hab_status(void)
{
	return 0;
}

#endif /* CONFIG_SECURE_BOOT */
#endif /* __ARCH_MX5_HAB_H */
