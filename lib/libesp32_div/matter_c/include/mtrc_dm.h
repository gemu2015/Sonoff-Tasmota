// mtrc_dm.h — Matter data-model registry (endpoints / clusters / attributes).
//
// Phase C1 of ROADMAP_1.4: the C core becomes a *generic* Matter engine whose
// data model is described by a registry instead of a hardcoded if/else ladder.
// A host (xdrv glue) or, ultimately, a TinyC script declares endpoints, their
// clusters, and each cluster's attributes; the IM Read/Write/Subscribe paths
// and the Descriptor cluster all read this registry.
//
// Storage is fixed-capacity and static (no malloc) — embedded-friendly and
// deterministic. Values are kept as a single u64 cache (covers bool / enum /
// u8..u64, the wire forms the report engine emits today). Live values that the
// firmware owns (e.g. the real relay state) still come through the port's
// on_attr_read callback, which overrides the cached value at read time.
//
// Pure C, no crypto, no firmware globals — host-testable (test/build_dm.sh).
// GPLv3. Implemented from the Matter 1.4.1 spec (Core §7 Data Model, §9 IM).

#ifndef MTRC_DM_H
#define MTRC_DM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Capacity. Tunable; sized for a root node + a handful of app endpoints.
#define MTRC_DM_MAX_ENDPOINTS   8
#define MTRC_DM_MAX_CLUSTERS   32   // total across all endpoints
#define MTRC_DM_MAX_ATTRS      96   // total across all clusters

// Attribute storage/wire type. Kept as metadata for write parsing and the
// Descriptor cluster; the report engine emits all of these as a TLV uint.
typedef enum {
  MTRC_DM_T_BOOL = 0,
  MTRC_DM_T_U8,
  MTRC_DM_T_U16,
  MTRC_DM_T_U32,
  MTRC_DM_T_U64,
  MTRC_DM_T_ENUM8,
} mtrc_dm_type_t;

// Attribute flags.
#define MTRC_DM_F_WRITABLE  0x01   // controller may Write this attribute
#define MTRC_DM_F_LIVE      0x02   // value is owned by the host (on_attr_read)

typedef struct {
  uint16_t endpoint;
  uint32_t cluster;
  uint32_t attr;
  uint8_t  type;     // mtrc_dm_type_t
  uint8_t  flags;    // MTRC_DM_F_*
  uint64_t value;    // cached value
} mtrc_dm_attr_t;

typedef struct {
  uint16_t endpoint;
  uint32_t cluster;
} mtrc_dm_cluster_t;

typedef struct {
  uint16_t endpoint;
  uint32_t device_type;
} mtrc_dm_endpoint_t;

// ---- lifecycle ---------------------------------------------------------
// Clear the whole registry (used by matter_init / matter_factory_reset).
void mtrc_dm_reset(void);

// ---- declaration (host / script side) ----------------------------------
// Add an endpoint with a device-type id. Returns the endpoint index (>=0) or
// -1 if full / already present (idempotent: returns the existing index).
int mtrc_dm_add_endpoint(uint16_t endpoint, uint32_t device_type);

// Add a cluster to an endpoint. Returns 0 on success, -1 on full. Idempotent.
int mtrc_dm_add_cluster(uint16_t endpoint, uint32_t cluster);

// Add (or update the metadata of) an attribute, seeding its cached value.
// Auto-adds the parent cluster if needed. Returns 0 on success, -1 on full.
int mtrc_dm_add_attr(uint16_t endpoint, uint32_t cluster, uint32_t attr,
                     mtrc_dm_type_t type, uint8_t flags, uint64_t initial);

// ---- access ------------------------------------------------------------
// Find an attribute record (NULL if absent).
mtrc_dm_attr_t *mtrc_dm_find(uint16_t endpoint, uint32_t cluster, uint32_t attr);

// Read the cached value. Returns 1 + fills *out if present, else 0.
int mtrc_dm_get(uint16_t endpoint, uint32_t cluster, uint32_t attr, uint64_t *out);

// Update the cached value. Returns 1 if the attribute exists AND the value
// changed, 0 if unchanged or absent (caller uses the 1 to fire subscriptions).
int mtrc_dm_set(uint16_t endpoint, uint32_t cluster, uint32_t attr, uint64_t value);

// ---- enumeration (Descriptor cluster + wildcard reads) -----------------
int  mtrc_dm_endpoint_count(void);
const mtrc_dm_endpoint_t *mtrc_dm_endpoint_at(int i);
int  mtrc_dm_cluster_count(void);
const mtrc_dm_cluster_t  *mtrc_dm_cluster_at(int i);
int  mtrc_dm_attr_count(void);
const mtrc_dm_attr_t     *mtrc_dm_attr_at(int i);

// Look up an endpoint's device type. Returns 1 + fills *out if found, else 0.
int  mtrc_dm_endpoint_device_type(uint16_t endpoint, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif // MTRC_DM_H
