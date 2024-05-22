#ifndef DMP_MODULE
#define DMP_MODULE

#include <linux/kobject.h>
#include <linux/container_of.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_ctx {
	struct dmp_stats *stats;
	struct dm_dev *dev;
};

#endif /* DMP_MODULE */