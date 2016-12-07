/*
 * Copyright(c) 2016 Intel Corporation.
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _HFI1_MMU_RB_H
#define _HFI1_MMU_RB_H

#include "hfi.h"

struct mmu_rb_node {
	unsigned long addr;
	unsigned long len;
	unsigned long __last;
	struct rb_node node;
};

struct mmu_rb_ops {
	bool (*filter)(struct mmu_rb_node *, unsigned long, unsigned long);
	int (*insert)(struct rb_root *, struct mmu_rb_node *);
	void (*remove)(struct rb_root *, struct mmu_rb_node *,
		       struct mm_struct *);
	int (*invalidate)(struct rb_root *, struct mmu_rb_node *);
};

int hfi1_mmu_rb_register(struct rb_root *root, struct mmu_rb_ops *ops);
void hfi1_mmu_rb_unregister(struct rb_root *);
int hfi1_mmu_rb_insert(struct rb_root *, struct mmu_rb_node *);
void hfi1_mmu_rb_remove(struct rb_root *, struct mmu_rb_node *);
struct mmu_rb_node *hfi1_mmu_rb_search(struct rb_root *, unsigned long,
				       unsigned long);
struct mmu_rb_node *hfi1_mmu_rb_extract(struct rb_root *, unsigned long,
					unsigned long);

#endif /* _HFI1_MMU_RB_H */
