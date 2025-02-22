#ifndef __BYTE_ORDER_H__
#define __BYTE_ORDER_H__

#include <asm/byteorder.h>

/* taken from include/linux/byteorder/generic.h */
#define cpu_to_le64  __cpu_to_le64
#define le64_to_cpu  __le64_to_cpu
#define cpu_to_le32  __cpu_to_le32
#define le32_to_cpu  __le32_to_cpu
#define cpu_to_le16  __cpu_to_le16
#define le16_to_cpu  __le16_to_cpu
#define cpu_to_be64  __cpu_to_be64
#define be64_to_cpu  __be64_to_cpu
#define cpu_to_be32  __cpu_to_be32
#define be32_to_cpu  __be32_to_cpu
#define cpu_to_be16  __cpu_to_be16
#define be16_to_cpu  __be16_to_cpu

/* change in situ versions */
#define cpu_to_le64s __cpu_to_le64s
#define le64_to_cpus __le64_to_cpus
#define cpu_to_le32s __cpu_to_le32s
#define le32_to_cpus __le32_to_cpus
#define cpu_to_le16s __cpu_to_le16s
#define le16_to_cpus __le16_to_cpus
#define cpu_to_be64s __cpu_to_be64s
#define be64_to_cpus __be64_to_cpus
#define cpu_to_be32s __cpu_to_be32s
#define be32_to_cpus __be32_to_cpus
#define cpu_to_be16s __cpu_to_be16s
#define be16_to_cpus __be16_to_cpus

#endif
