// SPDX-License-Identifier: MIT

/*
 * Copyright © 2019 Intel Corporation
 */

#include <linux/debugfs.h>

#include "debugfs_engines.h"
#include "debugfs_gt.h"
#include "debugfs_gt_irq.h"
#include "debugfs_gt_pm.h"
#include "intel_sseu_debugfs.h"
#include "uc/intel_uc_debugfs.h"
#include "i915_drv.h"
#include "intel_gt_pm.h"
#include "intel_gt_requests.h"
#include "uc/intel_uc_debugfs.h"

static int reset_show(void *data, u64 *val)
{
	struct intel_gt *gt = data;
	int ret = intel_gt_terminally_wedged(gt);

	switch (ret) {
	case -EIO:
		*val = 1;
		return 0;
	case 0:
		*val = 0;
		return 0;
	default:
		return ret;
	}
}

static int reset_store(void *data, u64 val)
{
	struct intel_gt *gt = data;

	/* Flush any previous reset before applying for a new one */
	wait_event(gt->reset.queue,
		   !test_bit(I915_RESET_BACKOFF, &gt->reset.flags));

	intel_gt_handle_error(gt, val, I915_ERROR_CAPTURE,
			      "Manually reset engine mask to %llx", val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reset_fops, reset_show, reset_store, "%llu\n");

static void __debugfs_gt_register(struct intel_gt *gt, struct dentry *root)
{
	static const struct debugfs_gt_file files[] = {
		{ "reset", &reset_fops, NULL },
	};

	intel_gt_debugfs_register_files(root, files, ARRAY_SIZE(files), gt);
}

void debugfs_gt_register(struct intel_gt *gt)
{
	struct dentry *root;

	if (!gt->i915->drm.primary->debugfs_root)
		return;

	root = debugfs_create_dir("gt", gt->i915->drm.primary->debugfs_root);
	if (IS_ERR(root))
		return;

	__debugfs_gt_register(gt, root);

	debugfs_engines_register(gt, root);
	debugfs_gt_pm_register(gt, root);
	debugfs_gt_register_irq(gt, root);
	intel_sseu_debugfs_register(gt, root);

	intel_uc_debugfs_register(&gt->uc, root);
}

void intel_gt_debugfs_register_files(struct dentry *root,
				     const struct debugfs_gt_file *files,
				     unsigned long count, void *data)
{
	while (count--) {
		umode_t mode = files->fops->write ? 0644 : 0444;
		if (!files->eval || files->eval(data))
			debugfs_create_file(files->name,
					    mode, root, data,
					    files->fops);

		files++;
	}
}