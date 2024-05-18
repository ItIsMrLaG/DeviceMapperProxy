#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_ctx
{
    long long I_counter;
    long long O_counter;

    long long avr_I_blsize;
    long long avr_O_blsize;

    struct dm_dev *dev;
};


static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv);
static void dmp_dtr(struct dm_target *ti);
static int dmp_map(struct dm_target *ti, struct bio *bio);


static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
    struct dmp_ctx *dc;
    int ret;

    if (argc != 1) {
        ti->error = "Invalid argument count";
		return -EINVAL;
    }

    dc = kmalloc(sizeof(*dc), GFP_KERNEL);
    if (dc == NULL) {
        ti->error = "Cannot allocate linear context";
		return -ENOMEM;
    }

    ret = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dc->dev);
    if (ret) {
		ti->error = "Device lookup failed";
		goto error_free;
	}

    ti->private = dc;
    return 0;

error_free:
    kfree(dc);
	return ret;
}

static void dmp_dtr(struct dm_target *ti)
{
    struct dmp_ctx *dc = ti->private;

	dm_put_device(ti, dc->dev);
	kfree(lc);
}


static struct target_type dmp_target = {
	.name   = "dmp",
	.module = THIS_MODULE,
	.ctr    = dmp_ctr,
	.dtr    = dmp_dtr,
	.map    = dmp_map,
};


static int __init dm_dmp_init(void)
{
	int r = dm_register_target(&dmp_target);

	if (r < 0)
		DMERR("register failed %d", r);

	return r;
}

static void dm_dmp_exit(void)
{
	dm_unregister_target(&dmp_target);
}

module_init(dm_dmp_init);
module_exit(dm_dmp_exit);

MODULE_AUTHOR("Georgy Sichkar <mail4egor@gmail.com>");
MODULE_DESCRIPTION(DM_NAME "  device mapper proxy");
MODULE_LICENSE("GPL");
