#include <linux/device-mapper.h>
#include <linux/bio.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/atomic/atomic-instrumented.h>

#include "dmp.h"
#include "stats.h"

static struct dmp_stats *common_stats;

static struct kset *dmp_stats_kset;

static unsigned int get_block_size(struct bio *bio)
{
	if (bio && bio->bi_bdev) {
		return bio->bi_iter.bi_size;
	}
	return 0;
}

static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct dmp_ctx *dc;
	int ret;

	if (argc != 1) {
		ti->error = "Invalid argument count";
		return -EINVAL;
	}

	dc = kzalloc(sizeof(*dc), GFP_KERNEL);
	if (dc == NULL) {
		ti->error = "Cannot allocate dmp context";
		return -ENOMEM;
	}

	ret = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table),
			    &dc->dev);
	if (ret) {
		ti->error = "Device lookup failed";
		goto error_free;
	}

	dc->stats = dmp_create_stats(dm_table_device_name(ti->table),
				     dmp_stats_kset);
	if (!dc->stats) {
		ret = -ENOMEM;
		goto error_device;
	}

	ti->private = dc;
	return 0;

error_device:
	dm_put_device(ti, dc->dev);

error_free:
	kfree(dc);
	return ret;
}

static void dmp_dtr(struct dm_target *ti)
{
	struct dmp_ctx *dc = ti->private;

	dm_put_device(ti, dc->dev);
	dmp_destroy_stats(dc->stats);
	kfree(dc);
}

static void update_stats(struct bio *bio, struct dmp_stats *stats)
{
	u32 bs = get_block_size(bio);

	if (bs == 0) {
		DMERR("strange block_size == 0");
	}

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		spin_lock(&stats->rlock);

		stats->r_cnt++;
		stats->r_blsize_cnt += bs;

		spin_unlock(&stats->rlock);
		break;

	case REQ_OP_WRITE:
		spin_lock(&stats->wlock);

		stats->w_cnt++;
		stats->w_blsize_cnt += bs;

		spin_unlock(&stats->wlock);
		break;
	default:
		break;
	}
}

static int dmp_map(struct dm_target *ti, struct bio *bio)
{
	struct dmp_ctx *dc = ti->private;

	bio_set_dev(bio, dc->dev->bdev);
	update_stats(bio, dc->stats);
	update_stats(bio, common_stats);

	return DM_MAPIO_REMAPPED;
}

static struct target_type dmp_target = {
	.name = DM_MSG_PREFIX,
	.version = { 1, 0, 0 },
	.module = THIS_MODULE,
	.ctr = dmp_ctr,
	.dtr = dmp_dtr,
	.map = dmp_map,
};

// ===================================== //

static int __init dm_dmp_init(void)
{
	int ret;

	dmp_stats_kset =
		kset_create_and_add("stat", NULL, &THIS_MODULE->mkobj.kobj);
	if (!dmp_stats_kset) {
		ret = -ENOMEM;
		goto error_kset_register;
	}

	common_stats = dmp_create_stats("all_devs", dmp_stats_kset);
	if (!common_stats) {
		ret = -ENOMEM;
		goto error_stats_create;
	}

	ret = dm_register_target(&dmp_target);
	if (ret < 0)
		goto error_register_failed;

	return ret;

error_register_failed:
error_stats_create:
	dmp_destroy_stats(common_stats);

error_kset_register:
	kset_unregister(dmp_stats_kset);
	DMERR("register failed %d", ret);
	return ret;
}

static void __exit dm_dmp_exit(void)
{
	dm_unregister_target(&dmp_target);
	dmp_destroy_stats(common_stats);
	kset_unregister(dmp_stats_kset);
}

module_init(dm_dmp_init);
module_exit(dm_dmp_exit);

MODULE_AUTHOR("Georgy Sichkar <mail4egor@gmail.com>");
MODULE_DESCRIPTION(DM_NAME "  device mapper proxy (dmp)");
MODULE_LICENSE("GPL");
