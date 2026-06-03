// mtrc_dm.c — Matter data-model registry. See mtrc_dm.h. GPLv3.

#include "mtrc_dm.h"

#ifdef MTRC_PLUGIN_BUILD
#define dm (g.dm)              // storage rides the PSRAM ctx (g); a ~9 KB file-scope
                               // mutable struct can't relocate in a BinPlugin
#else
static struct {
  mtrc_dm_endpoint_t ep[MTRC_DM_MAX_ENDPOINTS];   int ep_n;
  mtrc_dm_cluster_t  cl[MTRC_DM_MAX_CLUSTERS];     int cl_n;
  mtrc_dm_attr_t     at[MTRC_DM_MAX_ATTRS];        int at_n;
} dm;
#endif

MODULE_PART void mtrc_dm_reset(void) { SETMEMREGS
  dm.ep_n = 0; dm.cl_n = 0; dm.at_n = 0;
}

MODULE_PART int mtrc_dm_add_endpoint(uint16_t endpoint, uint32_t device_type) { SETMEMREGS
  for (int i = 0; i < dm.ep_n; i++)
    if (dm.ep[i].endpoint == endpoint) { dm.ep[i].device_type = device_type; return i; }
  if (dm.ep_n >= MTRC_DM_MAX_ENDPOINTS) return -1;
  dm.ep[dm.ep_n].endpoint = endpoint;
  dm.ep[dm.ep_n].device_type = device_type;
  return dm.ep_n++;
}

MODULE_PART int mtrc_dm_add_cluster(uint16_t endpoint, uint32_t cluster) { SETMEMREGS
  for (int i = 0; i < dm.cl_n; i++)
    if (dm.cl[i].endpoint == endpoint && dm.cl[i].cluster == cluster) return 0;
  if (dm.cl_n >= MTRC_DM_MAX_CLUSTERS) return -1;
  dm.cl[dm.cl_n].endpoint = endpoint;
  dm.cl[dm.cl_n].cluster  = cluster;
  dm.cl_n++;
  return 0;
}

MODULE_PART int mtrc_dm_add_attr(uint16_t endpoint, uint32_t cluster, uint32_t attr,
                     mtrc_dm_type_t type, uint8_t flags, uint64_t initial) { SETMEMREGS
  if (mtrc_dm_add_cluster(endpoint, cluster) < 0) return -1;
  mtrc_dm_attr_t *a = mtrc_dm_find(endpoint, cluster, attr);
  if (a) { a->type = (uint8_t)type; a->flags = flags; a->value = initial; return 0; }
  if (dm.at_n >= MTRC_DM_MAX_ATTRS) return -1;
  a = &dm.at[dm.at_n++];
  a->endpoint = endpoint; a->cluster = cluster; a->attr = attr;
  a->type = (uint8_t)type; a->flags = flags; a->value = initial;
  return 0;
}

MODULE_PART mtrc_dm_attr_t *mtrc_dm_find(uint16_t endpoint, uint32_t cluster, uint32_t attr) { SETMEMREGS
  for (int i = 0; i < dm.at_n; i++)
    if (dm.at[i].endpoint == endpoint && dm.at[i].cluster == cluster &&
        dm.at[i].attr == attr) return &dm.at[i];
  return NULL;
}

MODULE_PART int mtrc_dm_get(uint16_t endpoint, uint32_t cluster, uint32_t attr, uint64_t *out) { SETMEMREGS
  mtrc_dm_attr_t *a = mtrc_dm_find(endpoint, cluster, attr);
  if (!a) return 0;
  if (out) *out = a->value;
  return 1;
}

MODULE_PART int mtrc_dm_set(uint16_t endpoint, uint32_t cluster, uint32_t attr, uint64_t value) { SETMEMREGS
  mtrc_dm_attr_t *a = mtrc_dm_find(endpoint, cluster, attr);
  if (!a) return 0;
  if (a->value == value) return 0;
  a->value = value;
  return 1;
}

MODULE_PART int mtrc_dm_endpoint_count(void) { SETMEMREGS return dm.ep_n; }
MODULE_PART const mtrc_dm_endpoint_t *mtrc_dm_endpoint_at(int i) { SETMEMREGS
  return (i >= 0 && i < dm.ep_n) ? &dm.ep[i] : NULL;
}
MODULE_PART int mtrc_dm_cluster_count(void) { SETMEMREGS return dm.cl_n; }
MODULE_PART const mtrc_dm_cluster_t *mtrc_dm_cluster_at(int i) { SETMEMREGS
  return (i >= 0 && i < dm.cl_n) ? &dm.cl[i] : NULL;
}
MODULE_PART int mtrc_dm_attr_count(void) { SETMEMREGS return dm.at_n; }
MODULE_PART const mtrc_dm_attr_t *mtrc_dm_attr_at(int i) { SETMEMREGS
  return (i >= 0 && i < dm.at_n) ? &dm.at[i] : NULL;
}

MODULE_PART int mtrc_dm_endpoint_device_type(uint16_t endpoint, uint32_t *out) { SETMEMREGS
  for (int i = 0; i < dm.ep_n; i++)
    if (dm.ep[i].endpoint == endpoint) { if (out) *out = dm.ep[i].device_type; return 1; }
  return 0;
}
