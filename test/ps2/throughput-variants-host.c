/* Host-only reference substitutes for MMI throughput candidates. */
#include <stdint.h>
#include <string.h>
extern void ee_mmi_add_quads(void *,const void *,unsigned int);
extern void ee_mmi_copy_quads(void *,const void *,unsigned int);
extern void ee_mmi_fill_quads(void *,const void *,unsigned int);
extern void ee_mmi_over_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_in_a8_quads(void *,const void *,unsigned int,const void *);
extern void ee_mmi_8888_to_0565_quads(void *,const void *,unsigned int,const void *);
#define SUBSTITUTE_3(suffix,base) \
void ee_mmi_##suffix##_quads(void *d,const void *s,unsigned int n){ \
    ee_mmi_##base##_quads(d,s,n); \
}
SUBSTITUTE_3(add_u2,add)
SUBSTITUTE_3(add_u8,add)
SUBSTITUTE_3(copy_u2,copy)
SUBSTITUTE_3(copy_u8,copy)
SUBSTITUTE_3(fill_u2,fill)
SUBSTITUTE_3(fill_u8,fill)
#define SUBSTITUTE_4(suffix,base) \
void ee_mmi_##suffix##_quads(void *d,const void *s,unsigned int n,const void *c){ \
    ee_mmi_##base##_quads(d,s,n,c); \
}
SUBSTITUTE_4(over_u2,over)
SUBSTITUTE_4(in_a8_predecode,in_a8)
SUBSTITUTE_4(8888_to_0565_u2,8888_to_0565)

extern void ee_mmi_over_8888_0565_quads(void *,const void *,unsigned int,const void *);
SUBSTITUTE_4(over_8888_0565_u2,over_8888_0565)
