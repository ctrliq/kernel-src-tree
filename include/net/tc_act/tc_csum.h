#ifndef __NET_TC_CSUM_H
#define __NET_TC_CSUM_H

#include <linux/types.h>
#include <net/act_api.h>
#include <linux/tc_act/tc_csum.h>

struct tcf_csum {
	struct tcf_common common;

	u32 update_flags;
};
#define to_tcf_csum(a) \
	container_of(a->priv,struct tcf_csum,common)

static inline bool is_tcf_csum(const struct tc_action *a)
{
#ifdef CONFIG_NET_CLS_ACT
	if (a->ops && a->ops->type == TCA_ACT_CSUM)
		return true;
#endif
	return false;
}

static inline u32 tcf_csum_update_flags(const struct tc_action *a)
{
	return to_tcf_csum(a)->update_flags;
}

#endif /* __NET_TC_CSUM_H */
