#ifndef __UTILS_H__
#define __UTILS_H__

static inline u32 read32(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldr\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write32(u32 addr, u32 data)
{
	__asm__ volatile ("str\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u32 set32(u32 addr, u32 set)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u32 clear32(u32 addr, u32 clear)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u32 mask32(u32 addr, u32 clear, u32 set)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

static inline u16 read16(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldrh\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write16(u32 addr, u16 data)
{
	__asm__ volatile ("strh\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u16 set16(u32 addr, u16 set)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u16 clear16(u32 addr, u16 clear)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u16 mask16(u32 addr, u16 clear, u16 set)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

static inline u8 read8(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldrb\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write8(u32 addr, u8 data)
{
	__asm__ volatile ("strb\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u8 set8(u32 addr, u8 set)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u8 clear8(u32 addr, u8 clear)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u8 mask8(u32 addr, u8 clear, u8 set)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

#define STACK_ALIGN(type, name, cnt, alignment)         \
u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) + \
(((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) - \
((sizeof(type)*(cnt))%(alignment))) : 0))]; \
type *name = (type*)(((u32)(_al__##name)) + ((alignment) - (( \
(u32)(_al__##name))&((alignment)-1))))

#define ATTRIBUTE_ALIGN(v)				__attribute__((aligned(v)))


/*
 * These functions are guaranteed to copy by reading from src and writing to dst in <n>-bit units
 * If size is not aligned, the remaining bytes are not copied
 */
void memset32(void *dst, u32 value, u32 size);
void memcpy32(void *dst, void *src, u32 size);
void memset16(void *dst, u16 value, u32 size);
void memcpy16(void *dst, void *src, u32 size);
void memset8(void *dst, u8 value, u32 size);
void memcpy8(void *dst, void *src, u32 size);

void hexdump(void *d, int len);
int sprintf(char *str, const char *fmt, ...);

#endif
