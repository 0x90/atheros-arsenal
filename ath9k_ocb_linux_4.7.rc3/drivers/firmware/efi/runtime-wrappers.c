/*
 * runtime-wrappers.c - Runtime Services function call wrappers
 *
 * Copyright (C) 2014 Linaro Ltd. <ard.biesheuvel@linaro.org>
 *
 * Split off from arch/x86/platform/efi/efi.c
 *
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 1999-2002 Hewlett-Packard Co.
 * Copyright (C) 2005-2008 Intel Co.
 * Copyright (C) 2013 SuSE Labs
 *
 * This file is released under the GPLv2.
 */

#include <linux/bug.h>
#include <linux/efi.h>
#include <linux/irqflags.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>
#include <asm/efi.h>

static void efi_call_virt_check_flags(unsigned long flags, const char *call)
{
	unsigned long cur_flags, mismatch;

	local_save_flags(cur_flags);

	mismatch = flags ^ cur_flags;
	if (!WARN_ON_ONCE(mismatch & ARCH_EFI_IRQ_FLAGS_MASK))
		return;

	add_taint(TAINT_FIRMWARE_WORKAROUND, LOCKDEP_NOW_UNRELIABLE);
	pr_err_ratelimited(FW_BUG "IRQ flags corrupted (0x%08lx=>0x%08lx) by EFI %s\n",
			   flags, cur_flags, call);
	local_irq_restore(flags);
}

/*
 * Arch code can implement the following three template macros, avoiding
 * reptition for the void/non-void return cases of {__,}efi_call_virt:
 *
 *  * arch_efi_call_virt_setup
 *
 *    Sets up the environment for the call (e.g. switching page tables,
 *    allowing kernel-mode use of floating point, if required).
 *
 *  * arch_efi_call_virt
 *
 *    Performs the call. The last expression in the macro must be the call
 *    itself, allowing the logic to be shared by the void and non-void
 *    cases.
 *
 *  * arch_efi_call_virt_teardown
 *
 *    Restores the usual kernel environment once the call has returned.
 */

#define efi_call_virt(f, args...)					\
({									\
	efi_status_t __s;						\
	unsigned long flags;						\
	arch_efi_call_virt_setup();					\
	local_save_flags(flags);					\
	__s = arch_efi_call_virt(f, args);				\
	efi_call_virt_check_flags(flags, __stringify(f));		\
	arch_efi_call_virt_teardown();					\
	__s;								\
})

#define __efi_call_virt(f, args...)					\
({									\
	unsigned long flags;						\
	arch_efi_call_virt_setup();					\
	local_save_flags(flags);					\
	arch_efi_call_virt(f, args);					\
	efi_call_virt_check_flags(flags, __stringify(f));		\
	arch_efi_call_virt_teardown();					\
})

/*
 * According to section 7.1 of the UEFI spec, Runtime Services are not fully
 * reentrant, and there are particular combinations of calls that need to be
 * serialized. (source: UEFI Specification v2.4A)
 *
 * Table 31. Rules for Reentry Into Runtime Services
 * +------------------------------------+-------------------------------+
 * | If previous call is busy in	| Forbidden to call		|
 * +------------------------------------+-------------------------------+
 * | Any				| SetVirtualAddressMap()	|
 * +------------------------------------+-------------------------------+
 * | ConvertPointer()			| ConvertPointer()		|
 * +------------------------------------+-------------------------------+
 * | SetVariable()			| ResetSystem()			|
 * | UpdateCapsule()			|				|
 * | SetTime()				|				|
 * | SetWakeupTime()			|				|
 * | GetNextHighMonotonicCount()	|				|
 * +------------------------------------+-------------------------------+
 * | GetVariable()			| GetVariable()			|
 * | GetNextVariableName()		| GetNextVariableName()		|
 * | SetVariable()			| SetVariable()			|
 * | QueryVariableInfo()		| QueryVariableInfo()		|
 * | UpdateCapsule()			| UpdateCapsule()		|
 * | QueryCapsuleCapabilities()		| QueryCapsuleCapabilities()	|
 * | GetNextHighMonotonicCount()	| GetNextHighMonotonicCount()	|
 * +------------------------------------+-------------------------------+
 * | GetTime()				| GetTime()			|
 * | SetTime()				| SetTime()			|
 * | GetWakeupTime()			| GetWakeupTime()		|
 * | SetWakeupTime()			| SetWakeupTime()		|
 * +------------------------------------+-------------------------------+
 *
 * Due to the fact that the EFI pstore may write to the variable store in
 * interrupt context, we need to use a spinlock for at least the groups that
 * contain SetVariable() and QueryVariableInfo(). That leaves little else, as
 * none of the remaining functions are actually ever called at runtime.
 * So let's just use a single spinlock to serialize all Runtime Services calls.
 */
static DEFINE_SPINLOCK(efi_runtime_lock);

