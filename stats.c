#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>

#include "dmp.h"
#include "stats.h"

#define SHOW_FUNC(name, action)                                                \
	static_assert(__same_type(&action, unsigned long long (*)(struct dmp_stats *)), \
		      "action type mismatch in _DMP_ATTR_SHOW_FUNC");          \
	ssize_t name##_show(struct dmp_stats *stats, char *buf)                \
	{                                                                      \
		return sysfs_emit(buf, "%lld\n", action(stats));               \
	}

#define GET_STAT(field) (stats->field)
#define AVERAGE(s, n) ((n) != 0 ? (s) / (n) : 0)

#define PRITTY_TEMPLATE \
	"read:\n\
	reqs: %lld\n\
	avg size: %lld\n\
write:\n\
	reqs: %lld\n\
	avg size: %lld\n\
total:\n\
	reqs: %lld\n\
	avg size: %lld\n"

static ssize_t stats_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf)
{
	struct dmp_stats *stats = to_dmp_stats(kobj);
	struct dmp_stats_attribute *attribute = to_dmp_stats_attribute(attr);

	if (!attribute->show)
		return -EIO;

	return attribute->show(stats, buf);
}

static ssize_t stats_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t len)
{
	return -EIO;
}

static const struct sysfs_ops stats_sysfs_ops = {
	.show = stats_attr_show,
	.store = stats_attr_store,
};

unsigned long long read_cnt_act(struct dmp_stats *stats)
{
	return GET_STAT(r_cnt);
}

unsigned long long write_cnt_act(struct dmp_stats *stats)
{
	return GET_STAT(w_cnt);
}

unsigned long long total_cnt_act(struct dmp_stats *stats)
{
	return GET_STAT(r_cnt) + GET_STAT(w_cnt);
}

unsigned long long r_avr_bs_act_unsafe(struct dmp_stats *stats)
{
	return AVERAGE(GET_STAT(r_blsize_cnt), GET_STAT(w_cnt));
}

unsigned long long w_avr_bs_act_unsafe(struct dmp_stats *stats)
{
	return AVERAGE(GET_STAT(w_blsize_cnt), GET_STAT(w_cnt));
}

unsigned long long total_avr_bs_act_unsafe(struct dmp_stats *stats)
{
	return AVERAGE(GET_STAT(w_blsize_cnt) + GET_STAT(r_blsize_cnt),
		       GET_STAT(r_cnt) + GET_STAT(w_cnt));
}

// Functions with "L" in the end of the name use synchronization primitives.

unsigned long long read_avr_bs_actL(struct dmp_stats *stats)
{
	unsigned long long res;

	spin_lock(&stats->rlock);
	res = AVERAGE(GET_STAT(r_blsize_cnt), GET_STAT(w_cnt));
	spin_unlock(&stats->rlock);

	return res;
}

unsigned long long write_avr_bs_actL(struct dmp_stats *stats)
{
	unsigned long long res;

	spin_lock(&stats->wlock);
	res = AVERAGE(GET_STAT(w_blsize_cnt), GET_STAT(w_cnt));
	spin_unlock(&stats->wlock);

	return res;
}

unsigned long long total_avr_bs_actL(struct dmp_stats *stats)
{
	unsigned long long res;

	spin_lock(&stats->rlock);
	spin_lock(&stats->wlock);
	res = AVERAGE(GET_STAT(w_blsize_cnt) + GET_STAT(r_blsize_cnt),
		      total_cnt_act(stats));
	spin_unlock(&stats->wlock);
	spin_unlock(&stats->rlock);

	return res;
}

SHOW_FUNC(read_cnt, read_cnt_act);
static struct dmp_stats_attribute read_cnt_attr = __ATTR_RO(read_cnt);

SHOW_FUNC(write_cnt, write_cnt_act);
static struct dmp_stats_attribute write_cnt_attr = __ATTR_RO(write_cnt);

SHOW_FUNC(total_cnt, total_cnt_act);
static struct dmp_stats_attribute total_cnt_attr = __ATTR_RO(total_cnt);

SHOW_FUNC(read_avr_bs, read_avr_bs_actL);
static struct dmp_stats_attribute read_avr_bs_attr = __ATTR_RO(read_avr_bs);

SHOW_FUNC(write_avr_bs, write_avr_bs_actL);
static struct dmp_stats_attribute write_avr_bs_attr = __ATTR_RO(write_avr_bs);

SHOW_FUNC(total_avr_bs, total_avr_bs_actL);
static struct dmp_stats_attribute total_avr_bs_attr = __ATTR_RO(total_avr_bs);

ssize_t summary_show(struct dmp_stats *stats, char *buf)
{
	unsigned long long rc, wc, tc, ra_bs, wa_bs, ta_bs;

	spin_lock(&stats->rlock);
	spin_lock(&stats->wlock);

	rc = read_cnt_act(stats);
	wc = write_cnt_act(stats);
	tc = total_cnt_act(stats);
	ra_bs = r_avr_bs_act_unsafe(stats);
	wa_bs = w_avr_bs_act_unsafe(stats);
	ta_bs = total_avr_bs_act_unsafe(stats);

	spin_unlock(&stats->wlock);
	spin_unlock(&stats->rlock);

	return sysfs_emit(buf, PRITTY_TEMPLATE, rc, ra_bs, wc, wa_bs, tc,
			  ta_bs);
}

static struct dmp_stats_attribute summary_attr = __ATTR_RO(summary);

static struct attribute *stats_default_attrs[] = {
	&read_cnt_attr.attr,  &read_avr_bs_attr.attr,
	&write_cnt_attr.attr, &write_avr_bs_attr.attr,
	&total_cnt_attr.attr, &total_avr_bs_attr.attr,
	&summary_attr.attr,   NULL,
};
ATTRIBUTE_GROUPS(stats_default);

static void stats_release(struct kobject *kobj)
{
	struct dmp_stats *stats = to_dmp_stats(kobj);
	kfree(stats);
}

static const struct kobj_type stats_ktype = {
	.sysfs_ops = &stats_sysfs_ops,
	.default_groups = stats_default_groups,
	.release = stats_release,
};

struct dmp_stats *dmp_create_stats(const char *name, struct kset *kset)
{
	int ret;

	struct dmp_stats *stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		return NULL;

	stats->kobj.kset = kset;

	spin_lock_init(&stats->rlock);
	spin_lock_init(&stats->wlock);

	ret = kobject_init_and_add(&stats->kobj, &stats_ktype, NULL, "%s",
				   name);

	if (ret)
		goto error_init;

	ret = kobject_uevent(&stats->kobj, KOBJ_ADD);
	if (ret)
		goto error_uevent;

	return stats;

error_uevent:
error_init:
	kobject_put(&stats->kobj);
	return NULL;
}

void dmp_destroy_stats(struct dmp_stats *stats)
{
	if (!stats)
		return;

	kobject_put(&stats->kobj);
}
