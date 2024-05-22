#ifndef STATS_H
#define STATS_H

#include <linux/kobject.h>
#include <linux/spinlock_types.h>

struct dmp_stats {
	unsigned long long r_cnt;
	unsigned long long w_cnt;

	unsigned long long r_blsize_cnt;
	unsigned long long w_blsize_cnt;

	spinlock_t rlock;
	spinlock_t wlock;

	struct kobject kobj;
};
#define to_dmp_stats(ptr) container_of(ptr, struct dmp_stats, kobj)

struct dmp_stats_attribute {
	struct attribute attr;
	ssize_t (*show)(struct dmp_stats *stats, char *buf);
};
#define to_dmp_stats_attribute(ptr) \
	container_of(ptr, struct dmp_stats_attribute, attr)

struct dmp_stats *dmp_create_stats(const char *name, struct kset *kset);

void dmp_destroy_stats(struct dmp_stats *stats);

#endif /* STATS_H */