static efi_status_t virt_efi_get_time(efi_time_t *tm, efi_time_cap_t *tc)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(get_time, tm, tc);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_set_time(efi_time_t *tm)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(set_time, tm);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_get_wakeup_time(efi_bool_t *enabled,
					     efi_bool_t *pending,
					     efi_time_t *tm)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(get_wakeup_time, enabled, pending, tm);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_set_wakeup_time(efi_bool_t enabled, efi_time_t *tm)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(set_wakeup_time, enabled, tm);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_get_variable(efi_char16_t *name,
					  efi_guid_t *vendor,
					  u32 *attr,
					  unsigned long *data_size,
					  void *data)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(get_variable, name, vendor, attr, data_size,
			       data);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_get_next_variable(unsigned long *name_size,
					       efi_char16_t *name,
					       efi_guid_t *vendor)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(get_next_variable, name_size, name, vendor);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_set_variable(efi_char16_t *name,
					  efi_guid_t *vendor,
					  u32 attr,
					  unsigned long data_size,
					  void *data)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(set_variable, name, vendor, attr, data_size,
			       data);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t
virt_efi_set_variable_nonblocking(efi_char16_t *name, efi_guid_t *vendor,
				  u32 attr, unsigned long data_size,
				  void *data)
{
	efi_status_t status;

	if (!spin_trylock(&efi_runtime_lock))
		return EFI_NOT_READY;

	status = efi_call_virt(set_variable, name, vendor, attr, data_size,
			       data);
	spin_unlock(&efi_runtime_lock);
	return status;
}


static efi_status_t virt_efi_query_variable_info(u32 attr,
						 u64 *storage_space,
						 u64 *remaining_space,
						 u64 *max_variable_size)
{
	efi_status_t status;

	if (efi.runtime_version < EFI_2_00_SYSTEM_TABLE_REVISION)
		return EFI_UNSUPPORTED;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(query_variable_info, attr, storage_space,
			       remaining_space, max_variable_size);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t
virt_efi_query_variable_info_nonblocking(u32 attr,
					 u64 *storage_space,
					 u64 *remaining_space,
					 u64 *max_variable_size)
{
	efi_status_t status;

	if (efi.runtime_version < EFI_2_00_SYSTEM_TABLE_REVISION)
		return EFI_UNSUPPORTED;

	if (!spin_trylock(&efi_runtime_lock))
		return EFI_NOT_READY;

	status = efi_call_virt(query_variable_info, attr, storage_space,
			       remaining_space, max_variable_size);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_get_next_high_mono_count(u32 *count)
{
	efi_status_t status;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(get_next_high_mono_count, count);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static void virt_efi_reset_system(int reset_type,
				  efi_status_t status,
				  unsigned long data_size,
				  efi_char16_t *data)
{
	spin_lock(&efi_runtime_lock);
	__efi_call_virt(reset_system, reset_type, status, data_size, data);
	spin_unlock(&efi_runtime_lock);
}

static efi_status_t virt_efi_update_capsule(efi_capsule_header_t **capsules,
					    unsigned long count,
					    unsigned long sg_list)
{
	efi_status_t status;

	if (efi.runtime_version < EFI_2_00_SYSTEM_TABLE_REVISION)
		return EFI_UNSUPPORTED;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(update_capsule, capsules, count, sg_list);
	spin_unlock(&efi_runtime_lock);
	return status;
}

static efi_status_t virt_efi_query_capsule_caps(efi_capsule_header_t **capsules,
						unsigned long count,
						u64 *max_size,
						int *reset_type)
{
	efi_status_t status;

	if (efi.runtime_version < EFI_2_00_SYSTEM_TABLE_REVISION)
		return EFI_UNSUPPORTED;

	spin_lock(&efi_runtime_lock);
	status = efi_call_virt(query_capsule_caps, capsules, count, max_size,
			       reset_type);
	spin_unlock(&efi_runtime_lock);
	return status;
}

void efi_native_runtime_setup(void)
{
	efi.get_time = virt_efi_get_time;
	efi.set_time = virt_efi_set_time;
	efi.get_wakeup_time = virt_efi_get_wakeup_time;
	efi.set_wakeup_time = virt_efi_set_wakeup_time;
	efi.get_variable = virt_efi_get_variable;
	efi.get_next_variable = virt_efi_get_next_variable;
	efi.set_variable = virt_efi_set_variable;
	efi.set_variable_nonblocking = virt_efi_set_variable_nonblocking;
	efi.get_next_high_mono_count = virt_efi_get_next_high_mono_count;
	efi.reset_system = virt_efi_reset_system;
	efi.query_variable_info = virt_efi_query_variable_info;
	efi.query_variable_info_nonblocking = virt_efi_query_variable_info_nonblocking;
	efi.update_capsule = virt_efi_update_capsule;
	efi.query_capsule_caps = virt_efi_query_capsule_caps;
}
