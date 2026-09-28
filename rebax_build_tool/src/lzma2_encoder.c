/* Embedded public-domain 7-Zip LZMA2 encoder implementation. */
#include "lzma2_encoder.h"

/* ---- embedded source: Alloc.c ---- */
/* Alloc.c -- Memory allocation functions
: Igor Pavlov : Public domain */


#ifdef _WIN32
#endif
#include <stdlib.h>


#if defined(Z7_LARGE_PAGES) && defined(_WIN32) && \
    (!defined(Z7_WIN32_WINNT_MIN) || Z7_WIN32_WINNT_MIN < 0x0502)  // < Win2003 (xp-64)
  #define Z7_USE_DYN_GetLargePageMinimum
#endif

// for debug:
#if 0
#if defined(__CHERI__) && defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 16)
// #pragma message("=== Z7_ALLOC_NO_OFFSET_ALLOCATOR === ")
#define Z7_ALLOC_NO_OFFSET_ALLOCATOR
#endif
#endif

// #define SZ_ALLOC_DEBUG
/* use SZ_ALLOC_DEBUG to debug alloc/free operations */
#ifdef SZ_ALLOC_DEBUG

#include <string.h>
#include <stdio.h>
static int g_allocCount = 0;
#ifdef _WIN32
static int g_allocCountMid = 0;
#ifdef Z7_LARGE_PAGES
static int g_allocCountBig = 0;
#endif
#endif

#define CONVERT_INT_TO_STR(charType, tempSize) \
  char temp[tempSize]; unsigned i = 0; \
  while (val >= 10) { temp[i++] = (char)('0' + (unsigned)(val % 10)); val /= 10; } \
  *s++ = (charType)('0' + (unsigned)val); \
  while (i != 0) { i--; *s++ = temp[i]; } \
  *s = 0;

static void ConvertUInt64ToString(UInt64 val, char *s)
{
  CONVERT_INT_TO_STR(char, 24)
}

#define GET_HEX_CHAR(t) ((char)(((t < 10) ? ('0' + t) : ('A' + (t - 10)))))

static void ConvertUInt64ToHex(UInt64 val, char *s)
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)(val & 0xF);
    val >>= 4;
    s[--i] = GET_HEX_CHAR(t);
  }
  while (i);
}

#define DEBUG_OUT_STREAM stderr

static void Print(const char *s)
{
  fputs(s, DEBUG_OUT_STREAM);
}

static void PrintAligned(const char *s, size_t align)
{
  size_t len = strlen(s);
  for(;;)
  {
    fputc(' ', DEBUG_OUT_STREAM);
    if (len >= align)
      break;
    ++len;
  }
  Print(s);
}

static void PrintLn(void)
{
  Print("\n");
}

static void PrintHex(UInt64 v, size_t align)
{
  char s[32];
  ConvertUInt64ToHex(v, s);
  PrintAligned(s, align);
}

static void PrintDec(int v, size_t align)
{
  char s[32];
  ConvertUInt64ToString((unsigned)v, s);
  PrintAligned(s, align);
}

static void PrintAddr(void *p)
{
  PrintHex((UInt64)(size_t)(ptrdiff_t)p, 12);
}


#define PRINT_REALLOC(name, cnt, size, ptr) { \
    Print(name " "); \
    if (!ptr) PrintDec(cnt++, 10); \
    PrintHex(size, 10); \
    PrintAddr(ptr); \
    PrintLn(); }

#define PRINT_ALLOC(name, cnt, size, ptr) { \
    Print(name " "); \
    PrintDec(cnt++, 10); \
    PrintHex(size, 10); \
    PrintAddr(ptr); \
    PrintLn(); }
 
#define PRINT_FREE(name, cnt, ptr) if (ptr) { \
    Print(name " "); \
    PrintDec(--cnt, 10); \
    PrintAddr(ptr); \
    PrintLn(); }
 
#else

#ifdef _WIN32
#ifdef Z7_LARGE_PAGES
#define PRINT_ALLOC(name, cnt, size, ptr)
#endif
#endif
#define PRINT_FREE(name, cnt, ptr)
#define Print(s)
#define PrintLn()
#ifndef Z7_ALLOC_NO_OFFSET_ALLOCATOR
#define PrintHex(v, align)
#endif
#define PrintAddr(p)

#endif


/*
by specification:
  malloc(non_NULL, 0)   : returns NULL or a unique pointer value that can later be successfully passed to free()
  realloc(NULL, size)   : the call is equivalent to malloc(size)
  realloc(non_NULL, 0)  : the call is equivalent to free(ptr)

in main compilers:
  malloc(0)             : returns non_NULL
  realloc(NULL,     0)  : returns non_NULL
  realloc(non_NULL, 0)  : returns NULL
*/


void *MyAlloc(size_t size)
{
  if (size == 0)
    return NULL;
  // PRINT_ALLOC("Alloc    ", g_allocCount, size, NULL)
  #ifdef SZ_ALLOC_DEBUG
  {
    void *p = malloc(size);
    if (p)
    {
      PRINT_ALLOC("Alloc    ", g_allocCount, size, p)
    }
    return p;
  }
  #else
  return malloc(size);
  #endif
}

void MyFree(void *address)
{
  PRINT_FREE("Free    ", g_allocCount, address)
  
  free(address);
}

void *MyRealloc(void *address, size_t size)
{
  if (size == 0)
  {
    MyFree(address);
    return NULL;
  }
  // PRINT_REALLOC("Realloc  ", g_allocCount, size, address)
  #ifdef SZ_ALLOC_DEBUG
  {
    void *p = realloc(address, size);
    if (p)
    {
      PRINT_REALLOC("Realloc    ", g_allocCount, size, address)
    }
    return p;
  }
  #else
  return realloc(address, size);
  #endif
}


#ifdef _WIN32

void *MidAlloc(size_t size)
{
  if (size == 0)
    return NULL;
  #ifdef SZ_ALLOC_DEBUG
  {
    void *p = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (p)
    {
      PRINT_ALLOC("Alloc-Mid", g_allocCountMid, size, p)
    }
    return p;
  }
  #else
  return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
  #endif
}

void MidFree(void *address)
{
  PRINT_FREE("Free-Mid", g_allocCountMid, address)

  if (!address)
    return;
  VirtualFree(address, 0, MEM_RELEASE);
}

#ifdef Z7_LARGE_PAGES
// #pragma message("Z7_LARGE_PAGES")

#ifdef MEM_LARGE_PAGES
  #define MY_MEM_LARGE_PAGES  MEM_LARGE_PAGES
#else
  #define MY_MEM_LARGE_PAGES  0x20000000
#endif

extern
size_t g_LargePageSize;
size_t g_LargePageSize = 0;
extern
size_t g_LargePageThresholdMin;
size_t g_LargePageThresholdMin = 0;
extern
UInt32 g_LargePageFlags;
UInt32 g_LargePageFlags = 0;

void *BigAlloc(size_t size)
{
  if (size == 0)
    return NULL;

  PRINT_ALLOC("Alloc-Big", g_allocCountBig, size, NULL)

  #ifdef Z7_LARGE_PAGES
  {
    const size_t ps = g_LargePageSize - 1;
    if (ps < (1u << 30) && size > g_LargePageThresholdMin)
    {
      const size_t size2 = (size + ps) & ~ps;
      if (size2 >= size)
      {
        void *p = VirtualAlloc(NULL, size2, MEM_COMMIT | MY_MEM_LARGE_PAGES, PAGE_READWRITE);
        if (p)
        {
          PRINT_ALLOC("Alloc-BM ", g_allocCountMid, size2, p)
          return p;
        }
        if (g_LargePageFlags & Z7_LARGE_PAGES_FLAG_FAIL_STOP)
          return p;
      }
    }
  }
  #endif

  return MidAlloc(size);
}

void BigFree(void *address)
{
  PRINT_FREE("Free-Big", g_allocCountBig, address)
  MidFree(address);
}

#endif // Z7_LARGE_PAGES
#endif // _WIN32


static void *SzAlloc(ISzAllocPtr p, size_t size) { UNUSED_VAR(p)  return MyAlloc(size); }
static void SzFree(ISzAllocPtr p, void *address) { UNUSED_VAR(p)  MyFree(address); }
const ISzAlloc g_Alloc = { SzAlloc, SzFree };

#ifdef _WIN32
static void *SzMidAlloc(ISzAllocPtr p, size_t size) { UNUSED_VAR(p)  return MidAlloc(size); }
static void SzMidFree(ISzAllocPtr p, void *address) { UNUSED_VAR(p)  MidFree(address); }
const ISzAlloc g_MidAlloc = { SzMidAlloc, SzMidFree };
#endif

#if defined(Z7_LARGE_PAGES)
static void *SzBigAlloc(ISzAllocPtr p, size_t size) { UNUSED_VAR(p)  return BigAlloc(size); }
static void SzBigFree(ISzAllocPtr p, void *address) { UNUSED_VAR(p)  BigFree(address); }
const ISzAlloc g_BigAlloc = { SzBigAlloc, SzBigFree };
#endif

#ifndef Z7_ALLOC_NO_OFFSET_ALLOCATOR

#define ADJUST_ALLOC_SIZE 0
/*
#define ADJUST_ALLOC_SIZE (sizeof(void *) - 1)
*/
/*
  Use (ADJUST_ALLOC_SIZE = (sizeof(void *) - 1)), if
     MyAlloc() can return address that is NOT multiple of sizeof(void *).
*/

/*
  uintptr_t : <stdint.h> C99 (optional)
            : unsupported in VS6
*/
typedef
  #ifdef _WIN32
    UINT_PTR
  #elif 1
    uintptr_t
  #else
    ptrdiff_t
  #endif
    MY_uintptr_t;

#if 0 \
    || (defined(__CHERI__) \
    || defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ > 8))
// for 128-bit pointers (cheri):
#define MY_ALIGN_PTR_DOWN(p, align)  \
    ((void *)((char *)(p) - ((size_t)(MY_uintptr_t)(p) & ((align) - 1))))
#else
#define MY_ALIGN_PTR_DOWN(p, align) \
    ((void *)((((MY_uintptr_t)(p)) & ~((MY_uintptr_t)(align) - 1))))
#endif

#endif

#ifndef _WIN32
#include <unistd.h> // for _POSIX_ADVISORY_INFO : for some linux
#if (defined(Z7_ALLOC_NO_OFFSET_ALLOCATOR) \
        || defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L) \
        || defined(_POSIX_ADVISORY_INFO) && (_POSIX_ADVISORY_INFO >= 200112L) \
        || defined(__APPLE__) \
        /* || defined(__linux__) */)
  #define USE_posix_memalign
  // #pragma message("USE_posix_memalign")
#endif
#endif

#ifndef USE_posix_memalign
#define MY_ALIGN_PTR_UP_PLUS(p, align) MY_ALIGN_PTR_DOWN(((char *)(p) + (align) + ADJUST_ALLOC_SIZE), align)
#endif

/*
  This posix_memalign() is for test purposes only.
  We also need special Free() function instead of free(),
  if this posix_memalign() is used.
*/

/*
static int posix_memalign(void **ptr, size_t align, size_t size)
{
  size_t newSize = size + align;
  void *p;
  void *pAligned;
  *ptr = NULL;
  if (newSize < size)
    return 12; // ENOMEM
  p = MyAlloc(newSize);
  if (!p)
    return 12; // ENOMEM
  pAligned = MY_ALIGN_PTR_UP_PLUS(p, align);
  ((void **)pAligned)[-1] = p;
  *ptr = pAligned;
  return 0;
}
*/

/*
  ALLOC_ALIGN_SIZE >= sizeof(void *)
  ALLOC_ALIGN_SIZE >= cache_line_size
*/

#define ALLOC_ALIGN_SIZE ((size_t)1 << 7)

void *z7_AlignedAlloc(size_t size)
{
#ifndef USE_posix_memalign
  
  void *p;
  void *pAligned;
  size_t newSize;

  /* also we can allocate additional dummy ALLOC_ALIGN_SIZE bytes after aligned
     block to prevent cache line sharing with another allocated blocks */

  newSize = size + ALLOC_ALIGN_SIZE * 1 + ADJUST_ALLOC_SIZE;
  if (newSize < size)
    return NULL;

  p = MyAlloc(newSize);
  
  if (!p)
    return NULL;
  pAligned = MY_ALIGN_PTR_UP_PLUS(p, ALLOC_ALIGN_SIZE);

  Print(" size="); PrintHex(size, 8);
  Print(" a_size="); PrintHex(newSize, 8);
  Print(" ptr="); PrintAddr(p);
  Print(" a_ptr="); PrintAddr(pAligned);
  PrintLn();

  ((void **)pAligned)[-1] = p;

  return pAligned;

#else

  void *p;
  if (posix_memalign(&p, ALLOC_ALIGN_SIZE, size))
    return NULL;

  Print(" posix_memalign="); PrintAddr(p);
  PrintLn();

  return p;

#endif
}


void z7_AlignedFree(void *address)
{
#ifndef USE_posix_memalign
  if (address)
    MyFree(((void **)address)[-1]);
#else
  free(address);
#endif
}


static void *SzAlignedAlloc(ISzAllocPtr pp, size_t size)
{
  UNUSED_VAR(pp)
  return z7_AlignedAlloc(size);
}


static void SzAlignedFree(ISzAllocPtr pp, void *address)
{
  UNUSED_VAR(pp)
#ifndef USE_posix_memalign
  if (address)
    MyFree(((void **)address)[-1]);
#else
  free(address);
#endif
}

#ifndef _WIN32

#ifdef Z7_LARGE_PAGES

#if 0 // 1 for debug
  #include <stdio.h>
  #include <string.h>  // for strerror()
  #define PRF(x) x
#else
  #define PRF(x)
#endif

#ifdef USE_posix_memalign
  /* madvise():
     glibc <= 2.19 : _BSD_SOURCE
     glibc  > 2.19 : _DEFAULT_SOURCE
  */
  /* && (defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE)) */
#if 1 && !defined(Z7_NO_MADVISE) && \
  (defined(__linux__) || defined(__unix__) || defined(__APPLE__))
#include <sys/mman.h> // for madvise
// #pragma message("sys/mman.h")
#if (defined(MADV_HUGEPAGE) && defined(MADV_NOHUGEPAGE))
  #define Z7_USE_BIG_ALLOC_MADVISE
  // #pragma message("Z7_USE_BIG_ALLOC_MADVISE")
#endif
#endif
#endif // USE_posix_memalign

#ifdef Z7_USE_BIG_ALLOC_MADVISE
#define LARGE_PAGE_SIZE_DEFAULT (1 << 21)
#else
#define LARGE_PAGE_SIZE_DEFAULT 0
#endif

extern
size_t g_LargePageSize;
size_t g_LargePageSize = LARGE_PAGE_SIZE_DEFAULT;
extern
size_t g_LargePageThresholdMin;
size_t g_LargePageThresholdMin = LARGE_PAGE_SIZE_DEFAULT / 2;
extern
UInt32 g_LargePageFlags;
UInt32 g_LargePageFlags = 0;

void *BigAlloc(size_t size)
{
  if (size == 0)
    return NULL;
#ifdef USE_posix_memalign
  {
    const size_t pageSize = g_LargePageSize;
    void *buf = NULL; // on Linux (and other systems), posix_memalign() does not modify memptr on failure (POSIX.1-2008 TC2).
    PRF(printf("\nBigAlloc 0x%08x=%5uMB", (unsigned)(size), (unsigned)(size >> 20));)
    if (pageSize && size > g_LargePageThresholdMin)
    {
      int res;
      const size_t mask = pageSize - 1;
      /* we can allocate aligned size, so data at the end of buffer also will use huge page
         if (size2 for madvise() is not aligned for huge page size)
           { Last data block will use small pages. It reduces memory allocation,
             but last data block with small pages can work slower.
             It's useful, if we have very large HUGE_PAGE: 32MB or 512MB. }
      */
      size_t size2 = (size + mask) & ~mask;
      if (size2 < size || (size & mask) <= g_LargePageThresholdMin)
        size2 = size;
      res = posix_memalign(&buf, pageSize, size2);
      PRF(printf(" posix_memalign size=0x%08x=%5uMB align=%u",
          (unsigned)(size2), (unsigned)(size2 >> 20), (unsigned)pageSize);)
      PRF(printf(" buf=%p", (void *)buf);)
      if (res == 0)
      {
#ifdef Z7_USE_BIG_ALLOC_MADVISE
        if ((g_LargePageFlags & Z7_LARGE_PAGES_FLAG_NO_MADVISE) == 0)
        {
          // Advise the kernel to use huge pages for this memory range
          // MADV_HUGEPAGE / MADV_NOHUGEPAGE : since Linux 2.6.38
          // madvise() only operates on whole pages, therefore addr must be page-aligned (4KB/8KB/16KB/64KB).
          // The value of size is rounded up to a multiple of page size.
          PRF(printf(" madvise g_LargePageFlags=%x", (unsigned)g_LargePageFlags);)
          res = madvise(buf, size2, (g_LargePageFlags & Z7_LARGE_PAGES_FLAG_NO_HUGEPAGE) ? MADV_NOHUGEPAGE : MADV_HUGEPAGE);
          if (res)
          {
            PRF(printf("\nERROR res=%d, errno=%d=%s\n", res, (int)errno, strerror(errno));)
            if (g_LargePageFlags & Z7_LARGE_PAGES_FLAG_FAIL_STOP)
            {
              free(buf);
              return NULL;
            }
          }
        }
#endif // Z7_USE_BIG_ALLOC_MADVISE
        PRF(printf("\n");)
        return buf;
      }
      PRF(printf("\nERROR res=%d=%s\n", res, strerror(res));)
      if (g_LargePageFlags & Z7_LARGE_PAGES_FLAG_FAIL_STOP)
        return NULL;
      // (res == ENOMEM) "Out of memory" is possible, if pageSize is too big.
      // so we do second attempt with smaller alignment
    }
  }
#endif // !USE_posix_memalign
  PRF(printf(" z7_AlignedAlloc size=0x%08x=%5uMB\n", (unsigned)(size), (unsigned)(size >> 20));)
  return z7_AlignedAlloc(size);
}


void BigFree(void *address)
{
  z7_AlignedFree(address);
}
#endif // Z7_LARGE_PAGES
#endif // !_WIN32


#ifdef Z7_LARGE_PAGES
void z7_LargePage_Set(UInt32 flags, size_t pageSize, size_t threshold)
{
  g_LargePageFlags = flags;

#ifdef _WIN32
  if ((flags & Z7_LARGE_PAGES_FLAG_USE_HUGEPAGE) == 0)
  {
    g_LargePageSize = 0;
    g_LargePageThresholdMin = 0;
  }
  else
  {
    if ((flags & Z7_LARGE_PAGES_FLAG_DIRECT_PAGE_SIZE) == 0)
    {
#ifdef Z7_USE_DYN_GetLargePageMinimum
      Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION
typedef SIZE_T (WINAPI *Func_GetLargePageMinimum)(VOID);
      const
        Func_GetLargePageMinimum fn =
       (Func_GetLargePageMinimum) Z7_CAST_FUNC_C GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
            "GetLargePageMinimum");
      if (fn)
        pageSize = fn();
      else
        pageSize = 0;
#else
      pageSize = GetLargePageMinimum();
#endif
      if (pageSize & (pageSize - 1))
        pageSize = 0;
    }
    g_LargePageSize = pageSize;
    if ((flags & Z7_LARGE_PAGES_FLAG_DIRECT_THRESHOLD) == 0)
      threshold = pageSize / 2;
    g_LargePageThresholdMin = threshold;
  }

#else // !_WIN32

  if (flags & Z7_LARGE_PAGES_FLAG_NO_PAGECODE)
  {
    g_LargePageSize = 0;
    g_LargePageThresholdMin = 0;
  }
  else
  {
    if ((flags & Z7_LARGE_PAGES_FLAG_DIRECT_PAGE_SIZE) == 0)
      pageSize = LARGE_PAGE_SIZE_DEFAULT;
    g_LargePageSize = pageSize;
    if ((flags & Z7_LARGE_PAGES_FLAG_DIRECT_THRESHOLD) == 0)
      threshold = pageSize / 2;
    g_LargePageThresholdMin = threshold;
  }
  // PRF(printf("\ng_LargePageSize=%x g_LargePageThresholdMin = %x g_LargePageFlags = %x", (unsigned)g_LargePageSize, (unsigned)g_LargePageThresholdMin, (unsigned)g_LargePageFlags);)
#endif // !_WIN32
}
#endif // Z7_LARGE_PAGES

const ISzAlloc g_AlignedAlloc = { SzAlignedAlloc, SzAlignedFree };



/* we align ptr to support cases where CAlignOffsetAlloc::offset is not multiply of sizeof(void *) */
#ifndef Z7_ALLOC_NO_OFFSET_ALLOCATOR
#if 1
  #define MY_ALIGN_PTR_DOWN_1(p)  MY_ALIGN_PTR_DOWN(p, sizeof(void *))
  #define REAL_BLOCK_PTR_VAR(p)  ((void **)MY_ALIGN_PTR_DOWN_1(p))[-1]
#else
  // we can use this simplified code,
  // if (CAlignOffsetAlloc::offset == (k * sizeof(void *))
  #define REAL_BLOCK_PTR_VAR(p)  (((void **)(p))[-1])
#endif
#endif


#if 0
#ifndef Z7_ALLOC_NO_OFFSET_ALLOCATOR
#include <stdio.h>
static void PrintPtr(const char *s, const void *p)
{
  const Byte *p2 = (const Byte *)&p;
  unsigned i;
  printf("%s %p ", s, p);
  for (i = sizeof(p); i != 0;)
  {
    i--;
    printf("%02x", p2[i]);
  }
  printf("\n");
}
#endif
#endif


static void *AlignOffsetAlloc_Alloc(ISzAllocPtr pp, size_t size)
{
#if defined(Z7_ALLOC_NO_OFFSET_ALLOCATOR)
  UNUSED_VAR(pp)
  return z7_AlignedAlloc(size);
#else
  const CAlignOffsetAlloc *p = Z7_CONTAINER_FROM_VTBL_CONST(pp, CAlignOffsetAlloc, vt);
  void *adr;
  void *pAligned;
  size_t newSize;
  size_t extra;
  size_t alignSize = (size_t)1 << p->numAlignBits;

  if (alignSize < sizeof(void *))
    alignSize = sizeof(void *);
  
  if (p->offset >= alignSize)
    return NULL;

  /* also we can allocate additional dummy ALLOC_ALIGN_SIZE bytes after aligned
     block to prevent cache line sharing with another allocated blocks */
  extra = p->offset & (sizeof(void *) - 1);
  newSize = size + alignSize + extra + ADJUST_ALLOC_SIZE;
  if (newSize < size)
    return NULL;

  adr = ISzAlloc_Alloc(p->baseAlloc, newSize);
  
  if (!adr)
    return NULL;

  pAligned = (char *)MY_ALIGN_PTR_DOWN((char *)adr +
      alignSize - p->offset + extra + ADJUST_ALLOC_SIZE, alignSize) + p->offset;

#if 0
  printf("\nalignSize = %6x, offset=%6x, size=%8x \n", (unsigned)alignSize, (unsigned)p->offset, (unsigned)size);
  PrintPtr("base", adr);
  PrintPtr("alig", pAligned);
#endif

  PrintLn();
  Print("- Aligned: ");
  Print(" size="); PrintHex(size, 8);
  Print(" a_size="); PrintHex(newSize, 8);
  Print(" ptr="); PrintAddr(adr);
  Print(" a_ptr="); PrintAddr(pAligned);
  PrintLn();

  REAL_BLOCK_PTR_VAR(pAligned) = adr;

  return pAligned;
#endif
}


static void AlignOffsetAlloc_Free(ISzAllocPtr pp, void *address)
{
#if defined(Z7_ALLOC_NO_OFFSET_ALLOCATOR)
  UNUSED_VAR(pp)
  z7_AlignedFree(address);
#else
  if (address)
  {
    const CAlignOffsetAlloc *p = Z7_CONTAINER_FROM_VTBL_CONST(pp, CAlignOffsetAlloc, vt);
    PrintLn();
    Print("- Aligned Free: ");
    PrintLn();
    ISzAlloc_Free(p->baseAlloc, REAL_BLOCK_PTR_VAR(address));
  }
#endif
}


void AlignOffsetAlloc_CreateVTable(CAlignOffsetAlloc *p)
{
  p->vt.Alloc = AlignOffsetAlloc_Alloc;
  p->vt.Free = AlignOffsetAlloc_Free;
}

/* ---- embedded source: CpuArch.c ---- */
/* CpuArch.c -- CPU specific code
Igor Pavlov : Public domain */


// #include <stdio.h>


#ifdef MY_CPU_X86_OR_AMD64

#undef NEED_CHECK_FOR_CPUID
#if !defined(MY_CPU_AMD64)
#define NEED_CHECK_FOR_CPUID
#endif

/*
  cpuid instruction supports (subFunction) parameter in ECX,
  that is used only with some specific (function) parameter values.
  most functions use only (subFunction==0).
*/
/*
  __cpuid(): MSVC and GCC/CLANG use same function/macro name
             but parameters are different.
   We use MSVC __cpuid() parameters style for our z7_x86_cpuid() function.
*/

#if defined(__GNUC__) /* && (__GNUC__ >= 10) */ \
    || defined(__clang__) /* && (__clang_major__ >= 10) */

/* there was some CLANG/GCC compilers that have issues with
   rbx(ebx) handling in asm blocks in -fPIC mode (__PIC__ is defined).
   compiler's <cpuid.h> contains the macro __cpuid() that is similar to our code.
   The history of __cpuid() changes in CLANG/GCC:
   GCC:
     2007: it preserved ebx for (__PIC__ && __i386__)
     2013: it preserved rbx and ebx for __PIC__
     2014: it doesn't preserves rbx and ebx anymore
     we suppose that (__GNUC__ >= 5) fixed that __PIC__ ebx/rbx problem.
   CLANG:
     2014+: it preserves rbx, but only for 64-bit code. No __PIC__ check.
   Why CLANG cares about 64-bit mode only, and doesn't care about ebx (in 32-bit)?
   Do we need __PIC__ test for CLANG or we must care about rbx even if
   __PIC__ is not defined?
*/

#define ASM_LN "\n"
   
#if defined(MY_CPU_AMD64) && defined(__PIC__) \
    && ((defined (__GNUC__) && (__GNUC__ < 5)) || defined(__clang__))

  /* "=&r" selects free register. It can select even rbx, if that register is free.
     "=&D" for (RDI) also works, but the code can be larger with "=&D"
     "2"(subFun) : 2 is (zero-based) index in the output constraint list "=c" (ECX). */

#define x86_cpuid_MACRO_2(p, func, subFunc) { \
  __asm__ __volatile__ ( \
    ASM_LN   "mov     %%rbx, %q1"  \
    ASM_LN   "cpuid"               \
    ASM_LN   "xchg    %%rbx, %q1"  \
    : "=a" ((p)[0]), "=&r" ((p)[1]), "=c" ((p)[2]), "=d" ((p)[3]) : "0" (func), "2"(subFunc)); }

#elif defined(MY_CPU_X86) && defined(__PIC__) \
    && ((defined (__GNUC__) && (__GNUC__ < 5)) || defined(__clang__))

#define x86_cpuid_MACRO_2(p, func, subFunc) { \
  __asm__ __volatile__ ( \
    ASM_LN   "mov     %%ebx, %k1"  \
    ASM_LN   "cpuid"               \
    ASM_LN   "xchg    %%ebx, %k1"  \
    : "=a" ((p)[0]), "=&r" ((p)[1]), "=c" ((p)[2]), "=d" ((p)[3]) : "0" (func), "2"(subFunc)); }

#else

#define x86_cpuid_MACRO_2(p, func, subFunc) { \
  __asm__ __volatile__ ( \
    ASM_LN   "cpuid"               \
    : "=a" ((p)[0]), "=b" ((p)[1]), "=c" ((p)[2]), "=d" ((p)[3]) : "0" (func), "2"(subFunc)); }

#endif

#define x86_cpuid_MACRO(p, func)  x86_cpuid_MACRO_2(p, func, 0)

void Z7_FASTCALL z7_x86_cpuid(UInt32 p[4], UInt32 func)
{
  x86_cpuid_MACRO(p, func)
}

static
void Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
  x86_cpuid_MACRO_2(p, func, subFunc)
}


Z7_NO_INLINE
UInt32 Z7_FASTCALL z7_x86_cpuid_GetMaxFunc(void)
{
 #if defined(NEED_CHECK_FOR_CPUID)
  #define EFALGS_CPUID_BIT 21
  UInt32 a;
  __asm__ __volatile__ (
    ASM_LN   "pushf"
    ASM_LN   "pushf"
    ASM_LN   "pop     %0"
    // ASM_LN   "movl    %0, %1"
    // ASM_LN   "xorl    $0x200000, %0"
    ASM_LN   "btc     %1, %0"
    ASM_LN   "push    %0"
    ASM_LN   "popf"
    ASM_LN   "pushf"
    ASM_LN   "pop     %0"
    ASM_LN   "xorl    (%%esp), %0"

    ASM_LN   "popf"
    ASM_LN
    : "=&r" (a) // "=a"
    : "i" (EFALGS_CPUID_BIT)
    );
  if ((a & (1 << EFALGS_CPUID_BIT)) == 0)
    return 0;
 #endif
  {
    UInt32 p[4];
    x86_cpuid_MACRO(p, 0)
    return p[0];
  }
}

#undef ASM_LN

#elif !defined(_MSC_VER)

/*
// for gcc/clang and other: we can try to use __cpuid macro:
#include <cpuid.h>
void Z7_FASTCALL z7_x86_cpuid(UInt32 p[4], UInt32 func)
{
  __cpuid(func, p[0], p[1], p[2], p[3]);
}
UInt32 Z7_FASTCALL z7_x86_cpuid_GetMaxFunc(void)
{
  return (UInt32)__get_cpuid_max(0, NULL);
}
*/
// for unsupported cpuid:
void Z7_FASTCALL z7_x86_cpuid(UInt32 p[4], UInt32 func)
{
  UNUSED_VAR(func)
  p[0] = p[1] = p[2] = p[3] = 0;
}
UInt32 Z7_FASTCALL z7_x86_cpuid_GetMaxFunc(void)
{
  return 0;
}

#else // _MSC_VER

#if !defined(MY_CPU_AMD64)

UInt32 __declspec(naked) Z7_FASTCALL z7_x86_cpuid_GetMaxFunc(void)
{
  #if defined(NEED_CHECK_FOR_CPUID)
  #define EFALGS_CPUID_BIT 21
  __asm   pushfd
  __asm   pushfd
  /*
  __asm   pop     eax
  // __asm   mov     edx, eax
  __asm   btc     eax, EFALGS_CPUID_BIT
  __asm   push    eax
  */
  __asm   btc     dword ptr [esp], EFALGS_CPUID_BIT
  __asm   popfd
  __asm   pushfd
  __asm   pop     eax
  // __asm   xor     eax, edx
  __asm   xor     eax, [esp]
  // __asm   push    edx
  __asm   popfd
  __asm   and     eax, (1 shl EFALGS_CPUID_BIT)
  __asm   jz end_func
  #endif
  __asm   push    ebx
  __asm   xor     eax, eax    // func
  __asm   xor     ecx, ecx    // subFunction (optional) for (func == 0)
  __asm   cpuid
  __asm   pop     ebx
  #if defined(NEED_CHECK_FOR_CPUID)
  end_func:
  #endif
  __asm   ret 0
}

void __declspec(naked) Z7_FASTCALL z7_x86_cpuid(UInt32 p[4], UInt32 func)
{
  UNUSED_VAR(p)
  UNUSED_VAR(func)
  __asm   push    ebx
  __asm   push    edi
  __asm   mov     edi, ecx    // p
  __asm   mov     eax, edx    // func
  __asm   xor     ecx, ecx    // subfunction (optional) for (func == 0)
  __asm   cpuid
  __asm   mov     [edi     ], eax
  __asm   mov     [edi +  4], ebx
  __asm   mov     [edi +  8], ecx
  __asm   mov     [edi + 12], edx
  __asm   pop     edi
  __asm   pop     ebx
  __asm   ret     0
}

static
void __declspec(naked) Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
  UNUSED_VAR(p)
  UNUSED_VAR(func)
  UNUSED_VAR(subFunc)
  __asm   push    ebx
  __asm   push    edi
  __asm   mov     edi, ecx    // p
  __asm   mov     eax, edx    // func
  __asm   mov     ecx, [esp + 12]  // subFunc
  __asm   cpuid
  __asm   mov     [edi     ], eax
  __asm   mov     [edi +  4], ebx
  __asm   mov     [edi +  8], ecx
  __asm   mov     [edi + 12], edx
  __asm   pop     edi
  __asm   pop     ebx
  __asm   ret     4
}

#else // MY_CPU_AMD64

    #if _MSC_VER >= 1600
      #include <intrin.h>
      #define MY_cpuidex  __cpuidex

static
void Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
  __cpuidex((int *)p, func, subFunc);
}

    #else
/*
 __cpuid (func == (0 or 7)) requires subfunction number in ECX.
  MSDN: The __cpuid intrinsic clears the ECX register before calling the cpuid instruction.
   __cpuid() in new MSVC clears ECX.
   __cpuid() in old MSVC (14.00) x64 doesn't clear ECX
 We still can use __cpuid for low (func) values that don't require ECX,
 but __cpuid() in old MSVC will be incorrect for some func values: (func == 7).
 So here we use the hack for old MSVC to send (subFunction) in ECX register to cpuid instruction,
 where ECX value is first parameter for FASTCALL / NO_INLINE func.
 So the caller of MY_cpuidex_HACK() sets ECX as subFunction, and
 old MSVC for __cpuid() doesn't change ECX and cpuid instruction gets (subFunction) value.
 
DON'T remove Z7_NO_INLINE and Z7_FASTCALL for MY_cpuidex_HACK(): !!!
*/
static
Z7_NO_INLINE void Z7_FASTCALL MY_cpuidex_HACK(Int32 subFunction, Int32 func, Int32 *CPUInfo)
{
  UNUSED_VAR(subFunction)
  __cpuid(CPUInfo, func);
}
      #define MY_cpuidex(info, func, func2)  MY_cpuidex_HACK(func2, func, info)
      #pragma message("======== MY_cpuidex_HACK WAS USED ========")
static
void Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
  MY_cpuidex_HACK(subFunc, func, (Int32 *)p);
}
    #endif // _MSC_VER >= 1600

#if !defined(MY_CPU_AMD64)
/* inlining for __cpuid() in MSVC x86 (32-bit) produces big ineffective code,
   so we disable inlining here */
Z7_NO_INLINE
#endif
void Z7_FASTCALL z7_x86_cpuid(UInt32 p[4], UInt32 func)
{
  MY_cpuidex((Int32 *)p, (Int32)func, 0);
}

Z7_NO_INLINE
UInt32 Z7_FASTCALL z7_x86_cpuid_GetMaxFunc(void)
{
  Int32 a[4];
  MY_cpuidex(a, 0, 0);
  return a[0];
}

#endif // MY_CPU_AMD64
#endif // _MSC_VER

#if defined(NEED_CHECK_FOR_CPUID)
#define CHECK_CPUID_IS_SUPPORTED { if (z7_x86_cpuid_GetMaxFunc() == 0) return 0; }
#else
#define CHECK_CPUID_IS_SUPPORTED
#endif
#undef NEED_CHECK_FOR_CPUID


static
BoolInt x86cpuid_Func_1(UInt32 *p)
{
  CHECK_CPUID_IS_SUPPORTED
  z7_x86_cpuid(p, 1);
  return True;
}

/*
static const UInt32 kVendors[][1] =
{
  { 0x756E6547 }, // , 0x49656E69, 0x6C65746E },
  { 0x68747541 }, // , 0x69746E65, 0x444D4163 },
  { 0x746E6543 }  // , 0x48727561, 0x736C7561 }
};
*/

/*
typedef struct
{
  UInt32 maxFunc;
  UInt32 vendor[3];
  UInt32 ver;
  UInt32 b;
  UInt32 c;
  UInt32 d;
} Cx86cpuid;

enum
{
  CPU_FIRM_INTEL,
  CPU_FIRM_AMD,
  CPU_FIRM_VIA
};
int x86cpuid_GetFirm(const Cx86cpuid *p);
#define x86cpuid_ver_GetFamily(ver) (((ver >> 16) & 0xff0) | ((ver >> 8) & 0xf))
#define x86cpuid_ver_GetModel(ver)  (((ver >> 12) &  0xf0) | ((ver >> 4) & 0xf))
#define x86cpuid_ver_GetStepping(ver) (ver & 0xf)

int x86cpuid_GetFirm(const Cx86cpuid *p)
{
  unsigned i;
  for (i = 0; i < sizeof(kVendors) / sizeof(kVendors[0]); i++)
  {
    const UInt32 *v = kVendors[i];
    if (v[0] == p->vendor[0]
        // && v[1] == p->vendor[1]
        // && v[2] == p->vendor[2]
        )
      return (int)i;
  }
  return -1;
}

BoolInt CPU_Is_InOrder()
{
  Cx86cpuid p;
  UInt32 family, model;
  if (!x86cpuid_CheckAndRead(&p))
    return True;

  family = x86cpuid_ver_GetFamily(p.ver);
  model = x86cpuid_ver_GetModel(p.ver);

  switch (x86cpuid_GetFirm(&p))
  {
    case CPU_FIRM_INTEL: return (family < 6 || (family == 6 && (
        // In-Order Atom CPU
           model == 0x1C  // 45 nm, N4xx, D4xx, N5xx, D5xx, 230, 330
        || model == 0x26  // 45 nm, Z6xx
        || model == 0x27  // 32 nm, Z2460
        || model == 0x35  // 32 nm, Z2760
        || model == 0x36  // 32 nm, N2xxx, D2xxx
        )));
    case CPU_FIRM_AMD: return (family < 5 || (family == 5 && (model < 6 || model == 0xA)));
    case CPU_FIRM_VIA: return (family < 6 || (family == 6 && model < 0xF));
  }
  return False; // v23 : unknown processors are not In-Order
}
*/

#ifdef _WIN32
#endif

#if !defined(MY_CPU_AMD64) && defined(_WIN32)

/* for legacy SSE ia32: there is no user-space cpu instruction to check
   that OS supports SSE register storing/restoring on context switches.
   So we need some OS-specific function to check that it's safe to use SSE registers.
*/

Z7_FORCE_INLINE
static BoolInt CPU_Sys_Is_SSE_Supported(void)
{
#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable : 4996) // `GetVersion': was declared deprecated
#endif
  /* low byte is major version of Windows
     We suppose that any Windows version since
     Windows2000 (major == 5) supports SSE registers */
  return (Byte)GetVersion() >= 5;
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif
}
#define CHECK_SYS_SSE_SUPPORT if (!CPU_Sys_Is_SSE_Supported()) return False;
#else
#define CHECK_SYS_SSE_SUPPORT
#endif


#if !defined(MY_CPU_AMD64)

BoolInt CPU_IsSupported_CMOV(void)
{
  UInt32 a[4];
  if (!x86cpuid_Func_1(&a[0]))
    return 0;
  return (BoolInt)(a[3] >> 15) & 1;
}

BoolInt CPU_IsSupported_SSE(void)
{
  UInt32 a[4];
  CHECK_SYS_SSE_SUPPORT
  if (!x86cpuid_Func_1(&a[0]))
    return 0;
  return (BoolInt)(a[3] >> 25) & 1;
}

BoolInt CPU_IsSupported_SSE2(void)
{
  UInt32 a[4];
  CHECK_SYS_SSE_SUPPORT
  if (!x86cpuid_Func_1(&a[0]))
    return 0;
  return (BoolInt)(a[3] >> 26) & 1;
}

#endif


static UInt32 x86cpuid_Func_1_ECX(void)
{
  UInt32 a[4];
  CHECK_SYS_SSE_SUPPORT
  if (!x86cpuid_Func_1(&a[0]))
    return 0;
  return a[2];
}

BoolInt CPU_IsSupported_AES(void)
{
  return (BoolInt)(x86cpuid_Func_1_ECX() >> 25) & 1;
}

BoolInt CPU_IsSupported_SSSE3(void)
{
  return (BoolInt)(x86cpuid_Func_1_ECX() >> 9) & 1;
}

BoolInt CPU_IsSupported_SSE41(void)
{
  return (BoolInt)(x86cpuid_Func_1_ECX() >> 19) & 1;
}

BoolInt CPU_IsSupported_SHA(void)
{
  CHECK_SYS_SSE_SUPPORT

  if (z7_x86_cpuid_GetMaxFunc() < 7)
    return False;
  {
    UInt32 d[4];
    z7_x86_cpuid(d, 7);
    return (BoolInt)(d[1] >> 29) & 1;
  }
}


BoolInt CPU_IsSupported_SHA512(void)
{
  if (!CPU_IsSupported_AVX2()) return False; // maybe CPU_IsSupported_AVX() is enough here

  if (z7_x86_cpuid_GetMaxFunc() < 7)
    return False;
  {
    UInt32 d[4];
    z7_x86_cpuid_subFunc(d, 7, 0);
    if (d[0] < 1) // d[0] - is max supported subleaf value
      return False;
    z7_x86_cpuid_subFunc(d, 7, 1);
    return (BoolInt)(d[0]) & 1;
  }
}

/*
MSVC: _xgetbv() intrinsic is available since VS2010SP1.
   MSVC also defines (_XCR_XFEATURE_ENABLED_MASK) macro in
   <immintrin.h> that we can use or check.
   For any 32-bit x86 we can use asm code in MSVC,
   but MSVC asm code is huge after compilation.
   So _xgetbv() is better

ICC: _xgetbv() intrinsic is available (in what version of ICC?)
   ICC defines (__GNUC___) and it supports gnu assembler
   also ICC supports MASM style code with -use-msasm switch.
   but ICC doesn't support __attribute__((__target__))

GCC/CLANG 9:
  _xgetbv() is macro that works via __builtin_ia32_xgetbv()
  and we need __attribute__((__target__("xsave")).
  But with __target__("xsave") the function will be not
  inlined to function that has no __target__("xsave") attribute.
  If we want _xgetbv() call inlining, then we should use asm version
  instead of calling _xgetbv().
  Note:intrinsic is broke before GCC 8.2:
    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85684
*/

#if    defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 1100) \
    || defined(_MSC_VER) && (_MSC_VER >= 1600) && (_MSC_FULL_VER >= 160040219)  \
    || defined(__GNUC__) && (__GNUC__ >= 9) \
    || defined(__clang__) && (__clang_major__ >= 9)
// we define ATTRIB_XGETBV, if we want to use predefined _xgetbv() from compiler
#if defined(__INTEL_COMPILER)
#define ATTRIB_XGETBV
#elif defined(__GNUC__) || defined(__clang__)
// we don't define ATTRIB_XGETBV here, because asm version is better for inlining.
// #define ATTRIB_XGETBV __attribute__((__target__("xsave")))
#else
#define ATTRIB_XGETBV
#endif
#endif

#if defined(ATTRIB_XGETBV)
#include <immintrin.h>
#endif


// XFEATURE_ENABLED_MASK/XCR0
#define MY_XCR_XFEATURE_ENABLED_MASK 0

#if defined(ATTRIB_XGETBV)
ATTRIB_XGETBV
#endif
static UInt64 x86_xgetbv_0(UInt32 num)
{
#if defined(ATTRIB_XGETBV)
  {
    return
      #if (defined(_MSC_VER))
        _xgetbv(num);
      #else
        __builtin_ia32_xgetbv(
          #if !defined(__clang__)
            (int)
          #endif
            num);
      #endif
  }

#elif defined(__GNUC__) || defined(__clang__) || defined(__SUNPRO_CC)

  UInt32 a, d;
 #if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
  __asm__
  (
    "xgetbv"
    : "=a"(a), "=d"(d) : "c"(num) : "cc"
  );
 #else // is old gcc
  __asm__
  (
    ".byte 0x0f, 0x01, 0xd0" "\n\t"
    : "=a"(a), "=d"(d) : "c"(num) : "cc"
  );
 #endif
  return ((UInt64)d << 32) | a;
  // return a;

#elif defined(_MSC_VER) && !defined(MY_CPU_AMD64)
  
  UInt32 a, d;
  __asm {
    push eax
    push edx
    push ecx
    mov ecx, num;
    // xor ecx, ecx // = MY_XCR_XFEATURE_ENABLED_MASK
    _emit 0x0f
    _emit 0x01
    _emit 0xd0
    mov a, eax
    mov d, edx
    pop ecx
    pop edx
    pop eax
  }
  return ((UInt64)d << 32) | a;
  // return a;

#else // it's unknown compiler
  // #error "Need xgetbv function"
  UNUSED_VAR(num)
  // for MSVC-X64 we could call external function from external file.
  /* Actually we had checked OSXSAVE/AVX in cpuid before.
     So it's expected that OS supports at least AVX and below. */
  // if (num != MY_XCR_XFEATURE_ENABLED_MASK) return 0; // if not XCR0
  return
      // (1 << 0) |  // x87
        (1 << 1)   // SSE
      | (1 << 2);  // AVX
  
#endif
}

#ifdef _WIN32
/*
  Windows versions do not know about new ISA extensions that
  can be introduced. But we still can use new extensions,
  even if Windows doesn't report about supporting them,
  But we can use new extensions, only if Windows knows about new ISA extension
  that changes the number or size of registers: SSE, AVX/XSAVE, AVX512
  So it's enough to check
    MY_PF_AVX_INSTRUCTIONS_AVAILABLE
      instead of
    MY_PF_AVX2_INSTRUCTIONS_AVAILABLE
*/
#define MY_PF_XSAVE_ENABLED                            17
// #define MY_PF_SSSE3_INSTRUCTIONS_AVAILABLE             36
// #define MY_PF_SSE4_1_INSTRUCTIONS_AVAILABLE            37
// #define MY_PF_SSE4_2_INSTRUCTIONS_AVAILABLE            38
// #define MY_PF_AVX_INSTRUCTIONS_AVAILABLE               39
// #define MY_PF_AVX2_INSTRUCTIONS_AVAILABLE              40
// #define MY_PF_AVX512F_INSTRUCTIONS_AVAILABLE           41
#endif

BoolInt CPU_IsSupported_AVX(void)
{
  #ifdef _WIN32
  if (!IsProcessorFeaturePresent(MY_PF_XSAVE_ENABLED))
    return False;
  /* PF_AVX_INSTRUCTIONS_AVAILABLE probably is supported starting from
     some latest Win10 revisions. But we need AVX in older Windows also.
     So we don't use the following check: */
  /*
  if (!IsProcessorFeaturePresent(MY_PF_AVX_INSTRUCTIONS_AVAILABLE))
    return False;
  */
  #endif

  /*
    OS must use new special XSAVE/XRSTOR instructions to save
    AVX registers when it required for context switching.
    At OS statring:
      OS sets CR4.OSXSAVE flag to signal the processor that OS supports the XSAVE extensions.
      Also OS sets bitmask in XCR0 register that defines what
      registers will be processed by XSAVE instruction:
        XCR0.SSE[bit 0] - x87 registers and state
        XCR0.SSE[bit 1] - SSE registers and state
        XCR0.AVX[bit 2] - AVX registers and state
    CR4.OSXSAVE is reflected to CPUID.1:ECX.OSXSAVE[bit 27].
       So we can read that bit in user-space.
    XCR0 is available for reading in user-space by new XGETBV instruction.
  */
  {
    const UInt32 c = x86cpuid_Func_1_ECX();
    if (0 == (1
        & (c >> 28)   // AVX instructions are supported by hardware
        & (c >> 27))) // OSXSAVE bit: XSAVE and related instructions are enabled by OS.
      return False;
  }

  /* also we can check
     CPUID.1:ECX.XSAVE [bit 26] : that shows that
        XSAVE, XRESTOR, XSETBV, XGETBV instructions are supported by hardware.
     But that check is redundant, because if OSXSAVE bit is set, then XSAVE is also set */

  /* If OS have enabled XSAVE extension instructions (OSXSAVE == 1),
     in most cases we expect that OS also will support storing/restoring
     for AVX and SSE states at least.
     But to be ensure for that we call user-space instruction
     XGETBV(0) to get XCR0 value that contains bitmask that defines
     what exact states(registers) OS have enabled for storing/restoring.
  */

  {
    const UInt32 bm = (UInt32)x86_xgetbv_0(MY_XCR_XFEATURE_ENABLED_MASK);
    // printf("\n=== XGetBV=0x%x\n", bm);
    return 1
        & (BoolInt)(bm >> 1)  // SSE state is supported (set by OS) for storing/restoring
        & (BoolInt)(bm >> 2); // AVX state is supported (set by OS) for storing/restoring
  }
  // since Win7SP1: we can use GetEnabledXStateFeatures();
}


BoolInt CPU_IsSupported_AVX2(void)
{
  if (!CPU_IsSupported_AVX())
    return False;
  if (z7_x86_cpuid_GetMaxFunc() < 7)
    return False;
  {
    UInt32 d[4];
    z7_x86_cpuid(d, 7);
    // printf("\ncpuid(7): ebx=%8x ecx=%8x\n", d[1], d[2]);
    return 1
      & (BoolInt)(d[1] >> 5); // avx2
  }
}

#if 0
BoolInt CPU_IsSupported_AVX512F_AVX512VL(void)
{
  if (!CPU_IsSupported_AVX())
    return False;
  if (z7_x86_cpuid_GetMaxFunc() < 7)
    return False;
  {
    UInt32 d[4];
    BoolInt v;
    z7_x86_cpuid(d, 7);
    // printf("\ncpuid(7): ebx=%8x ecx=%8x\n", d[1], d[2]);
    v = 1
      & (BoolInt)(d[1] >> 16)  // avx512f
      & (BoolInt)(d[1] >> 31); // avx512vl
    if (!v)
      return False;
  }
  {
    const UInt32 bm = (UInt32)x86_xgetbv_0(MY_XCR_XFEATURE_ENABLED_MASK);
    // printf("\n=== XGetBV=0x%x\n", bm);
    return 1
        & (BoolInt)(bm >> 5)  // OPMASK
        & (BoolInt)(bm >> 6)  // ZMM upper 256-bit
        & (BoolInt)(bm >> 7); // ZMM16 ... ZMM31
  }
}
#endif

BoolInt CPU_IsSupported_VAES_AVX2(void)
{
  if (!CPU_IsSupported_AVX())
    return False;
  if (z7_x86_cpuid_GetMaxFunc() < 7)
    return False;
  {
    UInt32 d[4];
    z7_x86_cpuid(d, 7);
    // printf("\ncpuid(7): ebx=%8x ecx=%8x\n", d[1], d[2]);
    return 1
      & (BoolInt)(d[1] >> 5) // avx2
      // & (d[1] >> 31) // avx512vl
      & (BoolInt)(d[2] >> 9); // vaes // VEX-256/EVEX
  }
}

BoolInt CPU_IsSupported_PageGB(void)
{
  CHECK_CPUID_IS_SUPPORTED
  {
    UInt32 d[4];
    z7_x86_cpuid(d, 0x80000000);
    if (d[0] < 0x80000001)
      return False;
    z7_x86_cpuid(d, 0x80000001);
    return (BoolInt)(d[3] >> 26) & 1;
  }
}


#elif defined(MY_CPU_ARM_OR_ARM64)

#ifdef _WIN32


BoolInt CPU_IsSupported_CRC32(void)  { return IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE) ? 1 : 0; }
BoolInt CPU_IsSupported_CRYPTO(void) { return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) ? 1 : 0; }
BoolInt CPU_IsSupported_NEON(void)   { return IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE) ? 1 : 0; }

#else

#if defined(__APPLE__)

/*
#include <stdio.h>
#include <string.h>
static void Print_sysctlbyname(const char *name)
{
  size_t bufSize = 256;
  char buf[256];
  int res = sysctlbyname(name, &buf, &bufSize, NULL, 0);
  {
    int i;
    printf("\nres = %d : %s : '%s' : bufSize = %d, numeric", res, name, buf, (unsigned)bufSize);
    for (i = 0; i < 20; i++)
      printf(" %2x", (unsigned)(Byte)buf[i]);

  }
}
*/
/*
  Print_sysctlbyname("hw.pagesize");
  Print_sysctlbyname("machdep.cpu.brand_string");
*/

static BoolInt z7_sysctlbyname_Get_BoolInt(const char *name)
{
  UInt32 val = 0;
  if (z7_sysctlbyname_Get_UInt32(name, &val) == 0 && val == 1)
    return 1;
  return 0;
}

BoolInt CPU_IsSupported_CRC32(void)
{
  return z7_sysctlbyname_Get_BoolInt("hw.optional.armv8_crc32");
}

BoolInt CPU_IsSupported_NEON(void)
{
  return z7_sysctlbyname_Get_BoolInt("hw.optional.neon");
}

BoolInt CPU_IsSupported_SHA512(void)
{
  return z7_sysctlbyname_Get_BoolInt("hw.optional.armv8_2_sha512");
}

/*
BoolInt CPU_IsSupported_SHA3(void)
{
  return z7_sysctlbyname_Get_BoolInt("hw.optional.armv8_2_sha3");
}
*/

#ifdef MY_CPU_ARM64
#define APPLE_CRYPTO_SUPPORT_VAL 1
#else
#define APPLE_CRYPTO_SUPPORT_VAL 0
#endif

BoolInt CPU_IsSupported_SHA1(void) { return APPLE_CRYPTO_SUPPORT_VAL; }
BoolInt CPU_IsSupported_SHA2(void) { return APPLE_CRYPTO_SUPPORT_VAL; }
BoolInt CPU_IsSupported_AES (void) { return APPLE_CRYPTO_SUPPORT_VAL; }


#else // __APPLE__

#if defined(__GLIBC__) && (__GLIBC__ * 100 + __GLIBC_MINOR__ >= 216)
  #define Z7_GETAUXV_AVAILABLE
#elif !defined(__QNXNTO__)
// #pragma message("=== is not NEW GLIBC === ")
  #if defined __has_include
  #if __has_include (<sys/auxv.h>)
// #pragma message("=== sys/auxv.h is avail=== ")
    #define Z7_GETAUXV_AVAILABLE
  #endif
  #endif
#endif

#ifdef Z7_GETAUXV_AVAILABLE
// #pragma message("=== Z7_GETAUXV_AVAILABLE === ")
#include <sys/auxv.h>
#define USE_HWCAP
#endif

#ifdef USE_HWCAP

#if defined(__FreeBSD__) || defined(__OpenBSD__)
static unsigned long MY_getauxval(int aux)
{
  unsigned long val;
  if (elf_aux_info(aux, &val, sizeof(val)))
    return 0;
  return val;
}
#else
#define MY_getauxval  getauxval
  #if defined __has_include
  #if __has_include (<asm/hwcap.h>)
#include <asm/hwcap.h>
  #endif
  #endif
#endif

  #define MY_HWCAP_CHECK_FUNC_2(name1, name2) \
  BoolInt CPU_IsSupported_ ## name1(void) { return (MY_getauxval(AT_HWCAP)  & (HWCAP_  ## name2)); }

#ifdef MY_CPU_ARM64
  #define MY_HWCAP_CHECK_FUNC(name) \
  MY_HWCAP_CHECK_FUNC_2(name, name)
#if 1 || defined(__ARM_NEON)
  BoolInt CPU_IsSupported_NEON(void) { return True; }
#else
  MY_HWCAP_CHECK_FUNC_2(NEON, ASIMD)
#endif
// MY_HWCAP_CHECK_FUNC (ASIMD)
#elif defined(MY_CPU_ARM)
  #define MY_HWCAP_CHECK_FUNC(name) \
  BoolInt CPU_IsSupported_ ## name(void) { return (MY_getauxval(AT_HWCAP2) & (HWCAP2_ ## name)); }
  MY_HWCAP_CHECK_FUNC_2(NEON, NEON)
#endif

#else // USE_HWCAP

  #define MY_HWCAP_CHECK_FUNC(name) \
  BoolInt CPU_IsSupported_ ## name(void) { return 0; }
#if defined(__ARM_NEON)
  BoolInt CPU_IsSupported_NEON(void) { return True; }
#else
  MY_HWCAP_CHECK_FUNC(NEON)
#endif

#endif // USE_HWCAP

MY_HWCAP_CHECK_FUNC (CRC32)
MY_HWCAP_CHECK_FUNC (SHA1)
MY_HWCAP_CHECK_FUNC (SHA2)
MY_HWCAP_CHECK_FUNC (AES)
#ifdef MY_CPU_ARM64
// <hwcap.h> supports HWCAP_SHA512 and HWCAP_SHA3 since 2017.
// we define them here, if they are not defined
#ifndef HWCAP_SHA3
// #define HWCAP_SHA3    (1 << 17)
#endif
#ifndef HWCAP_SHA512
// #pragma message("=== HWCAP_SHA512 define === ")
#define HWCAP_SHA512  (1 << 21)
#endif
MY_HWCAP_CHECK_FUNC (SHA512)
// MY_HWCAP_CHECK_FUNC (SHA3)
#endif

#endif // __APPLE__
#endif // _WIN32

#endif // MY_CPU_ARM_OR_ARM64



#ifdef __APPLE__

/* sys/sysctl.h (via sys/proc.h) uses the legacy BSD types u_int/u_char/
 * u_short without declaring them itself; sys/types.h must come first,
 * and does not get pulled in automatically under -std=c11. */
#include <sys/types.h>
#include <sys/sysctl.h>

int z7_sysctlbyname_Get(const char *name, void *buf, size_t *bufSize)
{
  return sysctlbyname(name, buf, bufSize, NULL, 0);
}

int z7_sysctlbyname_Get_UInt32(const char *name, UInt32 *val)
{
  size_t bufSize = sizeof(*val);
  const int res = z7_sysctlbyname_Get(name, val, &bufSize);
  if (res == 0 && bufSize != sizeof(*val))
    return EFAULT;
  return res;
}

#endif

/* ---- embedded source: LzFind.c ---- */
/* LzFind.c -- Match finder for LZ algorithms
: Igor Pavlov : Public domain */


#include <string.h>
// #include <stdio.h>


#define kBlockMoveAlign       (1 << 7)    // alignment for memmove()
#define kBlockSizeAlign       (1 << 16)   // alignment for block allocation
#define kBlockSizeReserveMin  (1 << 24)   // it's 1/256 from 4 GB dictinary

#define kEmptyHashValue 0

#define kMaxValForNormalize ((UInt32)0)
// #define kMaxValForNormalize ((UInt32)(1 << 20) + 0xfff) // for debug

// #define kNormalizeAlign (1 << 7) // alignment for speculated accesses

#define GET_AVAIL_BYTES(p) \
  Inline_MatchFinder_GetNumAvailableBytes(p)


// #define kFix5HashSize (kHash2Size + kHash3Size + kHash4Size)
#define kFix5HashSize kFix4HashSize

/*
 HASH2_CALC:
   if (hv) match, then cur[0] and cur[1] also match
*/
#define HASH2_CALC hv = GetUi16(cur);

// (crc[0 ... 255] & 0xFF) provides one-to-one correspondence to [0 ... 255]

/*
 HASH3_CALC:
   if (cur[0]) and (h2) match, then cur[1]            also match
   if (cur[0]) and (hv) match, then cur[1] and cur[2] also match
*/
#define HASH3_CALC { \
  UInt32 temp = p->crc[cur[0]] ^ cur[1]; \
  h2 = temp & (kHash2Size - 1); \
  hv = (temp ^ ((UInt32)cur[2] << 8)) & p->hashMask; }

#define HASH4_CALC { \
  UInt32 temp = p->crc[cur[0]] ^ cur[1]; \
  h2 = temp & (kHash2Size - 1); \
  temp ^= ((UInt32)cur[2] << 8); \
  h3 = temp & (kHash3Size - 1); \
  hv = (temp ^ (p->crc[cur[3]] << kLzHash_CrcShift_1)) & p->hashMask; }

#define HASH5_CALC { \
  UInt32 temp = p->crc[cur[0]] ^ cur[1]; \
  h2 = temp & (kHash2Size - 1); \
  temp ^= ((UInt32)cur[2] << 8); \
  h3 = temp & (kHash3Size - 1); \
  temp ^= (p->crc[cur[3]] << kLzHash_CrcShift_1); \
  /* h4 = temp & p->hash4Mask; */ /* (kHash4Size - 1); */ \
  hv = (temp ^ (p->crc[cur[4]] << kLzHash_CrcShift_2)) & p->hashMask; }

#define HASH_ZIP_CALC hv = ((cur[2] | ((UInt32)cur[0] << 8)) ^ p->crc[cur[1]]) & 0xFFFF;


static void LzInWindow_Free(CMatchFinder *p, ISzAllocPtr alloc)
{
  // if (!p->directInput)
  {
    ISzAlloc_Free(alloc, p->bufBase);
    p->bufBase = NULL;
  }
}


static int LzInWindow_Create2(CMatchFinder *p, UInt32 blockSize, ISzAllocPtr alloc)
{
  if (blockSize == 0)
    return 0;
  if (!p->bufBase || p->blockSize != blockSize)
  {
    // size_t blockSizeT;
    LzInWindow_Free(p, alloc);
    p->blockSize = blockSize;
    // blockSizeT = blockSize;
    
    // printf("\nblockSize = 0x%x\n", blockSize);
    /*
    #if defined _WIN64
    // we can allocate 4GiB, but still use UInt32 for (p->blockSize)
    // we use UInt32 type for (p->blockSize), because
    // we don't want to wrap over 4 GiB,
    // when we use (p->streamPos - p->pos) that is UInt32.
    if (blockSize >= (UInt32)0 - (UInt32)kBlockSizeAlign)
    {
      blockSizeT = ((size_t)1 << 32);
      printf("\nchanged to blockSizeT = 4GiB\n");
    }
    #endif
    */
    
    p->bufBase = (Byte *)ISzAlloc_Alloc(alloc, blockSize);
    // printf("\nbufferBase = %p\n", p->bufBase);
    // return 0; // for debug
  }
  return (p->bufBase != NULL);
}

static const Byte *MatchFinder_GetPointerToCurrentPos(void *p)
{
  return ((CMatchFinder *)p)->buffer;
}

static UInt32 MatchFinder_GetNumAvailableBytes(void *p)
{
  return GET_AVAIL_BYTES((CMatchFinder *)p);
}


Z7_NO_INLINE
static void MatchFinder_ReadBlock(CMatchFinder *p)
{
  if (p->streamEndWasReached || p->result != SZ_OK)
    return;

  /* We use (p->streamPos - p->pos) value.
     (p->streamPos < p->pos) is allowed. */

  if (p->directInput)
  {
    UInt32 curSize = 0xFFFFFFFF - GET_AVAIL_BYTES(p);
    if (curSize > p->directInputRem)
      curSize = (UInt32)p->directInputRem;
    p->streamPos += curSize;
    p->directInputRem -= curSize;
    if (p->directInputRem == 0)
      p->streamEndWasReached = 1;
    return;
  }
  
  for (;;)
  {
    const Byte *dest = p->buffer + GET_AVAIL_BYTES(p);
    size_t size = (size_t)(p->bufBase + p->blockSize - dest);
    if (size == 0)
    {
      /* we call ReadBlock() after NeedMove() and MoveBlock().
         NeedMove() and MoveBlock() povide more than (keepSizeAfter)
         to the end of (blockSize).
         So we don't execute this branch in normal code flow.
         We can go here, if we will call ReadBlock() before NeedMove(), MoveBlock().
      */
      // p->result = SZ_ERROR_FAIL; // we can show error here
      return;
    }

    // #define kRead 3
    // if (size > kRead) size = kRead; // for debug

    /*
    // we need cast (Byte *)dest.
    #ifdef __clang__
      #pragma GCC diagnostic ignored "-Wcast-qual"
    #endif
    */
    p->result = ISeqInStream_Read(p->stream,
        p->bufBase + (dest - p->bufBase), &size);
    if (p->result != SZ_OK)
      return;
    if (size == 0)
    {
      p->streamEndWasReached = 1;
      return;
    }
    p->streamPos += (UInt32)size;
    if (GET_AVAIL_BYTES(p) > p->keepSizeAfter)
      return;
    /* here and in another (p->keepSizeAfter) checks we keep on 1 byte more than was requested by Create() function
         (GET_AVAIL_BYTES(p) >= p->keepSizeAfter) - minimal required size */
  }

  // on exit: (p->result != SZ_OK || p->streamEndWasReached || GET_AVAIL_BYTES(p) > p->keepSizeAfter)
}



Z7_NO_INLINE
void MatchFinder_MoveBlock(CMatchFinder *p)
{
  const size_t offset = (size_t)(p->buffer - p->bufBase) - p->keepSizeBefore;
  const size_t keepBefore = (offset & (kBlockMoveAlign - 1)) + p->keepSizeBefore;
  p->buffer = p->bufBase + keepBefore;
  memmove(p->bufBase,
      p->bufBase + (offset & ~((size_t)kBlockMoveAlign - 1)),
      keepBefore + (size_t)GET_AVAIL_BYTES(p));
}

/* We call MoveBlock() before ReadBlock().
   So MoveBlock() can be wasteful operation, if the whole input data
   can fit in current block even without calling MoveBlock().
   in important case where (dataSize <= historySize)
     condition (p->blockSize > dataSize + p->keepSizeAfter) is met
     So there is no MoveBlock() in that case case.
*/

int MatchFinder_NeedMove(CMatchFinder *p)
{
  if (p->directInput)
    return 0;
  if (p->streamEndWasReached || p->result != SZ_OK)
    return 0;
  return ((size_t)(p->bufBase + p->blockSize - p->buffer) <= p->keepSizeAfter);
}

void MatchFinder_ReadIfRequired(CMatchFinder *p)
{
  if (p->keepSizeAfter >= GET_AVAIL_BYTES(p))
    MatchFinder_ReadBlock(p);
}



static void MatchFinder_SetDefaultSettings(CMatchFinder *p)
{
  p->cutValue = 32;
  p->btMode = 1;
  p->numHashBytes = 4;
  p->numHashBytes_Min = 2;
  p->numHashOutBits = 0;
  p->bigHash = 0;
}

#define kCrcPoly 0xEDB88320

void MatchFinder_Construct(CMatchFinder *p)
{
  unsigned i;
  p->buffer = NULL;
  p->bufBase = NULL;
  p->directInput = 0;
  p->stream = NULL;
  p->hash = NULL;
  p->expectedDataSize = (UInt64)(Int64)-1;
  MatchFinder_SetDefaultSettings(p);

  for (i = 0; i < 256; i++)
  {
    UInt32 r = (UInt32)i;
    unsigned j;
    for (j = 0; j < 8; j++)
      r = (r >> 1) ^ (kCrcPoly & ((UInt32)0 - (r & 1)));
    p->crc[i] = r;
  }
}

#undef kCrcPoly

static void MatchFinder_FreeThisClassMemory(CMatchFinder *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->hash);
  p->hash = NULL;
}

void MatchFinder_Free(CMatchFinder *p, ISzAllocPtr alloc)
{
  MatchFinder_FreeThisClassMemory(p, alloc);
  LzInWindow_Free(p, alloc);
}

static CLzRef* AllocRefs(size_t num, ISzAllocPtr alloc)
{
  const size_t sizeInBytes = (size_t)num * sizeof(CLzRef);
  if (sizeInBytes / sizeof(CLzRef) != num)
    return NULL;
  return (CLzRef *)ISzAlloc_Alloc(alloc, sizeInBytes);
}

#if (kBlockSizeReserveMin < kBlockSizeAlign * 2)
  #error Stop_Compiling_Bad_Reserve
#endif



static UInt32 GetBlockSize(CMatchFinder *p, UInt32 historySize)
{
  UInt32 blockSize = (p->keepSizeBefore + p->keepSizeAfter);
  /*
  if (historySize > kMaxHistorySize)
    return 0;
  */
  // printf("\nhistorySize == 0x%x\n", historySize);
  
  if (p->keepSizeBefore < historySize || blockSize < p->keepSizeBefore)  // if 32-bit overflow
    return 0;
  
  {
    const UInt32 kBlockSizeMax = (UInt32)0 - (UInt32)kBlockSizeAlign;
    const UInt32 rem = kBlockSizeMax - blockSize;
    const UInt32 reserve = (blockSize >> (blockSize < ((UInt32)1 << 30) ? 1 : 2))
        + (1 << 12) + kBlockMoveAlign + kBlockSizeAlign; // do not overflow 32-bit here
    if (blockSize >= kBlockSizeMax
        || rem < kBlockSizeReserveMin) // we reject settings that will be slow
      return 0;
    if (reserve >= rem)
      blockSize = kBlockSizeMax;
    else
    {
      blockSize += reserve;
      blockSize &= ~(UInt32)(kBlockSizeAlign - 1);
    }
  }
  // printf("\n LzFind_blockSize = %x\n", blockSize);
  // printf("\n LzFind_blockSize = %d\n", blockSize >> 20);
  return blockSize;
}


// input is historySize
static UInt32 MatchFinder_GetHashMask2(CMatchFinder *p, UInt32 hs)
{
  if (p->numHashBytes == 2)
    return (1 << 16) - 1;
  if (hs != 0)
    hs--;
  hs |= (hs >> 1);
  hs |= (hs >> 2);
  hs |= (hs >> 4);
  hs |= (hs >> 8);
  // we propagated 16 bits in (hs). Low 16 bits must be set later
  if (hs >= (1 << 24))
  {
    if (p->numHashBytes == 3)
      hs = (1 << 24) - 1;
    /* if (bigHash) mode, GetHeads4b() in LzFindMt.c needs (hs >= ((1 << 24) - 1))) */
  }
  // (hash_size >= (1 << 16)) : Required for (numHashBytes > 2)
  hs |= (1 << 16) - 1; /* don't change it! */
  // bt5: we adjust the size with recommended minimum size
  if (p->numHashBytes >= 5)
    hs |= (256 << kLzHash_CrcShift_2) - 1;
  return hs;
}

// input is historySize
static UInt32 MatchFinder_GetHashMask(CMatchFinder *p, UInt32 hs)
{
  if (p->numHashBytes == 2)
    return (1 << 16) - 1;
  if (hs != 0)
    hs--;
  hs |= (hs >> 1);
  hs |= (hs >> 2);
  hs |= (hs >> 4);
  hs |= (hs >> 8);
  // we propagated 16 bits in (hs). Low 16 bits must be set later
  hs >>= 1;
  if (hs >= (1 << 24))
  {
    if (p->numHashBytes == 3)
      hs = (1 << 24) - 1;
    else
      hs >>= 1;
    /* if (bigHash) mode, GetHeads4b() in LzFindMt.c needs (hs >= ((1 << 24) - 1))) */
  }
  // (hash_size >= (1 << 16)) : Required for (numHashBytes > 2)
  hs |= (1 << 16) - 1; /* don't change it! */
  // bt5: we adjust the size with recommended minimum size
  if (p->numHashBytes >= 5)
    hs |= (256 << kLzHash_CrcShift_2) - 1;
  return hs;
}


int MatchFinder_Create(CMatchFinder *p, UInt32 historySize,
    UInt32 keepAddBufferBefore, UInt32 matchMaxLen, UInt32 keepAddBufferAfter,
    ISzAllocPtr alloc)
{
  /* we need one additional byte in (p->keepSizeBefore),
     since we use MoveBlock() after (p->pos++) and before dictionary using */
  // keepAddBufferBefore = (UInt32)0xFFFFFFFF - (1 << 22); // for debug
  p->keepSizeBefore = historySize + keepAddBufferBefore + 1;

  keepAddBufferAfter += matchMaxLen;
  /* we need (p->keepSizeAfter >= p->numHashBytes) */
  if (keepAddBufferAfter < p->numHashBytes)
    keepAddBufferAfter = p->numHashBytes;
  // keepAddBufferAfter -= 2; // for debug
  p->keepSizeAfter = keepAddBufferAfter;

  if (p->directInput)
    p->blockSize = 0;
  if (p->directInput || LzInWindow_Create2(p, GetBlockSize(p, historySize), alloc))
  {
    size_t hashSizeSum;
    {
      UInt32 hs;
      UInt32 hsCur;
      
      if (p->numHashOutBits != 0)
      {
        unsigned numBits = p->numHashOutBits;
        const unsigned nbMax =
            (p->numHashBytes == 2 ? 16 :
            (p->numHashBytes == 3 ? 24 : 32));
        if (numBits >= nbMax)
          numBits = nbMax;
        if (numBits >= 32)
          hs = (UInt32)0 - 1;
        else
          hs = ((UInt32)1 << numBits) - 1;
        // (hash_size >= (1 << 16)) : Required for (numHashBytes > 2)
        hs |= (1 << 16) - 1; /* don't change it! */
        if (p->numHashBytes >= 5)
          hs |= (256 << kLzHash_CrcShift_2) - 1;
        {
          const UInt32 hs2 = MatchFinder_GetHashMask2(p, historySize);
          if (hs >= hs2)
            hs = hs2;
        }
        hsCur = hs;
        if (p->expectedDataSize < historySize)
        {
          const UInt32 hs2 = MatchFinder_GetHashMask2(p, (UInt32)p->expectedDataSize);
          if (hsCur >= hs2)
            hsCur = hs2;
        }
      }
      else
      {
        hs = MatchFinder_GetHashMask(p, historySize);
        hsCur = hs;
        if (p->expectedDataSize < historySize)
        {
          hsCur = MatchFinder_GetHashMask(p, (UInt32)p->expectedDataSize);
          if (hsCur >= hs) // is it possible?
            hsCur = hs;
        }
      }

      p->hashMask = hsCur;

      hashSizeSum = hs;
      hashSizeSum++;
      if (hashSizeSum < hs)
        return 0;
      {
        UInt32 fixedHashSize = 0;
        if (p->numHashBytes > 2 && p->numHashBytes_Min <= 2) fixedHashSize += kHash2Size;
        if (p->numHashBytes > 3 && p->numHashBytes_Min <= 3) fixedHashSize += kHash3Size;
        // if (p->numHashBytes > 4) p->fixedHashSize += hs4; // kHash4Size;
        hashSizeSum += fixedHashSize;
        p->fixedHashSize = fixedHashSize;
      }
    }

    p->matchMaxLen = matchMaxLen;

    {
      size_t newSize;
      size_t numSons;
      const UInt32 newCyclicBufferSize = historySize + 1; // do not change it
      p->historySize = historySize;
      p->cyclicBufferSize = newCyclicBufferSize; // it must be = (historySize + 1)
      
      numSons = newCyclicBufferSize;
      if (p->btMode)
        numSons <<= 1;
      newSize = hashSizeSum + numSons;

      if (numSons < newCyclicBufferSize || newSize < numSons)
        return 0;

      // aligned size is not required here, but it can be better for some loops
      #define NUM_REFS_ALIGN_MASK 0xF
      newSize = (newSize + NUM_REFS_ALIGN_MASK) & ~(size_t)NUM_REFS_ALIGN_MASK;

      // 22.02: we don't reallocate buffer, if old size is enough
      if (p->hash && p->numRefs >= newSize)
        return 1;
      
      MatchFinder_FreeThisClassMemory(p, alloc);
      p->numRefs = newSize;
      p->hash = AllocRefs(newSize, alloc);
      
      if (p->hash)
      {
        p->son = p->hash + hashSizeSum;
        return 1;
      }
    }
  }

  MatchFinder_Free(p, alloc);
  return 0;
}


static void MatchFinder_SetLimits(CMatchFinder *p)
{
  UInt32 k;
  UInt32 n = kMaxValForNormalize - p->pos;
  if (n == 0)
    n = (UInt32)(Int32)-1;  // we allow (pos == 0) at start even with (kMaxValForNormalize == 0)
  
  k = p->cyclicBufferSize - p->cyclicBufferPos;
  if (k < n)
    n = k;

  k = GET_AVAIL_BYTES(p);
  {
    const UInt32 ksa = p->keepSizeAfter;
    UInt32 mm = p->matchMaxLen;
    if (k > ksa)
      k -= ksa; // we must limit exactly to keepSizeAfter for ReadBlock
    else if (k >= mm)
    {
      // the limitation for (p->lenLimit) update
      k -= mm;   // optimization : to reduce the number of checks
      k++;
      // k = 1; // non-optimized version : for debug
    }
    else
    {
      mm = k;
      if (k != 0)
        k = 1;
    }
    p->lenLimit = mm;
  }
  if (k < n)
    n = k;
  
  p->posLimit = p->pos + n;
}


void MatchFinder_Init_LowHash(CMatchFinder *p)
{
  size_t i;
  CLzRef *items = p->hash;
  const size_t numItems = p->fixedHashSize;
  for (i = 0; i < numItems; i++)
    items[i] = kEmptyHashValue;
}


void MatchFinder_Init_HighHash(CMatchFinder *p)
{
  size_t i;
  CLzRef *items = p->hash + p->fixedHashSize;
  const size_t numItems = (size_t)p->hashMask + 1;
  for (i = 0; i < numItems; i++)
    items[i] = kEmptyHashValue;
}


void MatchFinder_Init_4(CMatchFinder *p)
{
  if (!p->directInput)
    p->buffer = p->bufBase;
  {
    /* kEmptyHashValue = 0 (Zero) is used in hash tables as NO-VALUE marker.
       the code in CMatchFinderMt expects (pos = 1) */
    p->pos =
    p->streamPos =
        1; // it's smallest optimal value. do not change it
        // 0; // for debug
  }
  p->result = SZ_OK;
  p->streamEndWasReached = 0;
}


// (CYC_TO_POS_OFFSET == 0) is expected by some optimized code
#define CYC_TO_POS_OFFSET 0
// #define CYC_TO_POS_OFFSET 1 // for debug

void MatchFinder_Init(void *_p)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  MatchFinder_Init_HighHash(p);
  MatchFinder_Init_LowHash(p);
  MatchFinder_Init_4(p);
  // if (readData)
  MatchFinder_ReadBlock(p);

  /* if we init (cyclicBufferPos = pos), then we can use one variable
     instead of both (cyclicBufferPos) and (pos) : only before (cyclicBufferPos) wrapping */
  p->cyclicBufferPos = (p->pos - CYC_TO_POS_OFFSET); // init with relation to (pos)
  // p->cyclicBufferPos = 0; // smallest value
  // p->son[0] = p->son[1] = 0; // unused: we can init skipped record for speculated accesses.
  MatchFinder_SetLimits(p);
}



#ifdef MY_CPU_X86_OR_AMD64
  #if defined(__clang__) && (__clang_major__ >= 4) \
    || defined(Z7_GCC_VERSION) && (Z7_GCC_VERSION >= 40900)
    // || defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 1900)

      #define USE_LZFIND_SATUR_SUB_128
      #define USE_LZFIND_SATUR_SUB_256
      #define LZFIND_ATTRIB_SSE41 __attribute__((__target__("sse4.1")))
      #define LZFIND_ATTRIB_AVX2  __attribute__((__target__("avx2")))
  #elif defined(_MSC_VER)
    #if (_MSC_VER >= 1600)
      #define USE_LZFIND_SATUR_SUB_128
    #endif
    #if (_MSC_VER >= 1900)
      #define USE_LZFIND_SATUR_SUB_256
    #endif
  #endif

#elif defined(MY_CPU_ARM64) \
  /* || (defined(__ARM_ARCH) && (__ARM_ARCH >= 7)) */

  #if  defined(Z7_CLANG_VERSION) && (Z7_CLANG_VERSION >= 30800) \
    || defined(__GNUC__) && (__GNUC__ >= 6)
      #define USE_LZFIND_SATUR_SUB_128
    #ifdef MY_CPU_ARM64
      // #define LZFIND_ATTRIB_SSE41 __attribute__((__target__("")))
    #else
      #define LZFIND_ATTRIB_SSE41 __attribute__((__target__("fpu=neon")))
    #endif

  #elif defined(_MSC_VER)
    #if (_MSC_VER >= 1910)
      #define USE_LZFIND_SATUR_SUB_128
    #endif
  #endif

  #if defined(Z7_MSC_VER_ORIGINAL) && defined(MY_CPU_ARM64)
    #include <arm64_neon.h>
  #else
    #include <arm_neon.h>
  #endif

#endif


#ifdef USE_LZFIND_SATUR_SUB_128

// #define Z7_SHOW_HW_STATUS

#ifdef Z7_SHOW_HW_STATUS
#include <stdio.h>
#define PRF(x) x
PRF(;)
#else
#define PRF(x)
#endif


#ifdef MY_CPU_ARM_OR_ARM64

#ifdef MY_CPU_ARM64
// #define FORCE_LZFIND_SATUR_SUB_128
#endif
typedef uint32x4_t LzFind_v128;
#define SASUB_128_V(v, s) \
  vsubq_u32(vmaxq_u32(v, s), s)

#else // MY_CPU_ARM_OR_ARM64

#include <smmintrin.h> // sse4.1

typedef __m128i LzFind_v128;
// SSE 4.1
#define SASUB_128_V(v, s)   \
  _mm_sub_epi32(_mm_max_epu32(v, s), s)

#endif // MY_CPU_ARM_OR_ARM64


#define SASUB_128(i) \
  *(      LzFind_v128 *)(      void *)(items + (i) * 4) = SASUB_128_V( \
  *(const LzFind_v128 *)(const void *)(items + (i) * 4), sub2);


Z7_NO_INLINE
static
#ifdef LZFIND_ATTRIB_SSE41
LZFIND_ATTRIB_SSE41
#endif
void
Z7_FASTCALL
LzFind_SaturSub_128(UInt32 subValue, CLzRef *items, const CLzRef *lim)
{
  const LzFind_v128 sub2 =
    #ifdef MY_CPU_ARM_OR_ARM64
      vdupq_n_u32(subValue);
    #else
      _mm_set_epi32((Int32)subValue, (Int32)subValue, (Int32)subValue, (Int32)subValue);
    #endif
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
  do
  {
    SASUB_128(0)  SASUB_128(1)  items += 2 * 4;
    SASUB_128(0)  SASUB_128(1)  items += 2 * 4;
  }
  while (items != lim);
}



#ifdef USE_LZFIND_SATUR_SUB_256

#include <immintrin.h> // avx
/*
clang :immintrin.h uses
#if !(defined(_MSC_VER) || defined(__SCE__)) || __has_feature(modules) ||      \
    defined(__AVX2__)
#include <avx2intrin.h>
#endif
so we need <avxintrin.h> for clang-cl */

#if defined(__clang__)
#include <avxintrin.h>
#include <avx2intrin.h>
#endif

// AVX2:
#define SASUB_256(i) \
    *(      __m256i *)(      void *)(items + (i) * 8) = \
   _mm256_sub_epi32(_mm256_max_epu32( \
    *(const __m256i *)(const void *)(items + (i) * 8), sub2), sub2);

Z7_NO_INLINE
static
#ifdef LZFIND_ATTRIB_AVX2
LZFIND_ATTRIB_AVX2
#endif
void
Z7_FASTCALL
LzFind_SaturSub_256(UInt32 subValue, CLzRef *items, const CLzRef *lim)
{
  const __m256i sub2 = _mm256_set_epi32(
      (Int32)subValue, (Int32)subValue, (Int32)subValue, (Int32)subValue,
      (Int32)subValue, (Int32)subValue, (Int32)subValue, (Int32)subValue);
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
  do
  {
    SASUB_256(0)  SASUB_256(1)  items += 2 * 8;
    SASUB_256(0)  SASUB_256(1)  items += 2 * 8;
  }
  while (items != lim);
}
#endif // USE_LZFIND_SATUR_SUB_256

#ifndef FORCE_LZFIND_SATUR_SUB_128
typedef void (Z7_FASTCALL *LZFIND_SATUR_SUB_CODE_FUNC)(
    UInt32 subValue, CLzRef *items, const CLzRef *lim);
static LZFIND_SATUR_SUB_CODE_FUNC g_LzFind_SaturSub;
#endif // FORCE_LZFIND_SATUR_SUB_128

#endif // USE_LZFIND_SATUR_SUB_128


// kEmptyHashValue must be zero
// #define SASUB_32(i)  { UInt32 v = items[i];  UInt32 m = v - subValue;  if (v < subValue) m = kEmptyHashValue;  items[i] = m; }
#define SASUB_32(i)  { UInt32 v = items[i];  if (v < subValue) v = subValue; items[i] = v - subValue; }

#ifdef FORCE_LZFIND_SATUR_SUB_128

#define DEFAULT_SaturSub LzFind_SaturSub_128

#else

#define DEFAULT_SaturSub LzFind_SaturSub_32

Z7_NO_INLINE
static
void
Z7_FASTCALL
LzFind_SaturSub_32(UInt32 subValue, CLzRef *items, const CLzRef *lim)
{
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
  do
  {
    SASUB_32(0)  SASUB_32(1)  items += 2;
    SASUB_32(0)  SASUB_32(1)  items += 2;
    SASUB_32(0)  SASUB_32(1)  items += 2;
    SASUB_32(0)  SASUB_32(1)  items += 2;
  }
  while (items != lim);
}

#endif


Z7_NO_INLINE
void MatchFinder_Normalize3(UInt32 subValue, CLzRef *items, size_t numItems)
{
  #define LZFIND_NORM_ALIGN_BLOCK_SIZE (1 << 7)
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
  for (; numItems != 0 && ((unsigned)(ptrdiff_t)items & (LZFIND_NORM_ALIGN_BLOCK_SIZE - 1)) != 0; numItems--)
  {
    SASUB_32(0)
    items++;
  }
  {
    const size_t k_Align_Mask = (LZFIND_NORM_ALIGN_BLOCK_SIZE / 4 - 1);
    CLzRef *lim = items + (numItems & ~(size_t)k_Align_Mask);
    numItems &= k_Align_Mask;
    if (items != lim)
    {
      #if defined(USE_LZFIND_SATUR_SUB_128) && !defined(FORCE_LZFIND_SATUR_SUB_128)
        if (g_LzFind_SaturSub)
          g_LzFind_SaturSub(subValue, items, lim);
        else
      #endif
          DEFAULT_SaturSub(subValue, items, lim);
    }
    items = lim;
  }
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
  for (; numItems != 0; numItems--)
  {
    SASUB_32(0)
    items++;
  }
}



// call MatchFinder_CheckLimits() only after (p->pos++) update

Z7_NO_INLINE
static void MatchFinder_CheckLimits(CMatchFinder *p)
{
  if (// !p->streamEndWasReached && p->result == SZ_OK &&
      p->keepSizeAfter == GET_AVAIL_BYTES(p))
  {
    // we try to read only in exact state (p->keepSizeAfter == GET_AVAIL_BYTES(p))
    if (MatchFinder_NeedMove(p))
      MatchFinder_MoveBlock(p);
    MatchFinder_ReadBlock(p);
  }

  if (p->pos == kMaxValForNormalize)
  if (GET_AVAIL_BYTES(p) >= p->numHashBytes) // optional optimization for last bytes of data.
    /*
       if we disable normalization for last bytes of data, and
       if (data_size == 4 GiB), we don't call wastfull normalization,
       but (pos) will be wrapped over Zero (0) in that case.
       And we cannot resume later to normal operation
    */
  {
    // MatchFinder_Normalize(p);
    /* after normalization we need (p->pos >= p->historySize + 1); */
    /* we can reduce subValue to aligned value, if want to keep alignment
       of (p->pos) and (p->buffer) for speculated accesses. */
    const UInt32 subValue = (p->pos - p->historySize - 1) /* & ~(UInt32)(kNormalizeAlign - 1) */;
    // const UInt32 subValue = (1 << 15); // for debug
    // printf("\nMatchFinder_Normalize() subValue == 0x%x\n", subValue);
    MatchFinder_REDUCE_OFFSETS(p, subValue)
    MatchFinder_Normalize3(subValue, p->hash, (size_t)p->hashMask + 1 + p->fixedHashSize);
    {
      size_t numSonRefs = p->cyclicBufferSize;
      if (p->btMode)
        numSonRefs <<= 1;
      MatchFinder_Normalize3(subValue, p->son, numSonRefs);
    }
  }

  if (p->cyclicBufferPos == p->cyclicBufferSize)
    p->cyclicBufferPos = 0;
  
  MatchFinder_SetLimits(p);
}


/*
  (lenLimit > maxLen)
*/
Z7_FORCE_INLINE
static UInt32 * Hc_GetMatchesSpec(size_t lenLimit, UInt32 curMatch, UInt32 pos, const Byte *cur, CLzRef *son,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize, UInt32 cutValue,
    UInt32 *d, unsigned maxLen)
{
  /*
  son[_cyclicBufferPos] = curMatch;
  for (;;)
  {
    UInt32 delta = pos - curMatch;
    if (cutValue-- == 0 || delta >= _cyclicBufferSize)
      return d;
    {
      const Byte *pb = cur - delta;
      curMatch = son[_cyclicBufferPos - delta + (_cyclicBufferPos < delta ? _cyclicBufferSize : 0)];
      if (pb[maxLen] == cur[maxLen] && *pb == *cur)
      {
        UInt32 len = 0;
        while (++len != lenLimit)
          if (pb[len] != cur[len])
            break;
        if (maxLen < len)
        {
          maxLen = len;
          *d++ = len;
          *d++ = delta - 1;
          if (len == lenLimit)
            return d;
        }
      }
    }
  }
  */

  const Byte *lim = cur + lenLimit;
  son[_cyclicBufferPos] = curMatch;

  do
  {
    UInt32 delta;

    if (curMatch == 0)
      break;
    // if (curMatch2 >= curMatch) return NULL;
    delta = pos - curMatch;
    if (delta >= _cyclicBufferSize)
      break;
    {
      ptrdiff_t diff;
      curMatch = son[_cyclicBufferPos - delta + (_cyclicBufferPos < delta ? _cyclicBufferSize : 0)];
      diff = (ptrdiff_t)0 - (ptrdiff_t)delta;
      if (cur[maxLen] == cur[(ptrdiff_t)maxLen + diff])
      {
        const Byte *c = cur;
        while (*c == c[diff])
        {
          if (++c == lim)
          {
            d[0] = (UInt32)(lim - cur);
            d[1] = delta - 1;
            return d + 2;
          }
        }
        {
          const unsigned len = (unsigned)(c - cur);
          if (maxLen < len)
          {
            maxLen = len;
            d[0] = (UInt32)len;
            d[1] = delta - 1;
            d += 2;
          }
        }
      }
    }
  }
  while (--cutValue);
  
  return d;
}


Z7_FORCE_INLINE
UInt32 * GetMatchesSpec1(UInt32 lenLimit, UInt32 curMatch, UInt32 pos, const Byte *cur, CLzRef *son,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize, UInt32 cutValue,
    UInt32 *d, UInt32 maxLen)
{
  CLzRef *ptr0 = son + ((size_t)_cyclicBufferPos << 1) + 1;
  CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);
  unsigned len0 = 0, len1 = 0;

  UInt32 cmCheck;

  // if (curMatch >= pos) { *ptr0 = *ptr1 = kEmptyHashValue; return NULL; }

  cmCheck = (UInt32)(pos - _cyclicBufferSize);
  if ((UInt32)pos < _cyclicBufferSize)
    cmCheck = 0;

  if (cmCheck < curMatch)
  do
  {
    const UInt32 delta = pos - curMatch;
    {
      CLzRef *pair = son + ((size_t)(_cyclicBufferPos - delta + (_cyclicBufferPos < delta ? _cyclicBufferSize : 0)) << 1);
      const Byte *pb = cur - delta;
      unsigned len = (len0 < len1 ? len0 : len1);
      const UInt32 pair0 = pair[0];
      if (pb[len] == cur[len])
      {
        if (++len != lenLimit && pb[len] == cur[len])
          while (++len != lenLimit)
            if (pb[len] != cur[len])
              break;
        if (maxLen < len)
        {
          maxLen = (UInt32)len;
          *d++ = (UInt32)len;
          *d++ = delta - 1;
          if (len == lenLimit)
          {
            *ptr1 = pair0;
            *ptr0 = pair[1];
            return d;
          }
        }
      }
      if (pb[len] < cur[len])
      {
        *ptr1 = curMatch;
        // const UInt32 curMatch2 = pair[1];
        // if (curMatch2 >= curMatch) { *ptr0 = *ptr1 = kEmptyHashValue;  return NULL; }
        // curMatch = curMatch2;
        curMatch = pair[1];
        ptr1 = pair + 1;
        len1 = len;
      }
      else
      {
        *ptr0 = curMatch;
        curMatch = pair[0];
        ptr0 = pair;
        len0 = len;
      }
    }
  }
  while(--cutValue && cmCheck < curMatch);

  *ptr0 = *ptr1 = kEmptyHashValue;
  return d;
}


static void SkipMatchesSpec(UInt32 lenLimit, UInt32 curMatch, UInt32 pos, const Byte *cur, CLzRef *son,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize, UInt32 cutValue)
{
  CLzRef *ptr0 = son + ((size_t)_cyclicBufferPos << 1) + 1;
  CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);
  unsigned len0 = 0, len1 = 0;

  UInt32 cmCheck;

  cmCheck = (UInt32)(pos - _cyclicBufferSize);
  if ((UInt32)pos < _cyclicBufferSize)
    cmCheck = 0;

  if (// curMatch >= pos ||  // failure
      cmCheck < curMatch)
  do
  {
    const UInt32 delta = pos - curMatch;
    {
      CLzRef *pair = son + ((size_t)(_cyclicBufferPos - delta + (_cyclicBufferPos < delta ? _cyclicBufferSize : 0)) << 1);
      const Byte *pb = cur - delta;
      unsigned len = (len0 < len1 ? len0 : len1);
      if (pb[len] == cur[len])
      {
        while (++len != lenLimit)
          if (pb[len] != cur[len])
            break;
        {
          if (len == lenLimit)
          {
            *ptr1 = pair[0];
            *ptr0 = pair[1];
            return;
          }
        }
      }
      if (pb[len] < cur[len])
      {
        *ptr1 = curMatch;
        curMatch = pair[1];
        ptr1 = pair + 1;
        len1 = len;
      }
      else
      {
        *ptr0 = curMatch;
        curMatch = pair[0];
        ptr0 = pair;
        len0 = len;
      }
    }
  }
  while(--cutValue && cmCheck < curMatch);
  
  *ptr0 = *ptr1 = kEmptyHashValue;
  return;
}


#define MOVE_POS \
  p->cyclicBufferPos++; \
  p->buffer++; \
  { const UInt32 pos1 = p->pos + 1; \
    p->pos = pos1; \
    if (pos1 == p->posLimit) MatchFinder_CheckLimits(p); }

#define MOVE_POS_RET MOVE_POS return distances;

Z7_NO_INLINE
static void MatchFinder_MovePos(CMatchFinder *p)
{
  /* we go here at the end of stream data, when (avail < num_hash_bytes)
     We don't update sons[cyclicBufferPos << btMode].
     So (sons) record will contain junk. And we cannot resume match searching
     to normal operation, even if we will provide more input data in buffer.
     p->sons[p->cyclicBufferPos << p->btMode] = 0;  // kEmptyHashValue
     if (p->btMode)
        p->sons[(p->cyclicBufferPos << p->btMode) + 1] = 0;  // kEmptyHashValue
  */
  MOVE_POS
}

#define GET_MATCHES_HEADER2(minLen, ret_op) \
  UInt32 hv; const Byte *cur; UInt32 curMatch; \
  UInt32 lenLimit = p->lenLimit; \
  if (lenLimit < minLen) { MatchFinder_MovePos(p);  ret_op; } \
  cur = p->buffer;

#define GET_MATCHES_HEADER(minLen) GET_MATCHES_HEADER2(minLen, return distances)
#define SKIP_HEADER(minLen)  \
  do { GET_MATCHES_HEADER2(minLen, continue)

#define MF_PARAMS(p)  lenLimit, curMatch, p->pos, p->buffer, p->son, \
    p->cyclicBufferPos, p->cyclicBufferSize, p->cutValue

#define SKIP_FOOTER  \
    SkipMatchesSpec(MF_PARAMS(p)); \
    MOVE_POS \
  } while (--num);

#define GET_MATCHES_FOOTER_BASE(_maxLen_, func) \
  distances = func(MF_PARAMS(p), distances, (UInt32)_maxLen_); \
  MOVE_POS_RET

#define GET_MATCHES_FOOTER_BT(_maxLen_) \
  GET_MATCHES_FOOTER_BASE(_maxLen_, GetMatchesSpec1)

#define GET_MATCHES_FOOTER_HC(_maxLen_) \
  GET_MATCHES_FOOTER_BASE(_maxLen_, Hc_GetMatchesSpec)



#define UPDATE_maxLen { \
    const ptrdiff_t diff = (ptrdiff_t)0 - (ptrdiff_t)d2; \
    const Byte *c = cur + maxLen; \
    const Byte *lim = cur + lenLimit; \
    for (; c != lim; c++) if (*(c + diff) != *c) break; \
    maxLen = (unsigned)(c - cur); }

static UInt32* Bt2_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  GET_MATCHES_HEADER(2)
  HASH2_CALC
  curMatch = p->hash[hv];
  p->hash[hv] = p->pos;
  GET_MATCHES_FOOTER_BT(1)
}

UInt32* Bt3Zip_MatchFinder_GetMatches(CMatchFinder *p, UInt32 *distances)
{
  GET_MATCHES_HEADER(3)
  HASH_ZIP_CALC
  curMatch = p->hash[hv];
  p->hash[hv] = p->pos;
  GET_MATCHES_FOOTER_BT(2)
}


#define SET_mmm  \
  mmm = p->cyclicBufferSize; \
  if (pos < mmm) \
    mmm = pos;


static UInt32* Bt3_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  UInt32 mmm;
  UInt32 h2, d2, pos;
  unsigned maxLen;
  UInt32 *hash;
  GET_MATCHES_HEADER(3)

  HASH3_CALC

  hash = p->hash;
  pos = p->pos;

  d2 = pos - hash[h2];

  curMatch = (hash + kFix3HashSize)[hv];
  
  hash[h2] = pos;
  (hash + kFix3HashSize)[hv] = pos;

  SET_mmm

  maxLen = 2;

  if (d2 < mmm && *(cur - d2) == *cur)
  {
    UPDATE_maxLen
    distances[0] = (UInt32)maxLen;
    distances[1] = d2 - 1;
    distances += 2;
    if (maxLen == lenLimit)
    {
      SkipMatchesSpec(MF_PARAMS(p));
      MOVE_POS_RET
    }
  }
  
  GET_MATCHES_FOOTER_BT(maxLen)
}


static UInt32* Bt4_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  UInt32 mmm;
  UInt32 h2, h3, d2, d3, pos;
  unsigned maxLen;
  UInt32 *hash;
  GET_MATCHES_HEADER(4)

  HASH4_CALC

  hash = p->hash;
  pos = p->pos;

  d2 = pos - hash                  [h2];
  d3 = pos - (hash + kFix3HashSize)[h3];
  curMatch = (hash + kFix4HashSize)[hv];

  hash                  [h2] = pos;
  (hash + kFix3HashSize)[h3] = pos;
  (hash + kFix4HashSize)[hv] = pos;

  SET_mmm

  maxLen = 3;
  
  for (;;)
  {
    if (d2 < mmm && *(cur - d2) == *cur)
    {
      distances[0] = 2;
      distances[1] = d2 - 1;
      distances += 2;
      if (*(cur - d2 + 2) == cur[2])
      {
        // distances[-2] = 3;
      }
      else if (d3 < mmm && *(cur - d3) == *cur)
      {
        d2 = d3;
        distances[1] = d3 - 1;
        distances += 2;
      }
      else
        break;
    }
    else if (d3 < mmm && *(cur - d3) == *cur)
    {
      d2 = d3;
      distances[1] = d3 - 1;
      distances += 2;
    }
    else
      break;
  
    UPDATE_maxLen
    distances[-2] = (UInt32)maxLen;
    if (maxLen == lenLimit)
    {
      SkipMatchesSpec(MF_PARAMS(p));
      MOVE_POS_RET
    }
    break;
  }
  
  GET_MATCHES_FOOTER_BT(maxLen)
}


static UInt32* Bt5_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  UInt32 mmm;
  UInt32 h2, h3, d2, d3, pos;
  unsigned maxLen;
  UInt32 *hash;
  GET_MATCHES_HEADER(5)

  HASH5_CALC

  hash = p->hash;
  pos = p->pos;

  d2 = pos - hash                  [h2];
  d3 = pos - (hash + kFix3HashSize)[h3];
  // d4 = pos - (hash + kFix4HashSize)[h4];

  curMatch = (hash + kFix5HashSize)[hv];

  hash                  [h2] = pos;
  (hash + kFix3HashSize)[h3] = pos;
  // (hash + kFix4HashSize)[h4] = pos;
  (hash + kFix5HashSize)[hv] = pos;

  SET_mmm

  maxLen = 4;

  for (;;)
  {
    if (d2 < mmm && *(cur - d2) == *cur)
    {
      distances[0] = 2;
      distances[1] = d2 - 1;
      distances += 2;
      if (*(cur - d2 + 2) == cur[2])
      {
      }
      else if (d3 < mmm && *(cur - d3) == *cur)
      {
        distances[1] = d3 - 1;
        distances += 2;
        d2 = d3;
      }
      else
        break;
    }
    else if (d3 < mmm && *(cur - d3) == *cur)
    {
      distances[1] = d3 - 1;
      distances += 2;
      d2 = d3;
    }
    else
      break;

    distances[-2] = 3;
    if (*(cur - d2 + 3) != cur[3])
      break;
    UPDATE_maxLen
    distances[-2] = (UInt32)maxLen;
    if (maxLen == lenLimit)
    {
      SkipMatchesSpec(MF_PARAMS(p));
      MOVE_POS_RET
    }
    break;
  }
  
  GET_MATCHES_FOOTER_BT(maxLen)
}


static UInt32* Hc4_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  UInt32 mmm;
  UInt32 h2, h3, d2, d3, pos;
  unsigned maxLen;
  UInt32 *hash;
  GET_MATCHES_HEADER(4)

  HASH4_CALC

  hash = p->hash;
  pos = p->pos;
  
  d2 = pos - hash                  [h2];
  d3 = pos - (hash + kFix3HashSize)[h3];
  curMatch = (hash + kFix4HashSize)[hv];

  hash                  [h2] = pos;
  (hash + kFix3HashSize)[h3] = pos;
  (hash + kFix4HashSize)[hv] = pos;

  SET_mmm

  maxLen = 3;

  for (;;)
  {
    if (d2 < mmm && *(cur - d2) == *cur)
    {
      distances[0] = 2;
      distances[1] = d2 - 1;
      distances += 2;
      if (*(cur - d2 + 2) == cur[2])
      {
        // distances[-2] = 3;
      }
      else if (d3 < mmm && *(cur - d3) == *cur)
      {
        d2 = d3;
        distances[1] = d3 - 1;
        distances += 2;
      }
      else
        break;
    }
    else if (d3 < mmm && *(cur - d3) == *cur)
    {
      d2 = d3;
      distances[1] = d3 - 1;
      distances += 2;
    }
    else
      break;

    UPDATE_maxLen
    distances[-2] = (UInt32)maxLen;
    if (maxLen == lenLimit)
    {
      p->son[p->cyclicBufferPos] = curMatch;
      MOVE_POS_RET
    }
    break;
  }
  
  GET_MATCHES_FOOTER_HC(maxLen)
}


static UInt32 * Hc5_MatchFinder_GetMatches(void *_p, UInt32 *distances)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  UInt32 mmm;
  UInt32 h2, h3, d2, d3, pos;
  unsigned maxLen;
  UInt32 *hash;
  GET_MATCHES_HEADER(5)

  HASH5_CALC

  hash = p->hash;
  pos = p->pos;

  d2 = pos - hash                  [h2];
  d3 = pos - (hash + kFix3HashSize)[h3];
  // d4 = pos - (hash + kFix4HashSize)[h4];

  curMatch = (hash + kFix5HashSize)[hv];

  hash                  [h2] = pos;
  (hash + kFix3HashSize)[h3] = pos;
  // (hash + kFix4HashSize)[h4] = pos;
  (hash + kFix5HashSize)[hv] = pos;

  SET_mmm
  
  maxLen = 4;

  for (;;)
  {
    if (d2 < mmm && *(cur - d2) == *cur)
    {
      distances[0] = 2;
      distances[1] = d2 - 1;
      distances += 2;
      if (*(cur - d2 + 2) == cur[2])
      {
      }
      else if (d3 < mmm && *(cur - d3) == *cur)
      {
        distances[1] = d3 - 1;
        distances += 2;
        d2 = d3;
      }
      else
        break;
    }
    else if (d3 < mmm && *(cur - d3) == *cur)
    {
      distances[1] = d3 - 1;
      distances += 2;
      d2 = d3;
    }
    else
      break;

    distances[-2] = 3;
    if (*(cur - d2 + 3) != cur[3])
      break;
    UPDATE_maxLen
    distances[-2] = (UInt32)maxLen;
    if (maxLen == lenLimit)
    {
      p->son[p->cyclicBufferPos] = curMatch;
      MOVE_POS_RET
    }
    break;
  }
  
  GET_MATCHES_FOOTER_HC(maxLen)
}


UInt32* Hc3Zip_MatchFinder_GetMatches(CMatchFinder *p, UInt32 *distances)
{
  GET_MATCHES_HEADER(3)
  HASH_ZIP_CALC
  curMatch = p->hash[hv];
  p->hash[hv] = p->pos;
  GET_MATCHES_FOOTER_HC(2)
}


static void Bt2_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  SKIP_HEADER(2)
  {
    HASH2_CALC
    curMatch = p->hash[hv];
    p->hash[hv] = p->pos;
  }
  SKIP_FOOTER
}

void Bt3Zip_MatchFinder_Skip(CMatchFinder *p, UInt32 num)
{
  SKIP_HEADER(3)
  {
    HASH_ZIP_CALC
    curMatch = p->hash[hv];
    p->hash[hv] = p->pos;
  }
  SKIP_FOOTER
}

static void Bt3_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  SKIP_HEADER(3)
  {
    UInt32 h2;
    UInt32 *hash;
    HASH3_CALC
    hash = p->hash;
    curMatch = (hash + kFix3HashSize)[hv];
    hash[h2] =
    (hash + kFix3HashSize)[hv] = p->pos;
  }
  SKIP_FOOTER
}

static void Bt4_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  SKIP_HEADER(4)
  {
    UInt32 h2, h3;
    UInt32 *hash;
    HASH4_CALC
    hash = p->hash;
    curMatch = (hash + kFix4HashSize)[hv];
    hash                  [h2] =
    (hash + kFix3HashSize)[h3] =
    (hash + kFix4HashSize)[hv] = p->pos;
  }
  SKIP_FOOTER
}

static void Bt5_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  SKIP_HEADER(5)
  {
    UInt32 h2, h3;
    UInt32 *hash;
    HASH5_CALC
    hash = p->hash;
    curMatch = (hash + kFix5HashSize)[hv];
    hash                  [h2] =
    (hash + kFix3HashSize)[h3] =
    // (hash + kFix4HashSize)[h4] =
    (hash + kFix5HashSize)[hv] = p->pos;
  }
  SKIP_FOOTER
}


#define HC_SKIP_HEADER(minLen) \
    do { if (p->lenLimit < minLen) { MatchFinder_MovePos(p); num--; continue; } { \
    const Byte *cur; \
    UInt32 *hash; \
    UInt32 *son; \
    UInt32 pos = p->pos; \
    UInt32 num2 = num; \
    /* (p->pos == p->posLimit) is not allowed here !!! */ \
    { const UInt32 rem = p->posLimit - pos; if (num2 >= rem) num2 = rem; } \
    num -= num2; \
    { const UInt32 cycPos = p->cyclicBufferPos; \
      son = p->son + cycPos; \
      p->cyclicBufferPos = cycPos + num2; } \
    cur = p->buffer; \
    hash = p->hash; \
    do { \
    UInt32 curMatch; \
    UInt32 hv;


#define HC_SKIP_FOOTER \
    cur++;  pos++;  *son++ = curMatch; \
    } while (--num2); \
    p->buffer = cur; \
    p->pos = pos; \
    if (pos == p->posLimit) MatchFinder_CheckLimits(p); \
    }} while(num); \


static void Hc4_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  HC_SKIP_HEADER(4)

    UInt32 h2, h3;
    HASH4_CALC
    curMatch = (hash + kFix4HashSize)[hv];
    hash                  [h2] =
    (hash + kFix3HashSize)[h3] =
    (hash + kFix4HashSize)[hv] = pos;
  
  HC_SKIP_FOOTER
}


static void Hc5_MatchFinder_Skip(void *_p, UInt32 num)
{
  CMatchFinder *p = (CMatchFinder *)_p;
  HC_SKIP_HEADER(5)
  
    UInt32 h2, h3;
    HASH5_CALC
    curMatch = (hash + kFix5HashSize)[hv];
    hash                  [h2] =
    (hash + kFix3HashSize)[h3] =
    // (hash + kFix4HashSize)[h4] =
    (hash + kFix5HashSize)[hv] = pos;
  
  HC_SKIP_FOOTER
}


void Hc3Zip_MatchFinder_Skip(CMatchFinder *p, UInt32 num)
{
  HC_SKIP_HEADER(3)

    HASH_ZIP_CALC
    curMatch = hash[hv];
    hash[hv] = pos;

  HC_SKIP_FOOTER
}


void MatchFinder_CreateVTable(CMatchFinder *p, IMatchFinder2 *vTable)
{
  vTable->Init = MatchFinder_Init;
  vTable->GetNumAvailableBytes = MatchFinder_GetNumAvailableBytes;
  vTable->GetPointerToCurrentPos = MatchFinder_GetPointerToCurrentPos;
  if (!p->btMode)
  {
    if (p->numHashBytes <= 4)
    {
      vTable->GetMatches = Hc4_MatchFinder_GetMatches;
      vTable->Skip = Hc4_MatchFinder_Skip;
    }
    else
    {
      vTable->GetMatches = Hc5_MatchFinder_GetMatches;
      vTable->Skip = Hc5_MatchFinder_Skip;
    }
  }
  else if (p->numHashBytes == 2)
  {
    vTable->GetMatches = Bt2_MatchFinder_GetMatches;
    vTable->Skip = Bt2_MatchFinder_Skip;
  }
  else if (p->numHashBytes == 3)
  {
    vTable->GetMatches = Bt3_MatchFinder_GetMatches;
    vTable->Skip = Bt3_MatchFinder_Skip;
  }
  else if (p->numHashBytes == 4)
  {
    vTable->GetMatches = Bt4_MatchFinder_GetMatches;
    vTable->Skip = Bt4_MatchFinder_Skip;
  }
  else
  {
    vTable->GetMatches = Bt5_MatchFinder_GetMatches;
    vTable->Skip = Bt5_MatchFinder_Skip;
  }
}



void LzFindPrepare(void)
{
  #ifndef FORCE_LZFIND_SATUR_SUB_128
  #ifdef USE_LZFIND_SATUR_SUB_128
  LZFIND_SATUR_SUB_CODE_FUNC f = NULL;
  #ifdef MY_CPU_ARM_OR_ARM64
  {
    if (CPU_IsSupported_NEON())
    {
      // #pragma message ("=== LzFind NEON")
      PRF(printf("\n=== LzFind NEON\n"));
      f = LzFind_SaturSub_128;
    }
    // f = 0; // for debug
  }
  #else // MY_CPU_ARM_OR_ARM64
  if (CPU_IsSupported_SSE41())
  {
    // #pragma message ("=== LzFind SSE41")
    PRF(printf("\n=== LzFind SSE41\n"));
    f = LzFind_SaturSub_128;

    #ifdef USE_LZFIND_SATUR_SUB_256
    if (CPU_IsSupported_AVX2())
    {
      // #pragma message ("=== LzFind AVX2")
      PRF(printf("\n=== LzFind AVX2\n"));
      f = LzFind_SaturSub_256;
    }
    #endif
  }
  #endif // MY_CPU_ARM_OR_ARM64
  g_LzFind_SaturSub = f;
  #endif // USE_LZFIND_SATUR_SUB_128
  #endif // FORCE_LZFIND_SATUR_SUB_128
}


#undef MOVE_POS
#undef MOVE_POS_RET
#undef PRF

/* ---- embedded source: LzFindOpt.c ---- */
/* LzFindOpt.c -- multithreaded Match finder for LZ algorithms
2023-04-02 : Igor Pavlov : Public domain */



// #include "LzFindMt.h"

// #define LOG_ITERS

// #define LOG_THREAD

#ifdef LOG_THREAD
#include <stdio.h>
#define PRF(x) x
#else
// #define PRF(x)
#endif

#ifdef LOG_ITERS
#include <stdio.h>
UInt64 g_NumIters_Tree;
UInt64 g_NumIters_Loop;
UInt64 g_NumIters_Bytes;
#define LOG_ITER(x) x
#else
#define LOG_ITER(x)
#endif

// ---------- BT THREAD ----------

#define USE_SON_PREFETCH
#define USE_LONG_MATCH_OPT

#define kEmptyHashValue 0

// #define CYC_TO_POS_OFFSET 0

// #define CYC_TO_POS_OFFSET 1 // for debug

/*
Z7_NO_INLINE
UInt32 * Z7_FASTCALL GetMatchesSpecN_1(const Byte *lenLimit, size_t pos, const Byte *cur, CLzRef *son,
    UInt32 _cutValue, UInt32 *d, size_t _maxLen, const UInt32 *hash, const UInt32 *limit, const UInt32 *size, UInt32 *posRes)
{
  do
  {
    UInt32 delta;
    if (hash == size)
      break;
    delta = *hash++;

    if (delta == 0 || delta > (UInt32)pos)
      return NULL;

    lenLimit++;

    if (delta == (UInt32)pos)
    {
      CLzRef *ptr1 = son + ((size_t)pos << 1) - CYC_TO_POS_OFFSET * 2;
      *d++ = 0;
      ptr1[0] = kEmptyHashValue;
      ptr1[1] = kEmptyHashValue;
    }
else
{
  UInt32 *_distances = ++d;

  CLzRef *ptr0 = son + ((size_t)(pos) << 1) - CYC_TO_POS_OFFSET * 2 + 1;
  CLzRef *ptr1 = son + ((size_t)(pos) << 1) - CYC_TO_POS_OFFSET * 2;

  const Byte *len0 = cur, *len1 = cur;
  UInt32 cutValue = _cutValue;
  const Byte *maxLen = cur + _maxLen;

  for (LOG_ITER(g_NumIters_Tree++);;)
  {
    LOG_ITER(g_NumIters_Loop++);
    {
      const ptrdiff_t diff = (ptrdiff_t)0 - (ptrdiff_t)delta;
      CLzRef *pair = son + ((size_t)(((ptrdiff_t)pos - CYC_TO_POS_OFFSET) + diff) << 1);
      const Byte *len = (len0 < len1 ? len0 : len1);

    #ifdef USE_SON_PREFETCH
      const UInt32 pair0 = *pair;
    #endif

      if (len[diff] == len[0])
      {
        if (++len != lenLimit && len[diff] == len[0])
          while (++len != lenLimit)
          {
            LOG_ITER(g_NumIters_Bytes++);
            if (len[diff] != len[0])
              break;
          }
        if (maxLen < len)
        {
          maxLen = len;
          *d++ = (UInt32)(len - cur);
          *d++ = delta - 1;
          
          if (len == lenLimit)
          {
            const UInt32 pair1 = pair[1];
            *ptr1 =
              #ifdef USE_SON_PREFETCH
                pair0;
              #else
                pair[0];
              #endif
            *ptr0 = pair1;

            _distances[-1] = (UInt32)(d - _distances);

            #ifdef USE_LONG_MATCH_OPT

                if (hash == size || *hash != delta || lenLimit[diff] != lenLimit[0] || d >= limit)
                  break;

            {
              for (;;)
              {
                hash++;
                pos++;
                cur++;
                lenLimit++;
                {
                  CLzRef *ptr = son + ((size_t)(pos) << 1) - CYC_TO_POS_OFFSET * 2;
                  #if 0
                  *(UInt64 *)(void *)ptr = ((const UInt64 *)(const void *)ptr)[diff];
                  #else
                  const UInt32 p0 = ptr[0 + (diff * 2)];
                  const UInt32 p1 = ptr[1 + (diff * 2)];
                  ptr[0] = p0;
                  ptr[1] = p1;
                  // ptr[0] = ptr[0 + (diff * 2)];
                  // ptr[1] = ptr[1 + (diff * 2)];
                  #endif
                }
                // PrintSon(son + 2, pos - 1);
                // printf("\npos = %x delta = %x\n", pos, delta);
                len++;
                *d++ = 2;
                *d++ = (UInt32)(len - cur);
                *d++ = delta - 1;
                if (hash == size || *hash != delta || lenLimit[diff] != lenLimit[0] || d >= limit)
                  break;
              }
            }
            #endif

            break;
          }
        }
      }

      {
        const UInt32 curMatch = (UInt32)pos - delta; // (UInt32)(pos + diff);
        if (len[diff] < len[0])
        {
          delta = pair[1];
          if (delta >= curMatch)
            return NULL;
          *ptr1 = curMatch;
          ptr1 = pair + 1;
          len1 = len;
        }
        else
        {
          delta = *pair;
          if (delta >= curMatch)
            return NULL;
          *ptr0 = curMatch;
          ptr0 = pair;
          len0 = len;
        }

        delta = (UInt32)pos - delta;
 
        if (--cutValue == 0 || delta >= pos)
        {
          *ptr0 = *ptr1 = kEmptyHashValue;
          _distances[-1] = (UInt32)(d - _distances);
          break;
        }
      }
    }
  } // for (tree iterations)
}
    pos++;
    cur++;
  }
  while (d < limit);
  *posRes = (UInt32)pos;
  return d;
}
*/

/* define cbs if you use 2 functions.
       GetMatchesSpecN_1() :  (pos <  _cyclicBufferSize)
       GetMatchesSpecN_2() :  (pos >= _cyclicBufferSize)

  do not define cbs if you use 1 function:
       GetMatchesSpecN_2()
*/

// #define cbs _cyclicBufferSize

/*
  we use size_t for (pos) and (_cyclicBufferPos_ instead of UInt32
  to eliminate "movsx" BUG in old MSVC x64 compiler.
*/

UInt32 * Z7_FASTCALL GetMatchesSpecN_2(const Byte *lenLimit, size_t pos, const Byte *cur, CLzRef *son,
    UInt32 _cutValue, UInt32 *d, size_t _maxLen, const UInt32 *hash, const UInt32 *limit, const UInt32 *size,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize,
    UInt32 *posRes);

Z7_NO_INLINE
UInt32 * Z7_FASTCALL GetMatchesSpecN_2(const Byte *lenLimit, size_t pos, const Byte *cur, CLzRef *son,
    UInt32 _cutValue, UInt32 *d, size_t _maxLen, const UInt32 *hash, const UInt32 *limit, const UInt32 *size,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize,
    UInt32 *posRes)
{
  do // while (hash != size)
  {
    UInt32 delta;
    
  #ifndef cbs
    UInt32 cbs;
  #endif

    if (hash == size)
      break;

    delta = *hash++;

    if (delta == 0)
      return NULL;

    lenLimit++;

  #ifndef cbs
    cbs = _cyclicBufferSize;
    if ((UInt32)pos < cbs)
    {
      if (delta > (UInt32)pos)
        return NULL;
      cbs = (UInt32)pos;
    }
  #endif

    if (delta >= cbs)
    {
      CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);
      *d++ = 0;
      ptr1[0] = kEmptyHashValue;
      ptr1[1] = kEmptyHashValue;
    }
else
{
  UInt32 *_distances = ++d;

  CLzRef *ptr0 = son + ((size_t)_cyclicBufferPos << 1) + 1;
  CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);

  UInt32 cutValue = _cutValue;
  const Byte *len0 = cur, *len1 = cur;
  const Byte *maxLen = cur + _maxLen;

  // if (cutValue == 0) { *ptr0 = *ptr1 = kEmptyHashValue; } else
  for (LOG_ITER(g_NumIters_Tree++);;)
  {
    LOG_ITER(g_NumIters_Loop++);
    {
      // SPEC code
      CLzRef *pair = son + ((size_t)((ptrdiff_t)_cyclicBufferPos - (ptrdiff_t)delta
          + (ptrdiff_t)(UInt32)(_cyclicBufferPos < delta ? cbs : 0)
          ) << 1);

      const ptrdiff_t diff = (ptrdiff_t)0 - (ptrdiff_t)delta;
      const Byte *len = (len0 < len1 ? len0 : len1);

    #ifdef USE_SON_PREFETCH
      const UInt32 pair0 = *pair;
    #endif

      if (len[diff] == len[0])
      {
        if (++len != lenLimit && len[diff] == len[0])
          while (++len != lenLimit)
          {
            LOG_ITER(g_NumIters_Bytes++);
            if (len[diff] != len[0])
              break;
          }
        if (maxLen < len)
        {
          maxLen = len;
          *d++ = (UInt32)(len - cur);
          *d++ = delta - 1;
          
          if (len == lenLimit)
          {
            const UInt32 pair1 = pair[1];
            *ptr1 =
              #ifdef USE_SON_PREFETCH
                pair0;
              #else
                pair[0];
              #endif
            *ptr0 = pair1;

            _distances[-1] = (UInt32)(d - _distances);

            #ifdef USE_LONG_MATCH_OPT

                if (hash == size || *hash != delta || lenLimit[diff] != lenLimit[0] || d >= limit)
                  break;

            {
              for (;;)
              {
                *d++ = 2;
                *d++ = (UInt32)(lenLimit - cur);
                *d++ = delta - 1;
                cur++;
                lenLimit++;
                // SPEC
                _cyclicBufferPos++;
                {
                  // SPEC code
                  CLzRef *dest = son + ((size_t)(_cyclicBufferPos) << 1);
                  const CLzRef *src = dest + ((diff
                      + (ptrdiff_t)(UInt32)((_cyclicBufferPos < delta) ? cbs : 0)) << 1);
                  // CLzRef *ptr = son + ((size_t)(pos) << 1) - CYC_TO_POS_OFFSET * 2;
                  #if 0
                  *(UInt64 *)(void *)dest = *((const UInt64 *)(const void *)src);
                  #else
                  const UInt32 p0 = src[0];
                  const UInt32 p1 = src[1];
                  dest[0] = p0;
                  dest[1] = p1;
                  #endif
                }
                pos++;
                hash++;
                if (hash == size || *hash != delta || lenLimit[diff] != lenLimit[0] || d >= limit)
                  break;
              } // for() end for long matches
            }
            #endif

            break; // break from TREE iterations
          }
        }
      }
      {
        const UInt32 curMatch = (UInt32)pos - delta; // (UInt32)(pos + diff);
        if (len[diff] < len[0])
        {
          delta = pair[1];
          *ptr1 = curMatch;
          ptr1 = pair + 1;
          len1 = len;
          if (delta >= curMatch)
            return NULL;
        }
        else
        {
          delta = *pair;
          *ptr0 = curMatch;
          ptr0 = pair;
          len0 = len;
          if (delta >= curMatch)
            return NULL;
        }
        delta = (UInt32)pos - delta;
 
        if (--cutValue == 0 || delta >= cbs)
        {
          *ptr0 = *ptr1 = kEmptyHashValue;
          _distances[-1] = (UInt32)(d - _distances);
          break;
        }
      }
    }
  } // for (tree iterations)
}
    pos++;
    _cyclicBufferPos++;
    cur++;
  }
  while (d < limit);
  *posRes = (UInt32)pos;
  return d;
}



/*
typedef UInt32 uint32plus; // size_t

UInt32 * Z7_FASTCALL GetMatchesSpecN_3(uint32plus lenLimit, size_t pos, const Byte *cur, CLzRef *son,
    UInt32 _cutValue, UInt32 *d, uint32plus _maxLen, const UInt32 *hash, const UInt32 *limit, const UInt32 *size,
    size_t _cyclicBufferPos, UInt32 _cyclicBufferSize,
    UInt32 *posRes)
{
  do // while (hash != size)
  {
    UInt32 delta;

  #ifndef cbs
    UInt32 cbs;
  #endif

    if (hash == size)
      break;

    delta = *hash++;

    if (delta == 0)
      return NULL;

  #ifndef cbs
    cbs = _cyclicBufferSize;
    if ((UInt32)pos < cbs)
    {
      if (delta > (UInt32)pos)
        return NULL;
      cbs = (UInt32)pos;
    }
  #endif
    
    if (delta >= cbs)
    {
      CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);
      *d++ = 0;
      ptr1[0] = kEmptyHashValue;
      ptr1[1] = kEmptyHashValue;
    }
else
{
  CLzRef *ptr0 = son + ((size_t)_cyclicBufferPos << 1) + 1;
  CLzRef *ptr1 = son + ((size_t)_cyclicBufferPos << 1);
  UInt32 *_distances = ++d;
  uint32plus len0 = 0, len1 = 0;
  UInt32 cutValue = _cutValue;
  uint32plus maxLen = _maxLen;
  // lenLimit++; // const Byte *lenLimit = cur + _lenLimit;

  for (LOG_ITER(g_NumIters_Tree++);;)
  {
    LOG_ITER(g_NumIters_Loop++);
    {
      // const ptrdiff_t diff = (ptrdiff_t)0 - (ptrdiff_t)delta;
      CLzRef *pair = son + ((size_t)((ptrdiff_t)_cyclicBufferPos - delta
          + (ptrdiff_t)(UInt32)(_cyclicBufferPos < delta ? cbs : 0)
          ) << 1);
      const Byte *pb = cur - delta;
      uint32plus len = (len0 < len1 ? len0 : len1);

    #ifdef USE_SON_PREFETCH
      const UInt32 pair0 = *pair;
    #endif

      if (pb[len] == cur[len])
      {
        if (++len != lenLimit && pb[len] == cur[len])
          while (++len != lenLimit)
            if (pb[len] != cur[len])
              break;
        if (maxLen < len)
        {
          maxLen = len;
          *d++ = (UInt32)len;
          *d++ = delta - 1;
          if (len == lenLimit)
          {
            {
              const UInt32 pair1 = pair[1];
              *ptr0 = pair1;
              *ptr1 =
              #ifdef USE_SON_PREFETCH
                pair0;
              #else
                pair[0];
              #endif
            }

            _distances[-1] = (UInt32)(d - _distances);

            #ifdef USE_LONG_MATCH_OPT

                if (hash == size || *hash != delta || pb[lenLimit] != cur[lenLimit] || d >= limit)
                  break;

            {
              const ptrdiff_t diff = (ptrdiff_t)0 - (ptrdiff_t)delta;
              for (;;)
              {
                *d++ = 2;
                *d++ = (UInt32)lenLimit;
                *d++ = delta - 1;
                _cyclicBufferPos++;
                {
                  CLzRef *dest = son + ((size_t)_cyclicBufferPos << 1);
                  const CLzRef *src = dest + ((diff +
                      (ptrdiff_t)(UInt32)(_cyclicBufferPos < delta ? cbs : 0)) << 1);
                #if 0
                  *(UInt64 *)(void *)dest = *((const UInt64 *)(const void *)src);
                #else
                  const UInt32 p0 = src[0];
                  const UInt32 p1 = src[1];
                  dest[0] = p0;
                  dest[1] = p1;
                #endif
                }
                hash++;
                pos++;
                cur++;
                pb++;
                if (hash == size || *hash != delta || pb[lenLimit] != cur[lenLimit] || d >= limit)
                  break;
              }
            }
            #endif

            break;
          }
        }
      }
      {
        const UInt32 curMatch = (UInt32)pos - delta;
        if (pb[len] < cur[len])
        {
          delta = pair[1];
          *ptr1 = curMatch;
          ptr1 = pair + 1;
          len1 = len;
        }
        else
        {
          delta = *pair;
          *ptr0 = curMatch;
          ptr0 = pair;
          len0 = len;
        }

        {
          if (delta >= curMatch)
            return NULL;
          delta = (UInt32)pos - delta;
          if (delta >= cbs
              // delta >= _cyclicBufferSize || delta >= pos
              || --cutValue == 0)
          {
            *ptr0 = *ptr1 = kEmptyHashValue;
            _distances[-1] = (UInt32)(d - _distances);
            break;
          }
        }
      }
    }
  } // for (tree iterations)
}
    pos++;
    _cyclicBufferPos++;
    cur++;
  }
  while (d < limit);
  *posRes = (UInt32)pos;
  return d;
}
*/

/* ---- embedded source: LzmaEnc.c ---- */
/* LzmaEnc.c -- LZMA Encoder
Igor Pavlov : Public domain */


#include <string.h>

/* #define SHOW_STAT */
/* #define SHOW_STAT2 */

#if defined(SHOW_STAT) || defined(SHOW_STAT2)
#include <stdio.h>
#endif


#ifndef Z7_ST
#endif

/* the following LzmaEnc_* declarations is internal LZMA interface for LZMA2 encoder */

SRes LzmaEnc_PrepareForLzma2(CLzmaEncHandle p, ISeqInStreamPtr inStream, UInt32 keepWindowSize,
    ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_MemPrepare(CLzmaEncHandle p, const Byte *src, SizeT srcLen,
    UInt32 keepWindowSize, ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_CodeOneMemBlock(CLzmaEncHandle p, BoolInt reInit,
    Byte *dest, size_t *destLen, UInt32 desiredPackSize, UInt32 *unpackSize);
const Byte *LzmaEnc_GetCurBuf(CLzmaEncHandle p);
void LzmaEnc_Finish(CLzmaEncHandle p);
void LzmaEnc_SaveState(CLzmaEncHandle p);
void LzmaEnc_RestoreState(CLzmaEncHandle p);

#ifdef SHOW_STAT
static unsigned g_STAT_OFFSET = 0;
#endif

/* for good normalization speed we still reserve 256 MB before 4 GB range */
#define kLzmaMaxHistorySize ((UInt32)15 << 28)

// #define kNumTopBits 24
#define kTopValue ((UInt32)1 << 24)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5
#define kProbInitValue (kBitModelTotal >> 1)

#define kNumMoveReducingBits 4
#define kNumBitPriceShiftBits 4
// #define kBitPrice (1 << kNumBitPriceShiftBits)

#define REP_LEN_COUNT 64

void LzmaEncProps_Init(CLzmaEncProps *p)
{
  p->level = 5;
  p->dictSize = p->mc = 0;
  p->reduceSize = (UInt64)(Int64)-1;
  p->lc = p->lp = p->pb = p->algo = p->fb = p->btMode = p->numHashBytes = p->numThreads = -1;
  p->numHashOutBits = 0;
  p->writeEndMark = 0;
  p->affinityGroup = -1;
  p->affinity = 0;
  p->affinityInGroup = 0;
}

void LzmaEncProps_Normalize(CLzmaEncProps *p)
{
  int level = p->level;
  if (level < 0) level = 5;
  p->level = level;
  
  if (p->dictSize == 0)
    p->dictSize = (unsigned)level <= 4 ?
        (UInt32)1 << (level * 2 + 16) :
        (unsigned)level <= sizeof(size_t) / 2 + 4 ?
          (UInt32)1 << (level + 20) :
          (UInt32)1 << (sizeof(size_t) / 2 + 24);

  if (p->dictSize > p->reduceSize)
  {
    UInt32 v = (UInt32)p->reduceSize;
    const UInt32 kReduceMin = ((UInt32)1 << 12);
    if (v < kReduceMin)
      v = kReduceMin;
    if (p->dictSize > v)
      p->dictSize = v;
  }

  if (p->lc < 0) p->lc = 3;
  if (p->lp < 0) p->lp = 0;
  if (p->pb < 0) p->pb = 2;

  if (p->algo < 0) p->algo = (unsigned)level < 5 ? 0 : 1;
  if (p->fb < 0) p->fb = (unsigned)level < 7 ? 32 : 64;
  if (p->btMode < 0) p->btMode = (p->algo == 0 ? 0 : 1);
  if (p->numHashBytes < 0) p->numHashBytes = (p->btMode ? 4 : 5);
  if (p->mc == 0) p->mc = (16 + ((unsigned)p->fb >> 1)) >> (p->btMode ? 0 : 1);
  
  if (p->numThreads < 0)
    p->numThreads =
      #ifndef Z7_ST
      ((p->btMode && p->algo) ? 2 : 1);
      #else
      1;
      #endif
}

UInt32 LzmaEncProps_GetDictSize(const CLzmaEncProps *props2)
{
  CLzmaEncProps props = *props2;
  LzmaEncProps_Normalize(&props);
  return props.dictSize;
}


/*
x86/x64:

BSR:
  IF (SRC == 0) ZF = 1, DEST is undefined;
                  AMD : DEST is unchanged;
  IF (SRC != 0) ZF = 0; DEST is index of top non-zero bit
  BSR is slow in some processors

LZCNT:
  IF (SRC  == 0) CF = 1, DEST is size_in_bits_of_register(src) (32 or 64)
  IF (SRC  != 0) CF = 0, DEST = num_lead_zero_bits
  IF (DEST == 0) ZF = 1;

LZCNT works only in new processors starting from Haswell.
if LZCNT is not supported by processor, then it's executed as BSR.
LZCNT can be faster than BSR, if supported.
*/

// #define LZMA_LOG_BSR

#if defined(MY_CPU_ARM_OR_ARM64) /* || defined(MY_CPU_X86_OR_AMD64) */

  #if (defined(__clang__) && (__clang_major__ >= 6)) \
      || (defined(__GNUC__) && (__GNUC__ >= 6))
      #define LZMA_LOG_BSR
  #elif defined(_MSC_VER) && (_MSC_VER >= 1300)
    // #if defined(MY_CPU_ARM_OR_ARM64)
      #define LZMA_LOG_BSR
    // #endif
  #endif
#endif

// #include <intrin.h>

#ifdef LZMA_LOG_BSR

#if defined(__clang__) \
    || defined(__GNUC__)

/*
  C code:                  : (30 - __builtin_clz(x))
    gcc9/gcc10 for x64 /x86  : 30 - (bsr(x) xor 31)
    clang10 for x64          : 31 + (bsr(x) xor -32)
*/

  #define MY_clz(x)  ((unsigned)__builtin_clz(x))
  // __lzcnt32
  // __builtin_ia32_lzcnt_u32

#else  // #if defined(_MSC_VER)

  #ifdef MY_CPU_ARM_OR_ARM64

    #define MY_clz  _CountLeadingZeros

  #else // if defined(MY_CPU_X86_OR_AMD64)

    // #define MY_clz  __lzcnt  // we can use lzcnt (unsupported by old CPU)
    // _BitScanReverse code is not optimal for some MSVC compilers
    #define BSR2_RET(pos, res) { unsigned long zz; _BitScanReverse(&zz, (pos)); zz--; \
      res = (zz + zz) + (pos >> zz); }

  #endif // MY_CPU_X86_OR_AMD64

#endif // _MSC_VER


#ifndef BSR2_RET

    #define BSR2_RET(pos, res) { unsigned zz = 30 - MY_clz(pos); \
      res = (zz + zz) + (pos >> zz); }

#endif


unsigned GetPosSlot1(UInt32 pos);
unsigned GetPosSlot1(UInt32 pos)
{
  unsigned res;
  BSR2_RET(pos, res)
  return res;
}
#define GetPosSlot2(pos, res) { BSR2_RET(pos, res) }
#define GetPosSlot(pos, res) { if (pos < 2) res = pos; else BSR2_RET(pos, res) }


#else // ! LZMA_LOG_BSR

#define kNumLogBits (11 + sizeof(size_t) / 8 * 3)

#define kDicLogSizeMaxCompress ((kNumLogBits - 1) * 2 + 7)

static void LzmaEnc_FastPosInit(Byte *g_FastPos)
{
  unsigned slot;
  g_FastPos[0] = 0;
  g_FastPos[1] = 1;
  g_FastPos += 2;
  
  for (slot = 2; slot < kNumLogBits * 2; slot++)
  {
    size_t k = ((size_t)1 << ((slot >> 1) - 1));
    size_t j;
    for (j = 0; j < k; j++)
      g_FastPos[j] = (Byte)slot;
    g_FastPos += k;
  }
}

/* we can use ((limit - pos) >> 31) only if (pos < ((UInt32)1 << 31)) */
/*
#define BSR2_RET(pos, res) { unsigned zz = 6 + ((kNumLogBits - 1) & \
  (0 - (((((UInt32)1 << (kNumLogBits + 6)) - 1) - pos) >> 31))); \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }
*/

/*
#define BSR2_RET(pos, res) { unsigned zz = 6 + ((kNumLogBits - 1) & \
  (0 - (((((UInt32)1 << (kNumLogBits)) - 1) - (pos >> 6)) >> 31))); \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }
*/

#define BSR2_RET(pos, res) { unsigned zz = (pos < (1 << (kNumLogBits + 6))) ? 6 : 6 + kNumLogBits - 1; \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }

/*
#define BSR2_RET(pos, res) { res = (pos < (1 << (kNumLogBits + 6))) ? \
  p->g_FastPos[pos >> 6] + 12 : \
  p->g_FastPos[pos >> (6 + kNumLogBits - 1)] + (6 + (kNumLogBits - 1)) * 2; }
*/

#define GetPosSlot1(pos) p->g_FastPos[pos]
#define GetPosSlot2(pos, res) { BSR2_RET(pos, res); }
#define GetPosSlot(pos, res) { if (pos < kNumFullDistances) res = p->g_FastPos[pos & (kNumFullDistances - 1)]; else BSR2_RET(pos, res); }

#endif // LZMA_LOG_BSR


#define LZMA_NUM_REPS 4

typedef UInt16 CState;
typedef UInt16 CExtra;

typedef struct
{
  UInt32 price;
  CState state;
  CExtra extra;
      // 0   : normal
      // 1   : LIT : MATCH
      // > 1 : MATCH (extra-1) : LIT : REP0 (len)
  UInt32 len;
  UInt32 dist;
  UInt32 reps[LZMA_NUM_REPS];
} COptimal;


// 18.06
#define kNumOpts (1 << 11)
#define kPackReserve (kNumOpts * 8)
// #define kNumOpts (1 << 12)
// #define kPackReserve (1 + kNumOpts * 2)

#define kNumLenToPosStates 4
#define kNumPosSlotBits 6
// #define kDicLogSizeMin 0
#define kDicLogSizeMax 32
#define kDistTableSizeMax (kDicLogSizeMax * 2)

#define kNumAlignBits 4
#define kAlignTableSize (1 << kNumAlignBits)
#define kAlignMask (kAlignTableSize - 1)

#define kStartPosModelIndex 4
#define kEndPosModelIndex 14
#define kNumFullDistances (1 << (kEndPosModelIndex >> 1))

typedef
#ifdef Z7_LZMA_PROB32
  UInt32
#else
  UInt16
#endif
  CLzmaProb;

#define LZMA_PB_MAX 4
#define LZMA_LC_MAX 8
#define LZMA_LP_MAX 4

#define LZMA_NUM_PB_STATES_MAX (1 << LZMA_PB_MAX)

#define kLenNumLowBits 3
#define kLenNumLowSymbols (1 << kLenNumLowBits)
#define kLenNumHighBits 8
#define kLenNumHighSymbols (1 << kLenNumHighBits)
#define kLenNumSymbolsTotal (kLenNumLowSymbols * 2 + kLenNumHighSymbols)

#define LZMA_MATCH_LEN_MIN 2
#define LZMA_MATCH_LEN_MAX (LZMA_MATCH_LEN_MIN + kLenNumSymbolsTotal - 1)

#define kNumStates 12


typedef struct
{
  CLzmaProb low[LZMA_NUM_PB_STATES_MAX << (kLenNumLowBits + 1)];
  CLzmaProb high[kLenNumHighSymbols];
} CLenEnc;


typedef struct
{
  unsigned tableSize;
  UInt32 prices[LZMA_NUM_PB_STATES_MAX][kLenNumSymbolsTotal];
  // UInt32 prices1[LZMA_NUM_PB_STATES_MAX][kLenNumLowSymbols * 2];
  // UInt32 prices2[kLenNumSymbolsTotal];
} CLenPriceEnc;

#define GET_PRICE_LEN(p, posState, len) \
    ((p)->prices[posState][(size_t)(len) - LZMA_MATCH_LEN_MIN])

/*
#define GET_PRICE_LEN(p, posState, len) \
    ((p)->prices2[(size_t)(len) - 2] + ((p)->prices1[posState][((len) - 2) & (kLenNumLowSymbols * 2 - 1)] & (((len) - 2 - kLenNumLowSymbols * 2) >> 9)))
*/

typedef struct
{
  UInt32 range;
  unsigned cache;
  UInt64 low;
  UInt64 cacheSize;
  Byte *buf;
  Byte *bufLim;
  Byte *bufBase;
  ISeqOutStreamPtr outStream;
  UInt64 processed;
  SRes res;
} CRangeEnc;


typedef struct
{
  CLzmaProb *litProbs;

  unsigned state;
  UInt32 reps[LZMA_NUM_REPS];

  CLzmaProb posAlignEncoder[1 << kNumAlignBits];
  CLzmaProb isRep[kNumStates];
  CLzmaProb isRepG0[kNumStates];
  CLzmaProb isRepG1[kNumStates];
  CLzmaProb isRepG2[kNumStates];
  CLzmaProb isMatch[kNumStates][LZMA_NUM_PB_STATES_MAX];
  CLzmaProb isRep0Long[kNumStates][LZMA_NUM_PB_STATES_MAX];

  CLzmaProb posSlotEncoder[kNumLenToPosStates][1 << kNumPosSlotBits];
  CLzmaProb posEncoders[kNumFullDistances];
  
  CLenEnc lenProbs;
  CLenEnc repLenProbs;

} CSaveState;


typedef UInt32 CProbPrice;


struct CLzmaEnc
{
  void *matchFinderObj;
  IMatchFinder2 matchFinder;

  unsigned optCur;
  unsigned optEnd;

  unsigned longestMatchLen;
  unsigned numPairs;
  UInt32 numAvail;

  unsigned state;
  unsigned numFastBytes;
  unsigned additionalOffset;
  UInt32 reps[LZMA_NUM_REPS];
  unsigned lpMask, pbMask;
  CLzmaProb *litProbs;
  CRangeEnc rc;

  UInt32 backRes;

  unsigned lc, lp, pb;
  unsigned lclp;

  BoolInt fastMode;
  BoolInt writeEndMark;
  BoolInt finished;
  BoolInt multiThread;
  BoolInt needInit;
  // BoolInt _maxMode;

  UInt64 nowPos64;
  
  unsigned matchPriceCount;
  // unsigned alignPriceCount;
  int repLenEncCounter;

  unsigned distTableSize;

  UInt32 dictSize;
  SRes result;

  #ifndef Z7_ST
  BoolInt mtMode;
  // begin of CMatchFinderMt is used in LZ thread
  CMatchFinderMt matchFinderMt;
  // end of CMatchFinderMt is used in BT and HASH threads
  // #else
  // CMatchFinder matchFinderBase;
  #endif
  CMatchFinder matchFinderBase;

  
  // we suppose that we have 8-bytes alignment after CMatchFinder
 
  #ifndef Z7_ST
  Byte pad[128];
  #endif
  
  // LZ thread
  CProbPrice ProbPrices[kBitModelTotal >> kNumMoveReducingBits];

  // we want {len , dist} pairs to be 8-bytes aligned in matches array
  UInt32 matches[LZMA_MATCH_LEN_MAX * 2 + 2];

  // we want 8-bytes alignment here
  UInt32 alignPrices[kAlignTableSize];
  UInt32 posSlotPrices[kNumLenToPosStates][kDistTableSizeMax];
  UInt32 distancesPrices[kNumLenToPosStates][kNumFullDistances];

  CLzmaProb posAlignEncoder[1 << kNumAlignBits];
  CLzmaProb isRep[kNumStates];
  CLzmaProb isRepG0[kNumStates];
  CLzmaProb isRepG1[kNumStates];
  CLzmaProb isRepG2[kNumStates];
  CLzmaProb isMatch[kNumStates][LZMA_NUM_PB_STATES_MAX];
  CLzmaProb isRep0Long[kNumStates][LZMA_NUM_PB_STATES_MAX];
  CLzmaProb posSlotEncoder[kNumLenToPosStates][1 << kNumPosSlotBits];
  CLzmaProb posEncoders[kNumFullDistances];
  
  CLenEnc lenProbs;
  CLenEnc repLenProbs;

  #ifndef LZMA_LOG_BSR
  Byte g_FastPos[1 << kNumLogBits];
  #endif

  CLenPriceEnc lenEnc;
  CLenPriceEnc repLenEnc;

  COptimal opt[kNumOpts];

  CSaveState saveState;

  // BoolInt mf_Failure;
  #ifndef Z7_ST
  Byte pad2[128];
  #endif
};


#define MFB (p->matchFinderBase)
/*
#ifndef Z7_ST
#define MFB (p->matchFinderMt.MatchFinder)
#endif
*/

// #define GET_CLzmaEnc_p  CLzmaEnc *p = (CLzmaEnc*)(void *)p;
// #define GET_const_CLzmaEnc_p  const CLzmaEnc *p = (const CLzmaEnc*)(const void *)p;

#define COPY_ARR(dest, src, arr)  memcpy((dest)->arr, (src)->arr, sizeof((src)->arr));

#define COPY_LZMA_ENC_STATE(d, s, p)  \
  (d)->state = (s)->state;  \
  COPY_ARR(d, s, reps)  \
  COPY_ARR(d, s, posAlignEncoder)  \
  COPY_ARR(d, s, isRep)  \
  COPY_ARR(d, s, isRepG0)  \
  COPY_ARR(d, s, isRepG1)  \
  COPY_ARR(d, s, isRepG2)  \
  COPY_ARR(d, s, isMatch)  \
  COPY_ARR(d, s, isRep0Long)  \
  COPY_ARR(d, s, posSlotEncoder)  \
  COPY_ARR(d, s, posEncoders)  \
  (d)->lenProbs = (s)->lenProbs;  \
  (d)->repLenProbs = (s)->repLenProbs;  \
  memcpy((d)->litProbs, (s)->litProbs, ((size_t)0x300 * sizeof(CLzmaProb)) << (p)->lclp);

void LzmaEnc_SaveState(CLzmaEncHandle p)
{
  // GET_CLzmaEnc_p
  CSaveState *v = &p->saveState;
  COPY_LZMA_ENC_STATE(v, p, p)
}

void LzmaEnc_RestoreState(CLzmaEncHandle p)
{
  // GET_CLzmaEnc_p
  const CSaveState *v = &p->saveState;
  COPY_LZMA_ENC_STATE(p, v, p)
}


Z7_NO_INLINE
SRes LzmaEnc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props2)
{
  // GET_CLzmaEnc_p
  CLzmaEncProps props = *props2;
  LzmaEncProps_Normalize(&props);

  if (props.lc > LZMA_LC_MAX
      || props.lp > LZMA_LP_MAX
      || props.pb > LZMA_PB_MAX)
    return SZ_ERROR_PARAM;


  if (props.dictSize > kLzmaMaxHistorySize)
    props.dictSize = kLzmaMaxHistorySize;

  #ifndef LZMA_LOG_BSR
  {
    const UInt64 dict64 = props.dictSize;
    if (dict64 > ((UInt64)1 << kDicLogSizeMaxCompress))
      return SZ_ERROR_PARAM;
  }
  #endif

  p->dictSize = props.dictSize;
  {
    unsigned fb = (unsigned)props.fb;
    if (fb < 5)
      fb = 5;
    if (fb > LZMA_MATCH_LEN_MAX)
      fb = LZMA_MATCH_LEN_MAX;
    p->numFastBytes = fb;
  }
  p->lc = (unsigned)props.lc;
  p->lp = (unsigned)props.lp;
  p->pb = (unsigned)props.pb;
  p->fastMode = (props.algo == 0);
  // p->_maxMode = True;
  MFB.btMode = (Byte)(props.btMode ? 1 : 0);
  // MFB.btMode = (Byte)(props.btMode);
  {
    unsigned numHashBytes = 4;
    if (props.btMode)
    {
           if (props.numHashBytes <  2) numHashBytes = 2;
      else if (props.numHashBytes <  4) numHashBytes = (unsigned)props.numHashBytes;
    }
    if (props.numHashBytes >= 5) numHashBytes = 5;

    MFB.numHashBytes = numHashBytes;
    // MFB.numHashBytes_Min = 2;
    MFB.numHashOutBits = (Byte)props.numHashOutBits;
  }

  MFB.cutValue = props.mc;

  p->writeEndMark = (BoolInt)props.writeEndMark;

  #ifndef Z7_ST
  /*
  if (newMultiThread != _multiThread)
  {
    ReleaseMatchFinder();
    _multiThread = newMultiThread;
  }
  */
  p->multiThread = (props.numThreads > 1);
  p->matchFinderMt.btSync.affinity =
  p->matchFinderMt.hashSync.affinity = props.affinity;
  p->matchFinderMt.btSync.affinityGroup =
  p->matchFinderMt.hashSync.affinityGroup = props.affinityGroup;
  p->matchFinderMt.btSync.affinityInGroup =
  p->matchFinderMt.hashSync.affinityInGroup = props.affinityInGroup;
  #endif

  return SZ_OK;
}


void LzmaEnc_SetDataSize(CLzmaEncHandle p, UInt64 expectedDataSiize)
{
  // GET_CLzmaEnc_p
  MFB.expectedDataSize = expectedDataSiize;
}


#define kState_Start 0
#define kState_LitAfterMatch 4
#define kState_LitAfterRep   5
#define kState_MatchAfterLit 7
#define kState_RepAfterLit   8

static const Byte kLiteralNextStates[kNumStates] = {0, 0, 0, 0, 1, 2, 3, 4,  5,  6,   4, 5};
static const Byte kMatchNextStates[kNumStates]   = {7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10};
static const Byte kRepNextStates[kNumStates]     = {8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11};
static const Byte kShortRepNextStates[kNumStates]= {9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11};

#define IsLitState(s) ((s) < 7)
#define GetLenToPosState2(len) (((len) < kNumLenToPosStates - 1) ? (len) : kNumLenToPosStates - 1)
#define GetLenToPosState(len) (((len) < kNumLenToPosStates + 1) ? (len) - 2 : kNumLenToPosStates - 1)

#define kInfinityPrice (1 << 30)

static void RangeEnc_Construct(CRangeEnc *p)
{
  p->outStream = NULL;
  p->bufBase = NULL;
}

#define RangeEnc_GetProcessed(p)       (        (p)->processed + (size_t)((p)->buf - (p)->bufBase) +         (p)->cacheSize)
#define RangeEnc_GetProcessed_sizet(p) ((size_t)(p)->processed + (size_t)((p)->buf - (p)->bufBase) + (size_t)(p)->cacheSize)

#define RC_BUF_SIZE (1 << 16)

static int RangeEnc_Alloc(CRangeEnc *p, ISzAllocPtr alloc)
{
  if (!p->bufBase)
  {
    p->bufBase = (Byte *)ISzAlloc_Alloc(alloc, RC_BUF_SIZE);
    if (!p->bufBase)
      return 0;
    p->bufLim = p->bufBase + RC_BUF_SIZE;
  }
  return 1;
}

static void RangeEnc_Free(CRangeEnc *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->bufBase);
  p->bufBase = NULL;
}

static void RangeEnc_Init(CRangeEnc *p)
{
  p->range = 0xFFFFFFFF;
  p->cache = 0;
  p->low = 0;
  p->cacheSize = 0;

  p->buf = p->bufBase;

  p->processed = 0;
  p->res = SZ_OK;
}

Z7_NO_INLINE static void RangeEnc_FlushStream(CRangeEnc *p)
{
  const size_t num = (size_t)(p->buf - p->bufBase);
  if (p->res == SZ_OK)
  {
    if (num != ISeqOutStream_Write(p->outStream, p->bufBase, num))
      p->res = SZ_ERROR_WRITE;
  }
  p->processed += num;
  p->buf = p->bufBase;
}

Z7_NO_INLINE static void Z7_FASTCALL RangeEnc_ShiftLow(CRangeEnc *p)
{
  UInt32 low = (UInt32)p->low;
  unsigned high = (unsigned)(p->low >> 32);
  {
    const UInt32 low2 = low << 8;
    p->low = low2;
  }
  if (low < (UInt32)0xFF000000 || high != 0)
  {
    {
      Byte *buf = p->buf;
      *buf++ = (Byte)(p->cache + high);
      p->cache = (unsigned)(low >> 24);
      p->buf = buf;
      if (buf == p->bufLim)
        RangeEnc_FlushStream(p);
      if (p->cacheSize == 0)
        return;
    }
    high += 0xFF;
    for (;;)
    {
      Byte *buf = p->buf;
      *buf++ = (Byte)(high);
      p->buf = buf;
      if (buf == p->bufLim)
        RangeEnc_FlushStream(p);
      if (--p->cacheSize == 0)
        return;
    }
  }
  p->cacheSize++;
}

static void RangeEnc_FlushData(CRangeEnc *p)
{
  int i;
  for (i = 0; i < 5; i++)
    RangeEnc_ShiftLow(p);
}

#define RC_NORM(p) if (range < kTopValue) { range <<= 8; RangeEnc_ShiftLow(p); }

#define RC_BIT_PRE(p, prob) \
  ttt = *(prob); \
  newBound = (range >> kNumBitModelTotalBits) * ttt;

// #define Z7_LZMA_ENC_USE_BRANCH

#ifdef Z7_LZMA_ENC_USE_BRANCH

#define RC_BIT(p, prob, bit) { \
  RC_BIT_PRE(p, prob) \
  if (bit == 0) { range = newBound; ttt += (kBitModelTotal - ttt) >> kNumMoveBits; } \
  else { (p)->low += newBound; range -= newBound; ttt -= ttt >> kNumMoveBits; } \
  *(prob) = (CLzmaProb)ttt; \
  RC_NORM(p) \
  }

#else

#define RC_BIT(p, prob, bit) { \
  UInt32 mask; \
  RC_BIT_PRE(p, prob) \
  mask = 0 - (UInt32)bit; \
  range &= mask; \
  mask &= newBound; \
  range -= mask; \
  (p)->low += mask; \
  mask = (UInt32)bit - 1; \
  range += newBound & mask; \
  mask &= (kBitModelTotal - ((1 << kNumMoveBits) - 1)); \
  mask += ((1 << kNumMoveBits) - 1); \
  ttt += (UInt32)((Int32)(mask - ttt) >> kNumMoveBits); \
  *(prob) = (CLzmaProb)ttt; \
  RC_NORM(p) \
  }

#endif




#define RC_BIT_0_BASE(p, prob) \
  range = newBound; *(prob) = (CLzmaProb)(ttt + ((kBitModelTotal - ttt) >> kNumMoveBits));

#define RC_BIT_1_BASE(p, prob) \
  range -= newBound; (p)->low += newBound; *(prob) = (CLzmaProb)(ttt - (ttt >> kNumMoveBits)); \

#define RC_BIT_0(p, prob) \
  RC_BIT_0_BASE(p, prob) \
  RC_NORM(p)

#define RC_BIT_1(p, prob) \
  RC_BIT_1_BASE(p, prob) \
  RC_NORM(p)

static void RangeEnc_EncodeBit_0(CRangeEnc *p, CLzmaProb *prob)
{
  UInt32 range, ttt, newBound;
  range = p->range;
  RC_BIT_PRE(p, prob)
  RC_BIT_0(p, prob)
  p->range = range;
}

static void LitEnc_Encode(CRangeEnc *p, CLzmaProb *probs, UInt32 sym)
{
  UInt32 range = p->range;
  sym |= 0x100;
  do
  {
    UInt32 ttt, newBound;
    // RangeEnc_EncodeBit(p, probs + (sym >> 8), (sym >> 7) & 1);
    CLzmaProb *prob = probs + (sym >> 8);
    UInt32 bit = (sym >> 7) & 1;
    sym <<= 1;
    RC_BIT(p, prob, bit)
  }
  while (sym < 0x10000);
  p->range = range;
}

static void LitEnc_EncodeMatched(CRangeEnc *p, CLzmaProb *probs, UInt32 sym, UInt32 matchByte)
{
  UInt32 range = p->range;
  UInt32 offs = 0x100;
  sym |= 0x100;
  do
  {
    UInt32 ttt, newBound;
    CLzmaProb *prob;
    UInt32 bit;
    matchByte <<= 1;
    // RangeEnc_EncodeBit(p, probs + (offs + (matchByte & offs) + (sym >> 8)), (sym >> 7) & 1);
    prob = probs + (offs + (matchByte & offs) + (sym >> 8));
    bit = (sym >> 7) & 1;
    sym <<= 1;
    offs &= ~(matchByte ^ sym);
    RC_BIT(p, prob, bit)
  }
  while (sym < 0x10000);
  p->range = range;
}



static void LzmaEnc_InitPriceTables(CProbPrice *ProbPrices)
{
  UInt32 i;
  for (i = 0; i < (kBitModelTotal >> kNumMoveReducingBits); i++)
  {
    const unsigned kCyclesBits = kNumBitPriceShiftBits;
    UInt32 w = (i << kNumMoveReducingBits) + (1 << (kNumMoveReducingBits - 1));
    unsigned bitCount = 0;
    unsigned j;
    for (j = 0; j < kCyclesBits; j++)
    {
      w = w * w;
      bitCount <<= 1;
      while (w >= ((UInt32)1 << 16))
      {
        w >>= 1;
        bitCount++;
      }
    }
    ProbPrices[i] = (CProbPrice)(((unsigned)kNumBitModelTotalBits << kCyclesBits) - 15 - bitCount);
    // printf("\n%3d: %5d", i, ProbPrices[i]);
  }
}


#define GET_PRICE(prob, bit) \
  p->ProbPrices[((prob) ^ (unsigned)(((-(int)(bit))) & (kBitModelTotal - 1))) >> kNumMoveReducingBits]

#define GET_PRICEa(prob, bit) \
     ProbPrices[((prob) ^ (unsigned)((-((int)(bit))) & (kBitModelTotal - 1))) >> kNumMoveReducingBits]

#define GET_PRICE_0(prob) p->ProbPrices[(prob) >> kNumMoveReducingBits]
#define GET_PRICE_1(prob) p->ProbPrices[((prob) ^ (kBitModelTotal - 1)) >> kNumMoveReducingBits]

#define GET_PRICEa_0(prob) ProbPrices[(prob) >> kNumMoveReducingBits]
#define GET_PRICEa_1(prob) ProbPrices[((prob) ^ (kBitModelTotal - 1)) >> kNumMoveReducingBits]


static UInt32 LitEnc_GetPrice(const CLzmaProb *probs, UInt32 sym, const CProbPrice *ProbPrices)
{
  UInt32 price = 0;
  sym |= 0x100;
  do
  {
    unsigned bit = sym & 1;
    sym >>= 1;
    price += GET_PRICEa(probs[sym], bit);
  }
  while (sym >= 2);
  return price;
}


static UInt32 LitEnc_Matched_GetPrice(const CLzmaProb *probs, UInt32 sym, UInt32 matchByte, const CProbPrice *ProbPrices)
{
  UInt32 price = 0;
  UInt32 offs = 0x100;
  sym |= 0x100;
  do
  {
    matchByte <<= 1;
    price += GET_PRICEa(probs[offs + (matchByte & offs) + (sym >> 8)], (sym >> 7) & 1);
    sym <<= 1;
    offs &= ~(matchByte ^ sym);
  }
  while (sym < 0x10000);
  return price;
}


static void RcTree_ReverseEncode(CRangeEnc *rc, CLzmaProb *probs, unsigned numBits, unsigned sym)
{
  UInt32 range = rc->range;
  unsigned m = 1;
  do
  {
    UInt32 ttt, newBound;
    unsigned bit = sym & 1;
    // RangeEnc_EncodeBit(rc, probs + m, bit);
    sym >>= 1;
    RC_BIT(rc, probs + m, bit)
    m = (m << 1) | bit;
  }
  while (--numBits);
  rc->range = range;
}



static void LenEnc_Init(CLenEnc *p)
{
  unsigned i;
  for (i = 0; i < (LZMA_NUM_PB_STATES_MAX << (kLenNumLowBits + 1)); i++)
    p->low[i] = kProbInitValue;
  for (i = 0; i < kLenNumHighSymbols; i++)
    p->high[i] = kProbInitValue;
}

static void LenEnc_Encode(CLenEnc *p, CRangeEnc *rc, unsigned sym, unsigned posState)
{
  UInt32 range, ttt, newBound;
  CLzmaProb *probs = p->low;
  range = rc->range;
  RC_BIT_PRE(rc, probs)
  if (sym >= kLenNumLowSymbols)
  {
    RC_BIT_1(rc, probs)
    probs += kLenNumLowSymbols;
    RC_BIT_PRE(rc, probs)
    if (sym >= kLenNumLowSymbols * 2)
    {
      RC_BIT_1(rc, probs)
      rc->range = range;
      // RcTree_Encode(rc, p->high, kLenNumHighBits, sym - kLenNumLowSymbols * 2);
      LitEnc_Encode(rc, p->high, sym - kLenNumLowSymbols * 2);
      return;
    }
    sym -= kLenNumLowSymbols;
  }

  // RcTree_Encode(rc, probs + (posState << kLenNumLowBits), kLenNumLowBits, sym);
  {
    unsigned m;
    unsigned bit;
    RC_BIT_0(rc, probs)
    posState <<= (1 + kLenNumLowBits);
    probs += posState;
    bit = (sym >> 2)    ; RC_BIT(rc, probs + 1, bit)  m = (1 << 1) + bit;
    bit = (sym >> 1) & 1; RC_BIT(rc, probs + m, bit)  m = (m << 1) + bit;
    bit =  sym       & 1; RC_BIT(rc, probs + m, bit)
    rc->range = range;
  }
}

static void SetPrices_3(const CLzmaProb *probs, UInt32 startPrice, UInt32 *prices, const CProbPrice *ProbPrices)
{
  unsigned i;
  for (i = 0; i < 8; i += 2)
  {
    UInt32 price = startPrice;
    UInt32 prob;
    price += GET_PRICEa(probs[1           ], (i >> 2));
    price += GET_PRICEa(probs[2 + (i >> 2)], (i >> 1) & 1);
    prob = probs[4 + (i >> 1)];
    prices[i    ] = price + GET_PRICEa_0(prob);
    prices[i + 1] = price + GET_PRICEa_1(prob);
  }
}


Z7_NO_INLINE static void Z7_FASTCALL LenPriceEnc_UpdateTables(
    CLenPriceEnc *p,
    unsigned numPosStates,
    const CLenEnc *enc,
    const CProbPrice *ProbPrices)
{
  UInt32 b;
 
  {
    unsigned prob = enc->low[0];
    UInt32 a, c;
    unsigned posState;
    b = GET_PRICEa_1(prob);
    a = GET_PRICEa_0(prob);
    c = b + GET_PRICEa_0(enc->low[kLenNumLowSymbols]);
    for (posState = 0; posState < numPosStates; posState++)
    {
      UInt32 *prices = p->prices[posState];
      const unsigned posState2 = posState << (1 + kLenNumLowBits);
      const CLzmaProb *probs = enc->low + posState2;
      SetPrices_3(probs, a, prices, ProbPrices);
      SetPrices_3(probs + kLenNumLowSymbols, c, prices + kLenNumLowSymbols, ProbPrices);
    }
  }

  /*
  {
    unsigned i;
    UInt32 b;
    a = GET_PRICEa_0(enc->low[0]);
    for (i = 0; i < kLenNumLowSymbols; i++)
      p->prices2[i] = a;
    a = GET_PRICEa_1(enc->low[0]);
    b = a + GET_PRICEa_0(enc->low[kLenNumLowSymbols]);
    for (i = kLenNumLowSymbols; i < kLenNumLowSymbols * 2; i++)
      p->prices2[i] = b;
    a += GET_PRICEa_1(enc->low[kLenNumLowSymbols]);
  }
  */
 
  // p->counter = numSymbols;
  // p->counter = 64;

  {
    unsigned i = p->tableSize;
    
    if (i > kLenNumLowSymbols * 2)
    {
      const CLzmaProb *probs = enc->high;
      UInt32 *prices = p->prices[0] + kLenNumLowSymbols * 2;
      i -= kLenNumLowSymbols * 2 - 1;
      i >>= 1;
      b += GET_PRICEa_1(enc->low[kLenNumLowSymbols]);
      do
      {
        /*
        p->prices2[i] = a +
        // RcTree_GetPrice(enc->high, kLenNumHighBits, i - kLenNumLowSymbols * 2, ProbPrices);
        LitEnc_GetPrice(probs, i - kLenNumLowSymbols * 2, ProbPrices);
        */
        // UInt32 price = a + RcTree_GetPrice(probs, kLenNumHighBits - 1, sym, ProbPrices);
        unsigned sym = --i + (1 << (kLenNumHighBits - 1));
        UInt32 price = b;
        do
        {
          const unsigned bit = sym & 1;
          sym >>= 1;
          price += GET_PRICEa(probs[sym], bit);
        }
        while (sym >= 2);

        {
          const unsigned prob = probs[(size_t)i + (1 << (kLenNumHighBits - 1))];
          prices[(size_t)i * 2    ] = price + GET_PRICEa_0(prob);
          prices[(size_t)i * 2 + 1] = price + GET_PRICEa_1(prob);
        }
      }
      while (i);

      {
        unsigned posState;
        const size_t num = (p->tableSize - kLenNumLowSymbols * 2) * sizeof(p->prices[0][0]);
        for (posState = 1; posState < numPosStates; posState++)
          memcpy(p->prices[posState] + kLenNumLowSymbols * 2, p->prices[0] + kLenNumLowSymbols * 2, num);
      }
    }
  }
}

/*
  #ifdef SHOW_STAT
  g_STAT_OFFSET += num;
  printf("\n MovePos %u", num);
  #endif
*/
  
#define MOVE_POS(p, num) { \
    p->additionalOffset += (num); \
    p->matchFinder.Skip(p->matchFinderObj, (UInt32)(num)); }


static unsigned ReadMatchDistances(CLzmaEnc *p, unsigned *numPairsRes)
{
  unsigned numPairs;
  
  p->additionalOffset++;
  p->numAvail = p->matchFinder.GetNumAvailableBytes(p->matchFinderObj);
  {
    const UInt32 *d = p->matchFinder.GetMatches(p->matchFinderObj, p->matches);
    // if (!d) { p->mf_Failure = True; *numPairsRes = 0;  return 0; }
    numPairs = (unsigned)(d - p->matches);
  }
  *numPairsRes = numPairs;
  
  #ifdef SHOW_STAT
  printf("\n i = %u numPairs = %u    ", g_STAT_OFFSET, numPairs / 2);
  g_STAT_OFFSET++;
  {
    unsigned i;
    for (i = 0; i < numPairs; i += 2)
      printf("%2u %6u   | ", p->matches[i], p->matches[i + 1]);
  }
  #endif
  
  if (numPairs == 0)
    return 0;
  {
    const unsigned len = p->matches[(size_t)numPairs - 2];
    if (len != p->numFastBytes)
      return len;
    {
      UInt32 numAvail = p->numAvail;
      if (numAvail > LZMA_MATCH_LEN_MAX)
        numAvail = LZMA_MATCH_LEN_MAX;
      {
        const Byte *p1 = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
        const Byte *p2 = p1 + len;
        const ptrdiff_t dif = (ptrdiff_t)-1 - (ptrdiff_t)p->matches[(size_t)numPairs - 1];
        const Byte *lim = p1 + numAvail;
        for (; p2 != lim && *p2 == p2[dif]; p2++)
        {}
        return (unsigned)(p2 - p1);
      }
    }
  }
}

#define MARK_LIT ((UInt32)(Int32)-1)

#define MakeAs_Lit(p)       { (p)->dist = MARK_LIT; (p)->extra = 0; }
#define MakeAs_ShortRep(p)  { (p)->dist = 0; (p)->extra = 0; }
#define IsShortRep(p)       ((p)->dist == 0)


#define GetPrice_ShortRep(p, state, posState) \
  ( GET_PRICE_0(p->isRepG0[state]) + GET_PRICE_0(p->isRep0Long[state][posState]))

#define GetPrice_Rep_0(p, state, posState) ( \
    GET_PRICE_1(p->isMatch[state][posState]) \
  + GET_PRICE_1(p->isRep0Long[state][posState])) \
  + GET_PRICE_1(p->isRep[state]) \
  + GET_PRICE_0(p->isRepG0[state])
  
Z7_FORCE_INLINE
static UInt32 GetPrice_PureRep(const CLzmaEnc *p, unsigned repIndex, size_t state, size_t posState)
{
  UInt32 price;
  UInt32 prob = p->isRepG0[state];
  if (repIndex == 0)
  {
    price = GET_PRICE_0(prob);
    price += GET_PRICE_1(p->isRep0Long[state][posState]);
  }
  else
  {
    price = GET_PRICE_1(prob);
    prob = p->isRepG1[state];
    if (repIndex == 1)
      price += GET_PRICE_0(prob);
    else
    {
      price += GET_PRICE_1(prob);
      price += GET_PRICE(p->isRepG2[state], repIndex - 2);
    }
  }
  return price;
}


static unsigned Backward(CLzmaEnc *p, unsigned cur)
{
  unsigned wr = cur + 1;
  p->optEnd = wr;

  for (;;)
  {
    UInt32 dist = p->opt[cur].dist;
    unsigned len = (unsigned)p->opt[cur].len;
    unsigned extra = (unsigned)p->opt[cur].extra;
    cur -= len;

    if (extra)
    {
      wr--;
      p->opt[wr].len = (UInt32)len;
      cur -= extra;
      len = extra;
      if (extra == 1)
      {
        p->opt[wr].dist = dist;
        dist = MARK_LIT;
      }
      else
      {
        p->opt[wr].dist = 0;
        len--;
        wr--;
        p->opt[wr].dist = MARK_LIT;
        p->opt[wr].len = 1;
      }
    }

    if (cur == 0)
    {
      p->backRes = dist;
      p->optCur = wr;
      return len;
    }
    
    wr--;
    p->opt[wr].dist = dist;
    p->opt[wr].len = (UInt32)len;
  }
}



#define LIT_PROBS(pos, prevByte) \
  (p->litProbs + (UInt32)3 * (((((pos) << 8) + (prevByte)) & p->lpMask) << p->lc))


static unsigned GetOptimum(CLzmaEnc *p, UInt32 position)
{
  unsigned last, cur;
  UInt32 reps[LZMA_NUM_REPS];
  unsigned repLens[LZMA_NUM_REPS];
  UInt32 *matches;

  {
    UInt32 numAvail;
    unsigned numPairs, mainLen, repMaxIndex, i, posState;
    UInt32 matchPrice, repMatchPrice;
    const Byte *data;
    Byte curByte, matchByte;
    
    p->optCur = p->optEnd = 0;
    
    if (p->additionalOffset == 0)
      mainLen = ReadMatchDistances(p, &numPairs);
    else
    {
      mainLen = p->longestMatchLen;
      numPairs = p->numPairs;
    }
    
    numAvail = p->numAvail;
    if (numAvail < 2)
    {
      p->backRes = MARK_LIT;
      return 1;
    }
    if (numAvail > LZMA_MATCH_LEN_MAX)
      numAvail = LZMA_MATCH_LEN_MAX;
    
    data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
    repMaxIndex = 0;
    
    for (i = 0; i < LZMA_NUM_REPS; i++)
    {
      unsigned len;
      const Byte *data2;
      reps[i] = p->reps[i];
      data2 = data - reps[i];
      if (data[0] != data2[0] || data[1] != data2[1])
      {
        repLens[i] = 0;
        continue;
      }
      for (len = 2; len < numAvail && data[len] == data2[len]; len++)
      {}
      repLens[i] = len;
      if (len > repLens[repMaxIndex])
        repMaxIndex = i;
      if (len == LZMA_MATCH_LEN_MAX) // 21.03 : optimization
        break;
    }
    
    if (repLens[repMaxIndex] >= p->numFastBytes)
    {
      unsigned len;
      p->backRes = (UInt32)repMaxIndex;
      len = repLens[repMaxIndex];
      MOVE_POS(p, len - 1)
      return len;
    }
    
    matches = p->matches;
    #define MATCHES  matches
    // #define MATCHES  p->matches
    
    if (mainLen >= p->numFastBytes)
    {
      p->backRes = MATCHES[(size_t)numPairs - 1] + LZMA_NUM_REPS;
      MOVE_POS(p, mainLen - 1)
      return mainLen;
    }
    
    curByte = *data;
    matchByte = *(data - reps[0]);

    last = repLens[repMaxIndex];
    if (last <= mainLen)
      last = mainLen;
    
    if (last < 2 && curByte != matchByte)
    {
      p->backRes = MARK_LIT;
      return 1;
    }
    
    p->opt[0].state = (CState)p->state;
    
    posState = (position & p->pbMask);
    
    {
      const CLzmaProb *probs = LIT_PROBS(position, *(data - 1));
      p->opt[1].price = GET_PRICE_0(p->isMatch[p->state][posState]) +
        (!IsLitState(p->state) ?
          LitEnc_Matched_GetPrice(probs, curByte, matchByte, p->ProbPrices) :
          LitEnc_GetPrice(probs, curByte, p->ProbPrices));
    }

    MakeAs_Lit(&p->opt[1])
    
    matchPrice = GET_PRICE_1(p->isMatch[p->state][posState]);
    repMatchPrice = matchPrice + GET_PRICE_1(p->isRep[p->state]);
    
    // 18.06
    if (matchByte == curByte && repLens[0] == 0)
    {
      UInt32 shortRepPrice = repMatchPrice + GetPrice_ShortRep(p, p->state, posState);
      if (shortRepPrice < p->opt[1].price)
      {
        p->opt[1].price = shortRepPrice;
        MakeAs_ShortRep(&p->opt[1])
      }
      if (last < 2)
      {
        p->backRes = p->opt[1].dist;
        return 1;
      }
    }
   
    p->opt[1].len = 1;
    
    p->opt[0].reps[0] = reps[0];
    p->opt[0].reps[1] = reps[1];
    p->opt[0].reps[2] = reps[2];
    p->opt[0].reps[3] = reps[3];
    
    // ---------- REP ----------
    
    for (i = 0; i < LZMA_NUM_REPS; i++)
    {
      unsigned repLen = repLens[i];
      UInt32 price;
      if (repLen < 2)
        continue;
      price = repMatchPrice + GetPrice_PureRep(p, i, p->state, posState);
      do
      {
        UInt32 price2 = price + GET_PRICE_LEN(&p->repLenEnc, posState, repLen);
        COptimal *opt = &p->opt[repLen];
        if (price2 < opt->price)
        {
          opt->price = price2;
          opt->len = (UInt32)repLen;
          opt->dist = (UInt32)i;
          opt->extra = 0;
        }
      }
      while (--repLen >= 2);
    }
    
    
    // ---------- MATCH ----------
    {
      unsigned len = repLens[0] + 1;
      if (len <= mainLen)
      {
        unsigned offs = 0;
        UInt32 normalMatchPrice = matchPrice + GET_PRICE_0(p->isRep[p->state]);

        if (len < 2)
          len = 2;
        else
          while (len > MATCHES[offs])
            offs += 2;
    
        for (; ; len++)
        {
          COptimal *opt;
          UInt32 dist = MATCHES[(size_t)offs + 1];
          UInt32 price = normalMatchPrice + GET_PRICE_LEN(&p->lenEnc, posState, len);
          unsigned lenToPosState = GetLenToPosState(len);
       
          if (dist < kNumFullDistances)
            price += p->distancesPrices[lenToPosState][dist & (kNumFullDistances - 1)];
          else
          {
            unsigned slot;
            GetPosSlot2(dist, slot)
            price += p->alignPrices[dist & kAlignMask];
            price += p->posSlotPrices[lenToPosState][slot];
          }
          
          opt = &p->opt[len];
          
          if (price < opt->price)
          {
            opt->price = price;
            opt->len = (UInt32)len;
            opt->dist = dist + LZMA_NUM_REPS;
            opt->extra = 0;
          }
          
          if (len == MATCHES[offs])
          {
            offs += 2;
            if (offs == numPairs)
              break;
          }
        }
      }
    }
    

    cur = 0;

    #ifdef SHOW_STAT2
    /* if (position >= 0) */
    {
      unsigned i;
      printf("\n pos = %4X", position);
      for (i = cur; i <= last; i++)
      printf("\nprice[%4X] = %u", position - cur + i, p->opt[i].price);
    }
    #endif
  }


  
  // ---------- Optimal Parsing ----------

  for (;;)
  {
    unsigned numAvail;
    UInt32 numAvailFull;
    unsigned newLen, numPairs, prev, state, posState, startLen;
    UInt32 litPrice, matchPrice, repMatchPrice;
    BoolInt nextIsLit;
    Byte curByte, matchByte;
    const Byte *data;
    COptimal *curOpt, *nextOpt;

    if (++cur == last)
      break;
    
    // 18.06
    if (cur >= kNumOpts - 64)
    {
      unsigned j, best;
      UInt32 price = p->opt[cur].price;
      best = cur;
      for (j = cur + 1; j <= last; j++)
      {
        UInt32 price2 = p->opt[j].price;
        if (price >= price2)
        {
          price = price2;
          best = j;
        }
      }
      {
        unsigned delta = best - cur;
        if (delta != 0)
        {
          MOVE_POS(p, delta)
        }
      }
      cur = best;
      break;
    }

    newLen = ReadMatchDistances(p, &numPairs);
    
    if (newLen >= p->numFastBytes)
    {
      p->numPairs = numPairs;
      p->longestMatchLen = newLen;
      break;
    }
    
    curOpt = &p->opt[cur];

    position++;

    // we need that check here, if skip_items in p->opt are possible
    /*
    if (curOpt->price >= kInfinityPrice)
      continue;
    */

    prev = cur - curOpt->len;

    if (curOpt->len == 1)
    {
      state = (unsigned)p->opt[prev].state;
      if (IsShortRep(curOpt))
        state = kShortRepNextStates[state];
      else
        state = kLiteralNextStates[state];
    }
    else
    {
      const COptimal *prevOpt;
      UInt32 b0;
      UInt32 dist = curOpt->dist;

      if (curOpt->extra)
      {
        prev -= (unsigned)curOpt->extra;
        state = kState_RepAfterLit;
        if (curOpt->extra == 1)
          state = (dist < LZMA_NUM_REPS ? kState_RepAfterLit : kState_MatchAfterLit);
      }
      else
      {
        state = (unsigned)p->opt[prev].state;
        if (dist < LZMA_NUM_REPS)
          state = kRepNextStates[state];
        else
          state = kMatchNextStates[state];
      }

      prevOpt = &p->opt[prev];
      b0 = prevOpt->reps[0];

      if (dist < LZMA_NUM_REPS)
      {
        if (dist == 0)
        {
          reps[0] = b0;
          reps[1] = prevOpt->reps[1];
          reps[2] = prevOpt->reps[2];
          reps[3] = prevOpt->reps[3];
        }
        else
        {
          reps[1] = b0;
          b0 = prevOpt->reps[1];
          if (dist == 1)
          {
            reps[0] = b0;
            reps[2] = prevOpt->reps[2];
            reps[3] = prevOpt->reps[3];
          }
          else
          {
            reps[2] = b0;
            reps[0] = prevOpt->reps[dist];
            reps[3] = prevOpt->reps[dist ^ 1];
          }
        }
      }
      else
      {
        reps[0] = (dist - LZMA_NUM_REPS + 1);
        reps[1] = b0;
        reps[2] = prevOpt->reps[1];
        reps[3] = prevOpt->reps[2];
      }
    }
    
    curOpt->state = (CState)state;
    curOpt->reps[0] = reps[0];
    curOpt->reps[1] = reps[1];
    curOpt->reps[2] = reps[2];
    curOpt->reps[3] = reps[3];

    data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
    curByte = *data;
    matchByte = *(data - reps[0]);

    posState = (position & p->pbMask);

    /*
    The order of Price checks:
       <  LIT
       <= SHORT_REP
       <  LIT : REP_0
       <  REP    [ : LIT : REP_0 ]
       <  MATCH  [ : LIT : REP_0 ]
    */

    {
      UInt32 curPrice = curOpt->price;
      unsigned prob = p->isMatch[state][posState];
      matchPrice = curPrice + GET_PRICE_1(prob);
      litPrice = curPrice + GET_PRICE_0(prob);
    }

    nextOpt = &p->opt[(size_t)cur + 1];
    nextIsLit = False;

    // here we can allow skip_items in p->opt, if we don't check (nextOpt->price < kInfinityPrice)
    // 18.new.06
    if ((nextOpt->price < kInfinityPrice
        // && !IsLitState(state)
        && matchByte == curByte)
        || litPrice > nextOpt->price
        )
      litPrice = 0;
    else
    {
      const CLzmaProb *probs = LIT_PROBS(position, *(data - 1));
      litPrice += (!IsLitState(state) ?
          LitEnc_Matched_GetPrice(probs, curByte, matchByte, p->ProbPrices) :
          LitEnc_GetPrice(probs, curByte, p->ProbPrices));
      
      if (litPrice < nextOpt->price)
      {
        nextOpt->price = litPrice;
        nextOpt->len = 1;
        MakeAs_Lit(nextOpt)
        nextIsLit = True;
      }
    }

    repMatchPrice = matchPrice + GET_PRICE_1(p->isRep[state]);
    
    numAvailFull = p->numAvail;
    {
      unsigned temp = kNumOpts - 1 - cur;
      if (numAvailFull > temp)
        numAvailFull = (UInt32)temp;
    }

    // 18.06
    // ---------- SHORT_REP ----------
    if (IsLitState(state)) // 18.new
    if (matchByte == curByte)
    if (repMatchPrice < nextOpt->price) // 18.new
    // if (numAvailFull < 2 || data[1] != *(data - reps[0] + 1))
    if (
        // nextOpt->price >= kInfinityPrice ||
        nextOpt->len < 2   // we can check nextOpt->len, if skip items are not allowed in p->opt
        || (nextOpt->dist != 0
            // && nextOpt->extra <= 1 // 17.old
            )
        )
    {
      UInt32 shortRepPrice = repMatchPrice + GetPrice_ShortRep(p, state, posState);
      // if (shortRepPrice <= nextOpt->price) // 17.old
      if (shortRepPrice < nextOpt->price)  // 18.new
      {
        nextOpt->price = shortRepPrice;
        nextOpt->len = 1;
        MakeAs_ShortRep(nextOpt)
        nextIsLit = False;
      }
    }
    
    if (numAvailFull < 2)
      continue;
    numAvail = (numAvailFull <= p->numFastBytes ? numAvailFull : p->numFastBytes);

    // numAvail <= p->numFastBytes

    // ---------- LIT : REP_0 ----------

    if (!nextIsLit
        && litPrice != 0 // 18.new
        && matchByte != curByte
        && numAvailFull > 2)
    {
      const Byte *data2 = data - reps[0];
      if (data[1] == data2[1] && data[2] == data2[2])
      {
        unsigned len;
        unsigned limit = p->numFastBytes + 1;
        if (limit > numAvailFull)
          limit = numAvailFull;
        for (len = 3; len < limit && data[len] == data2[len]; len++)
        {}
        
        {
          unsigned state2 = kLiteralNextStates[state];
          unsigned posState2 = (position + 1) & p->pbMask;
          UInt32 price = litPrice + GetPrice_Rep_0(p, state2, posState2);
          {
            unsigned offset = cur + len;

            if (last < offset)
              last = offset;
          
            // do
            {
              UInt32 price2;
              COptimal *opt;
              len--;
              // price2 = price + GetPrice_Len_Rep_0(p, len, state2, posState2);
              price2 = price + GET_PRICE_LEN(&p->repLenEnc, posState2, len);

              opt = &p->opt[offset];
              // offset--;
              if (price2 < opt->price)
              {
                opt->price = price2;
                opt->len = (UInt32)len;
                opt->dist = 0;
                opt->extra = 1;
              }
            }
            // while (len >= 3);
          }
        }
      }
    }
    
    startLen = 2; /* speed optimization */

    {
      // ---------- REP ----------
      unsigned repIndex = 0; // 17.old
      // unsigned repIndex = IsLitState(state) ? 0 : 1; // 18.notused
      for (; repIndex < LZMA_NUM_REPS; repIndex++)
      {
        unsigned len;
        UInt32 price;
        const Byte *data2 = data - reps[repIndex];
        if (data[0] != data2[0] || data[1] != data2[1])
          continue;
        
        for (len = 2; len < numAvail && data[len] == data2[len]; len++)
        {}
        
        // if (len < startLen) continue; // 18.new: speed optimization

        {
          unsigned offset = cur + len;
          if (last < offset)
            last = offset;
        }
        {
          unsigned len2 = len;
          price = repMatchPrice + GetPrice_PureRep(p, repIndex, state, posState);
          do
          {
            UInt32 price2 = price + GET_PRICE_LEN(&p->repLenEnc, posState, len2);
            COptimal *opt = &p->opt[cur + len2];
            if (price2 < opt->price)
            {
              opt->price = price2;
              opt->len = (UInt32)len2;
              opt->dist = (UInt32)repIndex;
              opt->extra = 0;
            }
          }
          while (--len2 >= 2);
        }
        
        if (repIndex == 0) startLen = len + 1;  // 17.old
        // startLen = len + 1; // 18.new

        /* if (_maxMode) */
        {
          // ---------- REP : LIT : REP_0 ----------
          // numFastBytes + 1 + numFastBytes

          unsigned len2 = len + 1;
          unsigned limit = len2 + p->numFastBytes;
          if (limit > numAvailFull)
            limit = numAvailFull;
          
          len2 += 2;
          if (len2 <= limit)
          if (data[len2 - 2] == data2[len2 - 2])
          if (data[len2 - 1] == data2[len2 - 1])
          {
            unsigned state2 = kRepNextStates[state];
            unsigned posState2 = (position + len) & p->pbMask;
            price += GET_PRICE_LEN(&p->repLenEnc, posState, len)
                + GET_PRICE_0(p->isMatch[state2][posState2])
                + LitEnc_Matched_GetPrice(LIT_PROBS(position + len, data[(size_t)len - 1]),
                    data[len], data2[len], p->ProbPrices);
            
            // state2 = kLiteralNextStates[state2];
            state2 = kState_LitAfterRep;
            posState2 = (posState2 + 1) & p->pbMask;


            price += GetPrice_Rep_0(p, state2, posState2);

          for (; len2 < limit && data[len2] == data2[len2]; len2++)
          {}
          
          len2 -= len;
          // if (len2 >= 3)
          {
            {
              unsigned offset = cur + len + len2;

              if (last < offset)
                last = offset;
              // do
              {
                UInt32 price2;
                COptimal *opt;
                len2--;
                // price2 = price + GetPrice_Len_Rep_0(p, len2, state2, posState2);
                price2 = price + GET_PRICE_LEN(&p->repLenEnc, posState2, len2);

                opt = &p->opt[offset];
                // offset--;
                if (price2 < opt->price)
                {
                  opt->price = price2;
                  opt->len = (UInt32)len2;
                  opt->extra = (CExtra)(len + 1);
                  opt->dist = (UInt32)repIndex;
                }
              }
              // while (len2 >= 3);
            }
          }
          }
        }
      }
    }


    // ---------- MATCH ----------
    /* for (unsigned len = 2; len <= newLen; len++) */
    if (newLen > numAvail)
    {
      newLen = numAvail;
      for (numPairs = 0; newLen > MATCHES[numPairs]; numPairs += 2);
      MATCHES[numPairs] = (UInt32)newLen;
      numPairs += 2;
    }
    
    // startLen = 2; /* speed optimization */

    if (newLen >= startLen)
    {
      UInt32 normalMatchPrice = matchPrice + GET_PRICE_0(p->isRep[state]);
      UInt32 dist;
      unsigned offs, posSlot, len;
      
      {
        unsigned offset = cur + newLen;
        if (last < offset)
          last = offset;
      }

      offs = 0;
      while (startLen > MATCHES[offs])
        offs += 2;
      dist = MATCHES[(size_t)offs + 1];
      
      // if (dist >= kNumFullDistances)
      GetPosSlot2(dist, posSlot)
      
      for (len = /*2*/ startLen; ; len++)
      {
        UInt32 price = normalMatchPrice + GET_PRICE_LEN(&p->lenEnc, posState, len);
        {
          COptimal *opt;
          unsigned lenNorm = len - 2;
          lenNorm = GetLenToPosState2(lenNorm);
          if (dist < kNumFullDistances)
            price += p->distancesPrices[lenNorm][dist & (kNumFullDistances - 1)];
          else
            price += p->posSlotPrices[lenNorm][posSlot] + p->alignPrices[dist & kAlignMask];
          
          opt = &p->opt[cur + len];
          if (price < opt->price)
          {
            opt->price = price;
            opt->len = (UInt32)len;
            opt->dist = dist + LZMA_NUM_REPS;
            opt->extra = 0;
          }
        }

        if (len == MATCHES[offs])
        {
          // if (p->_maxMode) {
          // MATCH : LIT : REP_0

          const Byte *data2 = data - dist - 1;
          unsigned len2 = len + 1;
          unsigned limit = len2 + p->numFastBytes;
          if (limit > numAvailFull)
            limit = numAvailFull;
          
          len2 += 2;
          if (len2 <= limit)
          if (data[len2 - 2] == data2[len2 - 2])
          if (data[len2 - 1] == data2[len2 - 1])
          {
          for (; len2 < limit && data[len2] == data2[len2]; len2++)
          {}
          
          len2 -= len;
          
          // if (len2 >= 3)
          {
            unsigned state2 = kMatchNextStates[state];
            unsigned posState2 = (position + len) & p->pbMask;
            unsigned offset;
            price += GET_PRICE_0(p->isMatch[state2][posState2]);
            price += LitEnc_Matched_GetPrice(LIT_PROBS(position + len, data[(size_t)len - 1]),
                    data[len], data2[len], p->ProbPrices);

            // state2 = kLiteralNextStates[state2];
            state2 = kState_LitAfterMatch;

            posState2 = (posState2 + 1) & p->pbMask;
            price += GetPrice_Rep_0(p, state2, posState2);

            offset = cur + len + len2;

            if (last < offset)
              last = offset;
            // do
            {
              UInt32 price2;
              COptimal *opt;
              len2--;
              // price2 = price + GetPrice_Len_Rep_0(p, len2, state2, posState2);
              price2 = price + GET_PRICE_LEN(&p->repLenEnc, posState2, len2);
              opt = &p->opt[offset];
              // offset--;
              if (price2 < opt->price)
              {
                opt->price = price2;
                opt->len = (UInt32)len2;
                opt->extra = (CExtra)(len + 1);
                opt->dist = dist + LZMA_NUM_REPS;
              }
            }
            // while (len2 >= 3);
          }

          }
        
          offs += 2;
          if (offs == numPairs)
            break;
          dist = MATCHES[(size_t)offs + 1];
          // if (dist >= kNumFullDistances)
            GetPosSlot2(dist, posSlot)
        }
      }
    }
  }

  do
    p->opt[last].price = kInfinityPrice;
  while (--last);

  return Backward(p, cur);
}



#define ChangePair(smallDist, bigDist) (((bigDist) >> 7) > (smallDist))



static unsigned GetOptimumFast(CLzmaEnc *p)
{
  UInt32 numAvail, mainDist;
  unsigned mainLen, numPairs, repIndex, repLen, i;
  const Byte *data;

  if (p->additionalOffset == 0)
    mainLen = ReadMatchDistances(p, &numPairs);
  else
  {
    mainLen = p->longestMatchLen;
    numPairs = p->numPairs;
  }

  numAvail = p->numAvail;
  p->backRes = MARK_LIT;
  if (numAvail < 2)
    return 1;
  // if (mainLen < 2 && p->state == 0) return 1; // 18.06.notused
  if (numAvail > LZMA_MATCH_LEN_MAX)
    numAvail = LZMA_MATCH_LEN_MAX;
  data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
  repLen = repIndex = 0;
  
  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    unsigned len;
    const Byte *data2 = data - p->reps[i];
    if (data[0] != data2[0] || data[1] != data2[1])
      continue;
    for (len = 2; len < numAvail && data[len] == data2[len]; len++)
    {}
    if (len >= p->numFastBytes)
    {
      p->backRes = (UInt32)i;
      MOVE_POS(p, len - 1)
      return len;
    }
    if (len > repLen)
    {
      repIndex = i;
      repLen = len;
    }
  }

  if (mainLen >= p->numFastBytes)
  {
    p->backRes = p->matches[(size_t)numPairs - 1] + LZMA_NUM_REPS;
    MOVE_POS(p, mainLen - 1)
    return mainLen;
  }

  mainDist = 0; /* for GCC */
  
  if (mainLen >= 2)
  {
    mainDist = p->matches[(size_t)numPairs - 1];
    while (numPairs > 2)
    {
      UInt32 dist2;
      if (mainLen != p->matches[(size_t)numPairs - 4] + 1)
        break;
      dist2 = p->matches[(size_t)numPairs - 3];
      if (!ChangePair(dist2, mainDist))
        break;
      numPairs -= 2;
      mainLen--;
      mainDist = dist2;
    }
    if (mainLen == 2 && mainDist >= 0x80)
      mainLen = 1;
  }

  if (repLen >= 2)
    if (    repLen + 1 >= mainLen
        || (repLen + 2 >= mainLen && mainDist >= (1 << 9))
        || (repLen + 3 >= mainLen && mainDist >= (1 << 15)))
  {
    p->backRes = (UInt32)repIndex;
    MOVE_POS(p, repLen - 1)
    return repLen;
  }
  
  if (mainLen < 2 || numAvail <= 2)
    return 1;

  {
    unsigned len1 = ReadMatchDistances(p, &p->numPairs);
    p->longestMatchLen = len1;
  
    if (len1 >= 2)
    {
      UInt32 newDist = p->matches[(size_t)p->numPairs - 1];
      if (   (len1 >= mainLen && newDist < mainDist)
          || (len1 == mainLen + 1 && !ChangePair(mainDist, newDist))
          || (len1 >  mainLen + 1)
          || (len1 + 1 >= mainLen && mainLen >= 3 && ChangePair(newDist, mainDist)))
        return 1;
    }
  }
  
  data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
  
  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    unsigned len, limit;
    const Byte *data2 = data - p->reps[i];
    if (data[0] != data2[0] || data[1] != data2[1])
      continue;
    limit = mainLen - 1;
    for (len = 2;; len++)
    {
      if (len >= limit)
        return 1;
      if (data[len] != data2[len])
        break;
    }
  }
  
  p->backRes = mainDist + LZMA_NUM_REPS;
  if (mainLen != 2)
  {
    MOVE_POS(p, mainLen - 2)
  }
  return mainLen;
}




static void WriteEndMarker(CLzmaEnc *p, unsigned posState)
{
  UInt32 range;
  range = p->rc.range;
  {
    UInt32 ttt, newBound;
    CLzmaProb *prob = &p->isMatch[p->state][posState];
    RC_BIT_PRE(&p->rc, prob)
    RC_BIT_1(&p->rc, prob)
    prob = &p->isRep[p->state];
    RC_BIT_PRE(&p->rc, prob)
    RC_BIT_0(&p->rc, prob)
  }
  p->state = kMatchNextStates[p->state];
  
  p->rc.range = range;
  LenEnc_Encode(&p->lenProbs, &p->rc, 0, posState);
  range = p->rc.range;

  {
    // RcTree_Encode_PosSlot(&p->rc, p->posSlotEncoder[0], (1 << kNumPosSlotBits) - 1);
    CLzmaProb *probs = p->posSlotEncoder[0];
    unsigned m = 1;
    do
    {
      UInt32 ttt, newBound;
      RC_BIT_PRE(p, probs + m)
      RC_BIT_1(&p->rc, probs + m)
      m = (m << 1) + 1;
    }
    while (m < (1 << kNumPosSlotBits));
  }
  {
    // RangeEnc_EncodeDirectBits(&p->rc, ((UInt32)1 << (30 - kNumAlignBits)) - 1, 30 - kNumAlignBits);    UInt32 range = p->range;
    unsigned numBits = 30 - kNumAlignBits;
    do
    {
      range >>= 1;
      p->rc.low += range;
      RC_NORM(&p->rc)
    }
    while (--numBits);
  }
   
  {
    // RcTree_ReverseEncode(&p->rc, p->posAlignEncoder, kNumAlignBits, kAlignMask);
    CLzmaProb *probs = p->posAlignEncoder;
    unsigned m = 1;
    do
    {
      UInt32 ttt, newBound;
      RC_BIT_PRE(p, probs + m)
      RC_BIT_1(&p->rc, probs + m)
      m = (m << 1) + 1;
    }
    while (m < kAlignTableSize);
  }
  p->rc.range = range;
}


static SRes CheckErrors(CLzmaEnc *p)
{
  if (p->result != SZ_OK)
    return p->result;
  if (p->rc.res != SZ_OK)
    p->result = SZ_ERROR_WRITE;

  #ifndef Z7_ST
  if (
      // p->mf_Failure ||
        (p->mtMode &&
          ( // p->matchFinderMt.failure_LZ_LZ ||
            p->matchFinderMt.failure_LZ_BT))
     )
  {
    p->result = MY_HRES_ERROR_INTERNAL_ERROR;
    // printf("\nCheckErrors p->matchFinderMt.failureLZ\n");
  }
  #endif

  if (MFB.result != SZ_OK)
    p->result = SZ_ERROR_READ;
  
  if (p->result != SZ_OK)
    p->finished = True;
  return p->result;
}


Z7_NO_INLINE static SRes Flush(CLzmaEnc *p, UInt32 nowPos)
{
  /* ReleaseMFStream(); */
  p->finished = True;
  if (p->writeEndMark)
    WriteEndMarker(p, nowPos & p->pbMask);
  RangeEnc_FlushData(&p->rc);
  RangeEnc_FlushStream(&p->rc);
  return CheckErrors(p);
}


Z7_NO_INLINE static void FillAlignPrices(CLzmaEnc *p)
{
  unsigned i;
  const CProbPrice *ProbPrices = p->ProbPrices;
  const CLzmaProb *probs = p->posAlignEncoder;
  // p->alignPriceCount = 0;
  for (i = 0; i < kAlignTableSize / 2; i++)
  {
    UInt32 price = 0;
    unsigned sym = i;
    unsigned m = 1;
    unsigned bit;
    UInt32 prob;
    bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[m], bit); m = (m << 1) + bit;
    bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[m], bit); m = (m << 1) + bit;
    bit = sym & 1;            price += GET_PRICEa(probs[m], bit); m = (m << 1) + bit;
    prob = probs[m];
    p->alignPrices[i    ] = price + GET_PRICEa_0(prob);
    p->alignPrices[i + 8] = price + GET_PRICEa_1(prob);
    // p->alignPrices[i] = RcTree_ReverseGetPrice(p->posAlignEncoder, kNumAlignBits, i, p->ProbPrices);
  }
}


Z7_NO_INLINE static void FillDistancesPrices(CLzmaEnc *p)
{
  // int y; for (y = 0; y < 100; y++) {

  UInt32 tempPrices[kNumFullDistances];
  unsigned i, lps;

  const CProbPrice *ProbPrices = p->ProbPrices;
  p->matchPriceCount = 0;

  for (i = kStartPosModelIndex / 2; i < kNumFullDistances / 2; i++)
  {
    unsigned posSlot = GetPosSlot1(i);
    unsigned footerBits = (posSlot >> 1) - 1;
    unsigned base = ((2 | (posSlot & 1)) << footerBits);
    const CLzmaProb *probs = p->posEncoders + (size_t)base * 2;
    // tempPrices[i] = RcTree_ReverseGetPrice(p->posEncoders + base, footerBits, i - base, p->ProbPrices);
    UInt32 price = 0;
    unsigned m = 1;
    unsigned sym = i;
    unsigned offset = (unsigned)1 << footerBits;
    base += i;
    
    if (footerBits)
    do
    {
      unsigned bit = sym & 1;
      sym >>= 1;
      price += GET_PRICEa(probs[m], bit);
      m = (m << 1) + bit;
    }
    while (--footerBits);

    {
      unsigned prob = probs[m];
      tempPrices[base         ] = price + GET_PRICEa_0(prob);
      tempPrices[base + offset] = price + GET_PRICEa_1(prob);
    }
  }

  for (lps = 0; lps < kNumLenToPosStates; lps++)
  {
    unsigned slot;
    unsigned distTableSize2 = (p->distTableSize + 1) >> 1;
    UInt32 *posSlotPrices = p->posSlotPrices[lps];
    const CLzmaProb *probs = p->posSlotEncoder[lps];
    
    for (slot = 0; slot < distTableSize2; slot++)
    {
      // posSlotPrices[slot] = RcTree_GetPrice(encoder, kNumPosSlotBits, slot, p->ProbPrices);
      UInt32 price;
      unsigned bit;
      unsigned sym = slot + (1 << (kNumPosSlotBits - 1));
      unsigned prob;
      bit = sym & 1; sym >>= 1; price  = GET_PRICEa(probs[sym], bit);
      bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[sym], bit);
      bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[sym], bit);
      bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[sym], bit);
      bit = sym & 1; sym >>= 1; price += GET_PRICEa(probs[sym], bit);
      prob = probs[(size_t)slot + (1 << (kNumPosSlotBits - 1))];
      posSlotPrices[(size_t)slot * 2    ] = price + GET_PRICEa_0(prob);
      posSlotPrices[(size_t)slot * 2 + 1] = price + GET_PRICEa_1(prob);
    }
    
    {
      UInt32 delta = ((UInt32)((kEndPosModelIndex / 2 - 1) - kNumAlignBits) << kNumBitPriceShiftBits);
      for (slot = kEndPosModelIndex / 2; slot < distTableSize2; slot++)
      {
        posSlotPrices[(size_t)slot * 2    ] += delta;
        posSlotPrices[(size_t)slot * 2 + 1] += delta;
        delta += ((UInt32)1 << kNumBitPriceShiftBits);
      }
    }

    {
      UInt32 *dp = p->distancesPrices[lps];
      
      dp[0] = posSlotPrices[0];
      dp[1] = posSlotPrices[1];
      dp[2] = posSlotPrices[2];
      dp[3] = posSlotPrices[3];

      for (i = 4; i < kNumFullDistances; i += 2)
      {
        UInt32 slotPrice = posSlotPrices[GetPosSlot1(i)];
        dp[i    ] = slotPrice + tempPrices[i];
        dp[i + 1] = slotPrice + tempPrices[i + 1];
      }
    }
  }
  // }
}



static void LzmaEnc_Construct(CLzmaEnc *p)
{
  RangeEnc_Construct(&p->rc);
  MatchFinder_Construct(&MFB);
  
  #ifndef Z7_ST
  p->matchFinderMt.MatchFinder = &MFB;
  MatchFinderMt_Construct(&p->matchFinderMt);
  #endif

  {
    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    LzmaEnc_SetProps((CLzmaEncHandle)(void *)p, &props);
  }

  #ifndef LZMA_LOG_BSR
  LzmaEnc_FastPosInit(p->g_FastPos);
  #endif

  LzmaEnc_InitPriceTables(p->ProbPrices);
  p->litProbs = NULL;
  p->saveState.litProbs = NULL;
}

CLzmaEncHandle LzmaEnc_Create(ISzAllocPtr alloc)
{
  CLzmaEncHandle p = (CLzmaEncHandle)ISzAlloc_Alloc(alloc, sizeof(CLzmaEnc));
  if (p)
    LzmaEnc_Construct(p);
  return p;
}

static void LzmaEnc_FreeLits(CLzmaEnc *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->litProbs);
  ISzAlloc_Free(alloc, p->saveState.litProbs);
  p->litProbs = NULL;
  p->saveState.litProbs = NULL;
}

static void LzmaEnc_Destruct(CLzmaEnc *p, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  #ifndef Z7_ST
  MatchFinderMt_Destruct(&p->matchFinderMt, allocBig);
  #endif
  
  MatchFinder_Free(&MFB, allocBig);
  LzmaEnc_FreeLits(p, alloc);
  RangeEnc_Free(&p->rc, alloc);
}

void LzmaEnc_Destroy(CLzmaEncHandle p, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  // GET_CLzmaEnc_p
  LzmaEnc_Destruct(p, alloc, allocBig);
  ISzAlloc_Free(alloc, p);
}


Z7_NO_INLINE
static SRes LzmaEnc_CodeOneBlock(CLzmaEnc *p, UInt32 maxPackSize, UInt32 maxUnpackSize)
{
  UInt32 nowPos32, startPos32;
  if (p->needInit)
  {
    #ifndef Z7_ST
    if (p->mtMode)
    {
      RINOK(MatchFinderMt_InitMt(&p->matchFinderMt))
    }
    #endif
    p->matchFinder.Init(p->matchFinderObj);
    p->needInit = 0;
  }

  if (p->finished)
    return p->result;
  RINOK(CheckErrors(p))

  nowPos32 = (UInt32)p->nowPos64;
  startPos32 = nowPos32;

  if (p->nowPos64 == 0)
  {
    unsigned numPairs;
    Byte curByte;
    if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) == 0)
      return Flush(p, nowPos32);
    ReadMatchDistances(p, &numPairs);
    RangeEnc_EncodeBit_0(&p->rc, &p->isMatch[kState_Start][0]);
    // p->state = kLiteralNextStates[p->state];
    curByte = *(p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset);
    LitEnc_Encode(&p->rc, p->litProbs, curByte);
    p->additionalOffset--;
    nowPos32++;
  }

  if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) != 0)
  
  for (;;)
  {
    UInt32 dist;
    unsigned len, posState;
    UInt32 range, ttt, newBound;
    CLzmaProb *probs;
  
    if (p->fastMode)
      len = GetOptimumFast(p);
    else
    {
      unsigned oci = p->optCur;
      if (p->optEnd == oci)
        len = GetOptimum(p, nowPos32);
      else
      {
        const COptimal *opt = &p->opt[oci];
        len = opt->len;
        p->backRes = opt->dist;
        p->optCur = oci + 1;
      }
    }

    posState = (unsigned)nowPos32 & p->pbMask;
    range = p->rc.range;
    probs = &p->isMatch[p->state][posState];
    
    RC_BIT_PRE(&p->rc, probs)
    
    dist = p->backRes;

    #ifdef SHOW_STAT2
    printf("\n pos = %6X, len = %3u  pos = %6u", nowPos32, len, dist);
    #endif

    if (dist == MARK_LIT)
    {
      Byte curByte;
      const Byte *data;
      unsigned state;

      RC_BIT_0(&p->rc, probs)
      p->rc.range = range;
      data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset;
      probs = LIT_PROBS(nowPos32, *(data - 1));
      curByte = *data;
      state = p->state;
      p->state = kLiteralNextStates[state];
      if (IsLitState(state))
        LitEnc_Encode(&p->rc, probs, curByte);
      else
        LitEnc_EncodeMatched(&p->rc, probs, curByte, *(data - p->reps[0]));
    }
    else
    {
      RC_BIT_1(&p->rc, probs)
      probs = &p->isRep[p->state];
      RC_BIT_PRE(&p->rc, probs)
      
      if (dist < LZMA_NUM_REPS)
      {
        RC_BIT_1(&p->rc, probs)
        probs = &p->isRepG0[p->state];
        RC_BIT_PRE(&p->rc, probs)
        if (dist == 0)
        {
          RC_BIT_0(&p->rc, probs)
          probs = &p->isRep0Long[p->state][posState];
          RC_BIT_PRE(&p->rc, probs)
          if (len != 1)
          {
            RC_BIT_1_BASE(&p->rc, probs)
          }
          else
          {
            RC_BIT_0_BASE(&p->rc, probs)
            p->state = kShortRepNextStates[p->state];
          }
        }
        else
        {
          RC_BIT_1(&p->rc, probs)
          probs = &p->isRepG1[p->state];
          RC_BIT_PRE(&p->rc, probs)
          if (dist == 1)
          {
            RC_BIT_0_BASE(&p->rc, probs)
            dist = p->reps[1];
          }
          else
          {
            RC_BIT_1(&p->rc, probs)
            probs = &p->isRepG2[p->state];
            RC_BIT_PRE(&p->rc, probs)
            if (dist == 2)
            {
              RC_BIT_0_BASE(&p->rc, probs)
              dist = p->reps[2];
            }
            else
            {
              RC_BIT_1_BASE(&p->rc, probs)
              dist = p->reps[3];
              p->reps[3] = p->reps[2];
            }
            p->reps[2] = p->reps[1];
          }
          p->reps[1] = p->reps[0];
          p->reps[0] = dist;
        }

        RC_NORM(&p->rc)

        p->rc.range = range;

        if (len != 1)
        {
          LenEnc_Encode(&p->repLenProbs, &p->rc, len - LZMA_MATCH_LEN_MIN, posState);
          --p->repLenEncCounter;
          p->state = kRepNextStates[p->state];
        }
      }
      else
      {
        unsigned posSlot;
        RC_BIT_0(&p->rc, probs)
        p->rc.range = range;
        p->state = kMatchNextStates[p->state];

        LenEnc_Encode(&p->lenProbs, &p->rc, len - LZMA_MATCH_LEN_MIN, posState);
        // --p->lenEnc.counter;

        dist -= LZMA_NUM_REPS;
        p->reps[3] = p->reps[2];
        p->reps[2] = p->reps[1];
        p->reps[1] = p->reps[0];
        p->reps[0] = dist + 1;
        
        p->matchPriceCount++;
        GetPosSlot(dist, posSlot)
        // RcTree_Encode_PosSlot(&p->rc, p->posSlotEncoder[GetLenToPosState(len)], posSlot);
        {
          UInt32 sym = (UInt32)posSlot + (1 << kNumPosSlotBits);
          range = p->rc.range;
          probs = p->posSlotEncoder[GetLenToPosState(len)];
          do
          {
            CLzmaProb *prob = probs + (sym >> kNumPosSlotBits);
            UInt32 bit = (sym >> (kNumPosSlotBits - 1)) & 1;
            sym <<= 1;
            RC_BIT(&p->rc, prob, bit)
          }
          while (sym < (1 << kNumPosSlotBits * 2));
          p->rc.range = range;
        }
        
        if (dist >= kStartPosModelIndex)
        {
          unsigned footerBits = ((posSlot >> 1) - 1);

          if (dist < kNumFullDistances)
          {
            unsigned base = ((2 | (posSlot & 1)) << footerBits);
            RcTree_ReverseEncode(&p->rc, p->posEncoders + base, footerBits, (unsigned)(dist /* - base */));
          }
          else
          {
            UInt32 pos2 = (dist | 0xF) << (32 - footerBits);
            range = p->rc.range;
            // RangeEnc_EncodeDirectBits(&p->rc, posReduced >> kNumAlignBits, footerBits - kNumAlignBits);
            /*
            do
            {
              range >>= 1;
              p->rc.low += range & (0 - ((dist >> --footerBits) & 1));
              RC_NORM(&p->rc)
            }
            while (footerBits > kNumAlignBits);
            */
            do
            {
              range >>= 1;
              p->rc.low += range & (0 - (pos2 >> 31));
              pos2 += pos2;
              RC_NORM(&p->rc)
            }
            while (pos2 != 0xF0000000);


            // RcTree_ReverseEncode(&p->rc, p->posAlignEncoder, kNumAlignBits, posReduced & kAlignMask);

            {
              unsigned m = 1;
              unsigned bit;
              bit = dist & 1; dist >>= 1; RC_BIT(&p->rc, p->posAlignEncoder + m, bit)  m = (m << 1) + bit;
              bit = dist & 1; dist >>= 1; RC_BIT(&p->rc, p->posAlignEncoder + m, bit)  m = (m << 1) + bit;
              bit = dist & 1; dist >>= 1; RC_BIT(&p->rc, p->posAlignEncoder + m, bit)  m = (m << 1) + bit;
              bit = dist & 1;             RC_BIT(&p->rc, p->posAlignEncoder + m, bit)
              p->rc.range = range;
              // p->alignPriceCount++;
            }
          }
        }
      }
    }

    nowPos32 += (UInt32)len;
    p->additionalOffset -= len;
    
    if (p->additionalOffset == 0)
    {
      UInt32 processed;

      if (!p->fastMode)
      {
        /*
        if (p->alignPriceCount >= 16) // kAlignTableSize
          FillAlignPrices(p);
        if (p->matchPriceCount >= 128)
          FillDistancesPrices(p);
        if (p->lenEnc.counter <= 0)
          LenPriceEnc_UpdateTables(&p->lenEnc, 1 << p->pb, &p->lenProbs, p->ProbPrices);
        */
        if (p->matchPriceCount >= 64)
        {
          FillAlignPrices(p);
          // { int y; for (y = 0; y < 100; y++) {
          FillDistancesPrices(p);
          // }}
          LenPriceEnc_UpdateTables(&p->lenEnc, (unsigned)1 << p->pb, &p->lenProbs, p->ProbPrices);
        }
        if (p->repLenEncCounter <= 0)
        {
          p->repLenEncCounter = REP_LEN_COUNT;
          LenPriceEnc_UpdateTables(&p->repLenEnc, (unsigned)1 << p->pb, &p->repLenProbs, p->ProbPrices);
        }
      }
    
      if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) == 0)
        break;
      processed = nowPos32 - startPos32;
      
      if (maxPackSize)
      {
        if (processed + kNumOpts + 300 >= maxUnpackSize
            || RangeEnc_GetProcessed_sizet(&p->rc) + kPackReserve >= maxPackSize)
          break;
      }
      else if (processed >= (1 << 17))
      {
        p->nowPos64 += nowPos32 - startPos32;
        return CheckErrors(p);
      }
    }
  }

  p->nowPos64 += nowPos32 - startPos32;
  return Flush(p, nowPos32);
}



#define kBigHashDicLimit ((UInt32)1 << 24)

static SRes LzmaEnc_Alloc(CLzmaEnc *p, UInt32 keepWindowSize, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  UInt32 beforeSize = kNumOpts;
  UInt32 dictSize;

  if (!RangeEnc_Alloc(&p->rc, alloc))
    return SZ_ERROR_MEM;

  #ifndef Z7_ST
  p->mtMode = (p->multiThread && !p->fastMode && (MFB.btMode != 0));
  #endif

  {
    const unsigned lclp = p->lc + p->lp;
    if (!p->litProbs || !p->saveState.litProbs || p->lclp != lclp)
    {
      LzmaEnc_FreeLits(p, alloc);
      p->litProbs =           (CLzmaProb *)ISzAlloc_Alloc(alloc, ((size_t)0x300 * sizeof(CLzmaProb)) << lclp);
      p->saveState.litProbs = (CLzmaProb *)ISzAlloc_Alloc(alloc, ((size_t)0x300 * sizeof(CLzmaProb)) << lclp);
      if (!p->litProbs || !p->saveState.litProbs)
      {
        LzmaEnc_FreeLits(p, alloc);
        return SZ_ERROR_MEM;
      }
      p->lclp = lclp;
    }
  }

  MFB.bigHash = (Byte)(p->dictSize > kBigHashDicLimit ? 1 : 0);


  dictSize = p->dictSize;
  if (dictSize == ((UInt32)2 << 30) ||
      dictSize == ((UInt32)3 << 30))
  {
    /* 21.03 : here we reduce the dictionary for 2 reasons:
       1) we don't want 32-bit back_distance matches in decoder for 2 GB dictionary.
       2) we want to elimate useless last MatchFinder_Normalize3() for corner cases,
          where data size is aligned for 1 GB: 5/6/8 GB.
          That reducing must be >= 1 for such corner cases. */
    dictSize -= 1;
  }

  if (beforeSize + dictSize < keepWindowSize)
    beforeSize = keepWindowSize - dictSize;

  /* in worst case we can look ahead for
        max(LZMA_MATCH_LEN_MAX, numFastBytes + 1 + numFastBytes) bytes.
     we send larger value for (keepAfter) to MantchFinder_Create():
        (numFastBytes + LZMA_MATCH_LEN_MAX + 1)
  */

  #ifndef Z7_ST
  if (p->mtMode)
  {
    RINOK(MatchFinderMt_Create(&p->matchFinderMt, dictSize, beforeSize,
        p->numFastBytes, LZMA_MATCH_LEN_MAX + 1 /* 18.04 */
        , allocBig))
    p->matchFinderObj = &p->matchFinderMt;
    MFB.bigHash = (Byte)(MFB.hashMask >= 0xFFFFFF ? 1 : 0);
    MatchFinderMt_CreateVTable(&p->matchFinderMt, &p->matchFinder);
  }
  else
  #endif
  {
    if (!MatchFinder_Create(&MFB, dictSize, beforeSize,
        p->numFastBytes, LZMA_MATCH_LEN_MAX + 1 /* 21.03 */
        , allocBig))
      return SZ_ERROR_MEM;
    p->matchFinderObj = &MFB;
    MatchFinder_CreateVTable(&MFB, &p->matchFinder);
  }
  
  return SZ_OK;
}

static void LzmaEnc_Init(CLzmaEnc *p)
{
  unsigned i;
  p->state = 0;
  p->reps[0] =
  p->reps[1] =
  p->reps[2] =
  p->reps[3] = 1;

  RangeEnc_Init(&p->rc);

  for (i = 0; i < (1 << kNumAlignBits); i++)
    p->posAlignEncoder[i] = kProbInitValue;

  for (i = 0; i < kNumStates; i++)
  {
    unsigned j;
    for (j = 0; j < LZMA_NUM_PB_STATES_MAX; j++)
    {
      p->isMatch[i][j] = kProbInitValue;
      p->isRep0Long[i][j] = kProbInitValue;
    }
    p->isRep[i] = kProbInitValue;
    p->isRepG0[i] = kProbInitValue;
    p->isRepG1[i] = kProbInitValue;
    p->isRepG2[i] = kProbInitValue;
  }

  {
    for (i = 0; i < kNumLenToPosStates; i++)
    {
      CLzmaProb *probs = p->posSlotEncoder[i];
      unsigned j;
      for (j = 0; j < (1 << kNumPosSlotBits); j++)
        probs[j] = kProbInitValue;
    }
  }
  {
    for (i = 0; i < kNumFullDistances; i++)
      p->posEncoders[i] = kProbInitValue;
  }

  {
    const size_t num = (size_t)0x300 << (p->lp + p->lc);
    size_t k;
    CLzmaProb *probs = p->litProbs;
    for (k = 0; k < num; k++)
      probs[k] = kProbInitValue;
  }


  LenEnc_Init(&p->lenProbs);
  LenEnc_Init(&p->repLenProbs);

  p->optEnd = 0;
  p->optCur = 0;

  {
    for (i = 0; i < kNumOpts; i++)
      p->opt[i].price = kInfinityPrice;
  }

  p->additionalOffset = 0;

  p->pbMask = ((unsigned)1 << p->pb) - 1;
  p->lpMask = ((UInt32)0x100 << p->lp) - ((unsigned)0x100 >> p->lc);

  // p->mf_Failure = False;
}


static void LzmaEnc_InitPrices(CLzmaEnc *p)
{
  if (!p->fastMode)
  {
    FillDistancesPrices(p);
    FillAlignPrices(p);
  }

  p->lenEnc.tableSize =
  p->repLenEnc.tableSize =
      p->numFastBytes + 1 - LZMA_MATCH_LEN_MIN;

  p->repLenEncCounter = REP_LEN_COUNT;

  LenPriceEnc_UpdateTables(&p->lenEnc, (unsigned)1 << p->pb, &p->lenProbs, p->ProbPrices);
  LenPriceEnc_UpdateTables(&p->repLenEnc, (unsigned)1 << p->pb, &p->repLenProbs, p->ProbPrices);
}

static SRes LzmaEnc_AllocAndInit(CLzmaEnc *p, UInt32 keepWindowSize, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  unsigned i;
  for (i = kEndPosModelIndex / 2; i < kDicLogSizeMax; i++)
    if (p->dictSize <= ((UInt32)1 << i))
      break;
  p->distTableSize = i * 2;

  p->finished = False;
  p->result = SZ_OK;
  p->nowPos64 = 0;
  p->needInit = 1;
  RINOK(LzmaEnc_Alloc(p, keepWindowSize, alloc, allocBig))
  LzmaEnc_Init(p);
  LzmaEnc_InitPrices(p);
  return SZ_OK;
}

static SRes LzmaEnc_Prepare(CLzmaEncHandle p,
    ISeqOutStreamPtr outStream,
    ISeqInStreamPtr inStream,
    ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  // GET_CLzmaEnc_p
  MatchFinder_SET_STREAM(&MFB, inStream)
  p->rc.outStream = outStream;
  return LzmaEnc_AllocAndInit(p, 0, alloc, allocBig);
}

SRes LzmaEnc_PrepareForLzma2(CLzmaEncHandle p,
    ISeqInStreamPtr inStream, UInt32 keepWindowSize,
    ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  // GET_CLzmaEnc_p
  MatchFinder_SET_STREAM(&MFB, inStream)
  return LzmaEnc_AllocAndInit(p, keepWindowSize, alloc, allocBig);
}

SRes LzmaEnc_MemPrepare(CLzmaEncHandle p,
    const Byte *src, SizeT srcLen,
    UInt32 keepWindowSize,
    ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  // GET_CLzmaEnc_p
  MatchFinder_SET_DIRECT_INPUT_BUF(&MFB, src, srcLen)
  LzmaEnc_SetDataSize(p, srcLen);
  return LzmaEnc_AllocAndInit(p, keepWindowSize, alloc, allocBig);
}

void LzmaEnc_Finish(CLzmaEncHandle p)
{
  #ifndef Z7_ST
  // GET_CLzmaEnc_p
  if (p->mtMode)
    MatchFinderMt_ReleaseStream(&p->matchFinderMt);
  #else
  UNUSED_VAR(p)
  #endif
}


typedef struct
{
  ISeqOutStream vt;
  Byte *data;
  size_t rem;
  BoolInt overflow;
} CLzmaEnc_SeqOutStreamBuf;

static size_t SeqOutStreamBuf_Write(ISeqOutStreamPtr pp, const void *data, size_t size)
{
  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CLzmaEnc_SeqOutStreamBuf)
  if (p->rem < size)
  {
    size = p->rem;
    p->overflow = True;
  }
  if (size != 0)
  {
    memcpy(p->data, data, size);
    p->rem -= size;
    p->data += size;
  }
  return size;
}


/*
UInt32 LzmaEnc_GetNumAvailableBytes(CLzmaEncHandle p)
{
  GET_const_CLzmaEnc_p
  return p->matchFinder.GetNumAvailableBytes(p->matchFinderObj);
}
*/

const Byte *LzmaEnc_GetCurBuf(CLzmaEncHandle p)
{
  // GET_const_CLzmaEnc_p
  return p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset;
}


// (desiredPackSize == 0) is not allowed
SRes LzmaEnc_CodeOneMemBlock(CLzmaEncHandle p, BoolInt reInit,
    Byte *dest, size_t *destLen, UInt32 desiredPackSize, UInt32 *unpackSize)
{
  // GET_CLzmaEnc_p
  UInt64 nowPos64;
  SRes res;
  CLzmaEnc_SeqOutStreamBuf outStream;

  outStream.vt.Write = SeqOutStreamBuf_Write;
  outStream.data = dest;
  outStream.rem = *destLen;
  outStream.overflow = False;

  p->writeEndMark = False;
  p->finished = False;
  p->result = SZ_OK;

  if (reInit)
    LzmaEnc_Init(p);
  LzmaEnc_InitPrices(p);
  RangeEnc_Init(&p->rc);
  p->rc.outStream = &outStream.vt;
  nowPos64 = p->nowPos64;
  
  res = LzmaEnc_CodeOneBlock(p, desiredPackSize, *unpackSize);
  
  *unpackSize = (UInt32)(p->nowPos64 - nowPos64);
  *destLen -= outStream.rem;
  if (outStream.overflow)
    return SZ_ERROR_OUTPUT_EOF;

  return res;
}


Z7_NO_INLINE
static SRes LzmaEnc_Encode2(CLzmaEnc *p, ICompressProgressPtr progress)
{
  SRes res = SZ_OK;

  #ifndef Z7_ST
  Byte allocaDummy[0x300];
  allocaDummy[0] = 0;
  allocaDummy[1] = allocaDummy[0];
  #endif

  for (;;)
  {
    res = LzmaEnc_CodeOneBlock(p, 0, 0);
    if (res != SZ_OK || p->finished)
      break;
    if (progress)
    {
      res = ICompressProgress_Progress(progress, p->nowPos64, RangeEnc_GetProcessed(&p->rc));
      if (res != SZ_OK)
      {
        res = SZ_ERROR_PROGRESS;
        break;
      }
    }
  }
  
  LzmaEnc_Finish((CLzmaEncHandle)(void *)p);

  /*
  if (res == SZ_OK && !Inline_MatchFinder_IsFinishedOK(&MFB))
    res = SZ_ERROR_FAIL;
  }
  */

  return res;
}


SRes LzmaEnc_Encode(CLzmaEncHandle p, ISeqOutStreamPtr outStream, ISeqInStreamPtr inStream, ICompressProgressPtr progress,
    ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  // GET_CLzmaEnc_p
  RINOK(LzmaEnc_Prepare(p, outStream, inStream, alloc, allocBig))
  return LzmaEnc_Encode2(p, progress);
}


SRes LzmaEnc_WriteProperties(CLzmaEncHandle p, Byte *props, SizeT *size)
{
  if (*size < LZMA_PROPS_SIZE)
    return SZ_ERROR_PARAM;
  *size = LZMA_PROPS_SIZE;
  {
    // GET_CLzmaEnc_p
    const UInt32 dictSize = p->dictSize;
    UInt32 v;
    props[0] = (Byte)((p->pb * 5 + p->lp) * 9 + p->lc);
    
    // we write aligned dictionary value to properties for lzma decoder
    if (dictSize >= ((UInt32)1 << 21))
    {
      const UInt32 kDictMask = ((UInt32)1 << 20) - 1;
      v = (dictSize + kDictMask) & ~kDictMask;
      if (v < dictSize)
        v = dictSize;
    }
    else
    {
      unsigned i = 11 * 2;
      do
      {
        v = (UInt32)(2 + (i & 1)) << (i >> 1);
        i++;
      }
      while (v < dictSize);
    }

    SetUi32(props + 1, v)
    return SZ_OK;
  }
}


unsigned LzmaEnc_IsWriteEndMark(CLzmaEncHandle p)
{
  // GET_CLzmaEnc_p
  return (unsigned)p->writeEndMark;
}


SRes LzmaEnc_MemEncode(CLzmaEncHandle p, Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
    int writeEndMark, ICompressProgressPtr progress, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  SRes res;
  // GET_CLzmaEnc_p

  CLzmaEnc_SeqOutStreamBuf outStream;

  outStream.vt.Write = SeqOutStreamBuf_Write;
  outStream.data = dest;
  outStream.rem = *destLen;
  outStream.overflow = False;

  p->writeEndMark = writeEndMark;
  p->rc.outStream = &outStream.vt;

  res = LzmaEnc_MemPrepare(p, src, srcLen, 0, alloc, allocBig);
  
  if (res == SZ_OK)
  {
    res = LzmaEnc_Encode2(p, progress);
    if (res == SZ_OK && p->nowPos64 != srcLen)
      res = SZ_ERROR_FAIL;
  }

  *destLen -= (SizeT)outStream.rem;
  if (outStream.overflow)
    return SZ_ERROR_OUTPUT_EOF;
  return res;
}


SRes LzmaEncode(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
    const CLzmaEncProps *props, Byte *propsEncoded, SizeT *propsSize, int writeEndMark,
    ICompressProgressPtr progress, ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  CLzmaEncHandle p = LzmaEnc_Create(alloc);
  SRes res;
  if (!p)
    return SZ_ERROR_MEM;

  res = LzmaEnc_SetProps(p, props);
  if (res == SZ_OK)
  {
    res = LzmaEnc_WriteProperties(p, propsEncoded, propsSize);
    if (res == SZ_OK)
      res = LzmaEnc_MemEncode(p, dest, destLen, src, srcLen,
          writeEndMark, progress, alloc, allocBig);
  }

  LzmaEnc_Destroy(p, alloc, allocBig);
  return res;
}


/*
#ifndef Z7_ST
void LzmaEnc_GetLzThreads(CLzmaEncHandle p, HANDLE lz_threads[2])
{
  GET_const_CLzmaEnc_p
  lz_threads[0] = p->matchFinderMt.hashSync.thread;
  lz_threads[1] = p->matchFinderMt.btSync.thread;
}
#endif
*/

/* ---- embedded source: Lzma2Enc.c ---- */
/* Lzma2Enc.c -- LZMA2 Encoder
: Igor Pavlov : Public domain */


#include <string.h>

/* #define Z7_ST */


#ifndef Z7_ST
#else
#define MTCODER_THREADS_MAX 1
#endif

#define LZMA2_CONTROL_LZMA (1 << 7)
#define LZMA2_CONTROL_COPY_NO_RESET 2
#define LZMA2_CONTROL_COPY_RESET_DIC 1
#define LZMA2_CONTROL_EOF 0

#define LZMA2_LCLP_MAX 4

#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

#define LZMA2_PACK_SIZE_MAX (1 << 16)
#define LZMA2_COPY_CHUNK_SIZE LZMA2_PACK_SIZE_MAX
#define LZMA2_UNPACK_SIZE_MAX (1 << 21)
#define LZMA2_KEEP_WINDOW_SIZE LZMA2_UNPACK_SIZE_MAX

#define LZMA2_CHUNK_SIZE_COMPRESSED_MAX ((1 << 16) + 16)


#define PRF(x) /* x */


/* ---------- CLimitedSeqInStream ---------- */

typedef struct
{
  ISeqInStream vt;
  ISeqInStreamPtr realStream;
  UInt64 limit;
  UInt64 processed;
  int finished;
} CLimitedSeqInStream;

static void LimitedSeqInStream_Init(CLimitedSeqInStream *p)
{
  p->limit = (UInt64)(Int64)-1;
  p->processed = 0;
  p->finished = 0;
}

static SRes LimitedSeqInStream_Read(ISeqInStreamPtr pp, void *data, size_t *size)
{
  Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CLimitedSeqInStream)
  size_t size2 = *size;
  SRes res = SZ_OK;
  
  if (p->limit != (UInt64)(Int64)-1)
  {
    const UInt64 rem = p->limit - p->processed;
    if (size2 > rem)
      size2 = (size_t)rem;
  }
  if (size2 != 0)
  {
    res = ISeqInStream_Read(p->realStream, data, &size2);
    p->finished = (size2 == 0 ? 1 : 0);
    p->processed += size2;
  }
  *size = size2;
  return res;
}


/* ---------- CLzma2EncInt ---------- */

typedef struct
{
  CLzmaEncHandle enc;
  Byte propsAreSet;
  Byte propsByte;
  Byte needInitState;
  Byte needInitProp;
  UInt64 srcPos;
} CLzma2EncInt;


static SRes Lzma2EncInt_InitStream(CLzma2EncInt *p, const CLzma2EncProps *props)
{
  if (!p->propsAreSet)
  {
    SizeT propsSize = LZMA_PROPS_SIZE;
    Byte propsEncoded[LZMA_PROPS_SIZE];
    RINOK(LzmaEnc_SetProps(p->enc, &props->lzmaProps))
    RINOK(LzmaEnc_WriteProperties(p->enc, propsEncoded, &propsSize))
    p->propsByte = propsEncoded[0];
    p->propsAreSet = True;
  }
  return SZ_OK;
}

static void Lzma2EncInt_InitBlock(CLzma2EncInt *p)
{
  p->srcPos = 0;
  p->needInitState = True;
  p->needInitProp = True;
}


SRes LzmaEnc_PrepareForLzma2(CLzmaEncHandle p, ISeqInStreamPtr inStream, UInt32 keepWindowSize,
    ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_MemPrepare(CLzmaEncHandle p, const Byte *src, SizeT srcLen,
    UInt32 keepWindowSize, ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_CodeOneMemBlock(CLzmaEncHandle p, BoolInt reInit,
    Byte *dest, size_t *destLen, UInt32 desiredPackSize, UInt32 *unpackSize);
const Byte *LzmaEnc_GetCurBuf(CLzmaEncHandle p);
void LzmaEnc_Finish(CLzmaEncHandle p);
void LzmaEnc_SaveState(CLzmaEncHandle p);
void LzmaEnc_RestoreState(CLzmaEncHandle p);

/*
UInt32 LzmaEnc_GetNumAvailableBytes(CLzmaEncHandle p);
*/

static SRes Lzma2EncInt_EncodeSubblock(CLzma2EncInt *p, Byte *outBuf,
    size_t *packSizeRes, ISeqOutStreamPtr outStream)
{
  const size_t packSizeLimit = *packSizeRes;
  size_t packSize = packSizeLimit;
  UInt32 unpackSize = LZMA2_UNPACK_SIZE_MAX;
  const unsigned lzHeaderSize = 5 + (p->needInitProp ? 1 : 0);
  SRes res;

  *packSizeRes = 0;
  if (packSize < lzHeaderSize)
    return SZ_ERROR_OUTPUT_EOF;
  packSize -= lzHeaderSize;
  
  LzmaEnc_SaveState(p->enc);
  res = LzmaEnc_CodeOneMemBlock(p->enc, p->needInitState,
      outBuf + lzHeaderSize, &packSize, LZMA2_PACK_SIZE_MAX, &unpackSize);
  
  PRF(printf("\npackSize = %7d unpackSize = %7d  ", packSize, unpackSize));

  if (unpackSize == 0)
    return res;

  if (res == SZ_ERROR_OUTPUT_EOF
      || (res == SZ_OK && (packSize + 2 >= unpackSize || packSize > (1 << 16))))
  {
    size_t destPos = 0;
    PRF(printf("################# COPY           "));

    while (unpackSize)
    {
      const UInt32 u = (unpackSize < LZMA2_COPY_CHUNK_SIZE) ? unpackSize : LZMA2_COPY_CHUNK_SIZE;
      if (packSizeLimit - destPos < u + 3)
        return SZ_ERROR_OUTPUT_EOF;
      outBuf[destPos++] = (Byte)(p->srcPos == 0 ? LZMA2_CONTROL_COPY_RESET_DIC : LZMA2_CONTROL_COPY_NO_RESET);
      outBuf[destPos++] = (Byte)((u - 1) >> 8);
      outBuf[destPos++] = (Byte)(u - 1);
      memcpy(outBuf + destPos, LzmaEnc_GetCurBuf(p->enc) - unpackSize, u);
      unpackSize -= u;
      destPos += u;
      p->srcPos += u;
      
      if (outStream)
      {
        *packSizeRes += destPos;
        if (ISeqOutStream_Write(outStream, outBuf, destPos) != destPos)
          return SZ_ERROR_WRITE;
        destPos = 0;
      }
      else
        *packSizeRes = destPos;
      /* needInitState = True; */
    }
    
    LzmaEnc_RestoreState(p->enc);
    return SZ_OK;
  }

  if (res != SZ_OK)
    return res;
  {
    size_t destPos = 0;
    const UInt32 u = unpackSize - 1;
    const UInt32 pm = (UInt32)(packSize - 1);
    const unsigned mode = (p->srcPos == 0) ? 3 : (p->needInitState ? (p->needInitProp ? 2 : 1) : 0);

    PRF(printf("               "));

    outBuf[destPos++] = (Byte)(LZMA2_CONTROL_LZMA | (mode << 5) | ((u >> 16) & 0x1F));
    outBuf[destPos++] = (Byte)(u >> 8);
    outBuf[destPos++] = (Byte)u;
    outBuf[destPos++] = (Byte)(pm >> 8);
    outBuf[destPos++] = (Byte)pm;
    
    if (p->needInitProp)
      outBuf[destPos++] = p->propsByte;
    
    p->needInitProp = False;
    p->needInitState = False;
    destPos += packSize;
    p->srcPos += unpackSize;

    if (outStream)
      if (ISeqOutStream_Write(outStream, outBuf, destPos) != destPos)
        return SZ_ERROR_WRITE;
    
    *packSizeRes = destPos;
    return SZ_OK;
  }
}


/* ---------- Lzma2 Props ---------- */

void Lzma2EncProps_Init(CLzma2EncProps *p)
{
  LzmaEncProps_Init(&p->lzmaProps);
  p->blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO;
  p->numBlockThreads_Reduced = -1;
  p->numBlockThreads_Max = -1;
  p->numTotalThreads = -1;
  p->numThreadGroups = 0;
}

void Lzma2EncProps_Normalize(CLzma2EncProps *p)
{
  UInt64 fileSize;
  int t1, t1n, t2, t2r, t3;
  {
    CLzmaEncProps lzmaProps = p->lzmaProps;
    LzmaEncProps_Normalize(&lzmaProps);
    t1n = lzmaProps.numThreads;
  }

  t1 = p->lzmaProps.numThreads;
  t2 = p->numBlockThreads_Max;
  t3 = p->numTotalThreads;

  if (t2 > MTCODER_THREADS_MAX)
    t2 = MTCODER_THREADS_MAX;

  if (t3 <= 0)
  {
    if (t2 <= 0)
      t2 = 1;
    t3 = t1n * t2;
  }
  else if (t2 <= 0)
  {
    t2 = t3 / t1n;
    if (t2 == 0)
    {
      t1 = 1;
      t2 = t3;
    }
    if (t2 > MTCODER_THREADS_MAX)
      t2 = MTCODER_THREADS_MAX;
  }
  else if (t1 <= 0)
  {
    t1 = t3 / t2;
    if (t1 == 0)
      t1 = 1;
  }
  else
    t3 = t1n * t2;

  p->lzmaProps.numThreads = t1;

  t2r = t2;

  fileSize = p->lzmaProps.reduceSize;

  if (   p->blockSize != LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID
      && p->blockSize != LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO
      && (p->blockSize < fileSize || fileSize == (UInt64)(Int64)-1))
    p->lzmaProps.reduceSize = p->blockSize;

  LzmaEncProps_Normalize(&p->lzmaProps);

  p->lzmaProps.reduceSize = fileSize;

  t1 = p->lzmaProps.numThreads;

  if (p->blockSize == LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID)
  {
    t2r = t2 = 1;
    t3 = t1;
  }
  else if (p->blockSize == LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO && t2 <= 1)
  {
    /* if there is no block multi-threading, we use SOLID block */
    p->blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID;
  }
  else
  {
    if (p->blockSize == LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO)
    {
      const UInt32 kMinSize = (UInt32)1 << 20;
      const UInt32 kMaxSize = (UInt32)1 << 28;
      const UInt32 dictSize = p->lzmaProps.dictSize;
      UInt64 blockSize = (UInt64)dictSize << 2;
      if (blockSize < kMinSize) blockSize = kMinSize;
      if (blockSize > kMaxSize) blockSize = kMaxSize;
      if (blockSize < dictSize) blockSize = dictSize;
      blockSize += (kMinSize - 1);
      blockSize &= ~(UInt64)(kMinSize - 1);
      p->blockSize = blockSize;
    }
    
    if (t2 > 1 && fileSize != (UInt64)(Int64)-1)
    {
      UInt64 numBlocks = fileSize / p->blockSize;
      if (numBlocks * p->blockSize != fileSize)
        numBlocks++;
      if (numBlocks < (unsigned)t2)
      {
        t2r = (int)numBlocks;
        if (t2r == 0)
          t2r = 1;
        t3 = t1 * t2r;
      }
    }
  }
  
  p->numBlockThreads_Max = t2;
  p->numBlockThreads_Reduced = t2r;
  p->numTotalThreads = t3;
}


static SRes Progress(ICompressProgressPtr p, UInt64 inSize, UInt64 outSize)
{
  return (p && ICompressProgress_Progress(p, inSize, outSize) != SZ_OK) ? SZ_ERROR_PROGRESS : SZ_OK;
}


/* ---------- Lzma2 ---------- */

struct CLzma2Enc
{
  Byte propEncoded;
  CLzma2EncProps props;
  UInt64 expectedDataSize;
  
  Byte *tempBufLzma;

  ISzAllocPtr alloc;
  ISzAllocPtr allocBig;

  CLzma2EncInt coders[MTCODER_THREADS_MAX];

  #ifndef Z7_ST
  
  ISeqOutStreamPtr outStream;
  Byte *outBuf;
  size_t outBuf_Rem;   /* remainder in outBuf */

  size_t outBufSize;   /* size of allocated outBufs[i] */
  size_t outBufsDataSizes[MTCODER_BLOCKS_MAX];
  BoolInt mtCoder_WasConstructed;
  CMtCoder mtCoder;
  Byte *outBufs[MTCODER_BLOCKS_MAX];

  #endif
};



CLzma2EncHandle Lzma2Enc_Create(ISzAllocPtr alloc, ISzAllocPtr allocBig)
{
  CLzma2Enc *p = (CLzma2Enc *)ISzAlloc_Alloc(alloc, sizeof(CLzma2Enc));
  if (!p)
    return NULL;
  Lzma2EncProps_Init(&p->props);
  Lzma2EncProps_Normalize(&p->props);
  p->expectedDataSize = (UInt64)(Int64)-1;
  p->tempBufLzma = NULL;
  p->alloc = alloc;
  p->allocBig = allocBig;
  {
    unsigned i;
    for (i = 0; i < MTCODER_THREADS_MAX; i++)
      p->coders[i].enc = NULL;
  }
  
  #ifndef Z7_ST
  p->mtCoder_WasConstructed = False;
  {
    unsigned i;
    for (i = 0; i < MTCODER_BLOCKS_MAX; i++)
      p->outBufs[i] = NULL;
    p->outBufSize = 0;
  }
  #endif

  return (CLzma2EncHandle)p;
}


#ifndef Z7_ST

static void Lzma2Enc_FreeOutBufs(CLzma2Enc *p)
{
  unsigned i;
  for (i = 0; i < MTCODER_BLOCKS_MAX; i++)
    if (p->outBufs[i])
    {
      ISzAlloc_Free(p->alloc, p->outBufs[i]);
      p->outBufs[i] = NULL;
    }
  p->outBufSize = 0;
}

#endif

// #define GET_CLzma2Enc_p  CLzma2Enc *p = (CLzma2Enc *)(void *)p;

void Lzma2Enc_Destroy(CLzma2EncHandle p)
{
  // GET_CLzma2Enc_p
  unsigned i;
  for (i = 0; i < MTCODER_THREADS_MAX; i++)
  {
    CLzma2EncInt *t = &p->coders[i];
    if (t->enc)
    {
      LzmaEnc_Destroy(t->enc, p->alloc, p->allocBig);
      t->enc = NULL;
    }
  }


  #ifndef Z7_ST
  if (p->mtCoder_WasConstructed)
  {
    MtCoder_Destruct(&p->mtCoder);
    p->mtCoder_WasConstructed = False;
  }
  Lzma2Enc_FreeOutBufs(p);
  #endif

  ISzAlloc_Free(p->alloc, p->tempBufLzma);
  p->tempBufLzma = NULL;

  ISzAlloc_Free(p->alloc, p);
}


SRes Lzma2Enc_SetProps(CLzma2EncHandle p, const CLzma2EncProps *props)
{
  // GET_CLzma2Enc_p
  CLzmaEncProps lzmaProps = props->lzmaProps;
  LzmaEncProps_Normalize(&lzmaProps);
  if (lzmaProps.lc + lzmaProps.lp > LZMA2_LCLP_MAX)
    return SZ_ERROR_PARAM;
  p->props = *props;
  Lzma2EncProps_Normalize(&p->props);
  return SZ_OK;
}


void Lzma2Enc_SetDataSize(CLzma2EncHandle p, UInt64 expectedDataSiize)
{
  // GET_CLzma2Enc_p
  p->expectedDataSize = expectedDataSiize;
}


Byte Lzma2Enc_WriteProperties(CLzma2EncHandle p)
{
  // GET_CLzma2Enc_p
  unsigned i;
  UInt32 dicSize = LzmaEncProps_GetDictSize(&p->props.lzmaProps);
  for (i = 0; i < 40; i++)
    if (dicSize <= LZMA2_DIC_SIZE_FROM_PROP(i))
      break;
  return (Byte)i;
}


static SRes Lzma2Enc_EncodeMt1(
    CLzma2Enc *me,
    CLzma2EncInt *p,
    ISeqOutStreamPtr outStream,
    Byte *outBuf, size_t *outBufSize,
    ISeqInStreamPtr inStream,
    const Byte *inData, size_t inDataSize,
    int finished,
    ICompressProgressPtr progress)
{
  UInt64 unpackTotal = 0;
  UInt64 packTotal = 0;
  size_t outLim = 0;
  CLimitedSeqInStream limitedInStream;

  if (outBuf)
  {
    outLim = *outBufSize;
    *outBufSize = 0;
  }

  if (!p->enc)
  {
    p->propsAreSet = False;
    p->enc = LzmaEnc_Create(me->alloc);
    if (!p->enc)
      return SZ_ERROR_MEM;
  }

  limitedInStream.realStream = inStream;
  if (inStream)
  {
    limitedInStream.vt.Read = LimitedSeqInStream_Read;
  }
  
  if (!outBuf)
  {
    // outStream version works only in one thread. So we use CLzma2Enc::tempBufLzma
    if (!me->tempBufLzma)
    {
      me->tempBufLzma = (Byte *)ISzAlloc_Alloc(me->alloc, LZMA2_CHUNK_SIZE_COMPRESSED_MAX);
      if (!me->tempBufLzma)
        return SZ_ERROR_MEM;
    }
  }

  RINOK(Lzma2EncInt_InitStream(p, &me->props))

  for (;;)
  {
    SRes res = SZ_OK;
    SizeT inSizeCur = 0;

    Lzma2EncInt_InitBlock(p);
    
    LimitedSeqInStream_Init(&limitedInStream);
    limitedInStream.limit = me->props.blockSize;

    if (inStream)
    {
      UInt64 expected = (UInt64)(Int64)-1;
      // inStream version works only in one thread. So we use CLzma2Enc::expectedDataSize
      if (me->expectedDataSize != (UInt64)(Int64)-1
          && me->expectedDataSize >= unpackTotal)
        expected = me->expectedDataSize - unpackTotal;
      if (me->props.blockSize != LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID
          && expected > me->props.blockSize)
        expected = (size_t)me->props.blockSize;

      LzmaEnc_SetDataSize(p->enc, expected);

      RINOK(LzmaEnc_PrepareForLzma2(p->enc,
          &limitedInStream.vt,
          LZMA2_KEEP_WINDOW_SIZE,
          me->alloc,
          me->allocBig))
    }
    else
    {
      inSizeCur = (SizeT)(inDataSize - (size_t)unpackTotal);
      if (me->props.blockSize != LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID
          && inSizeCur > me->props.blockSize)
        inSizeCur = (SizeT)(size_t)me->props.blockSize;
    
      // LzmaEnc_SetDataSize(p->enc, inSizeCur);
      
      RINOK(LzmaEnc_MemPrepare(p->enc,
          inData + (size_t)unpackTotal, inSizeCur,
          LZMA2_KEEP_WINDOW_SIZE,
          me->alloc,
          me->allocBig))
    }

    for (;;)
    {
      size_t packSize = LZMA2_CHUNK_SIZE_COMPRESSED_MAX;
      if (outBuf)
        packSize = outLim - (size_t)packTotal;
      
      res = Lzma2EncInt_EncodeSubblock(p,
          outBuf ? outBuf + (size_t)packTotal : me->tempBufLzma, &packSize,
          outBuf ? NULL : outStream);
      
      if (res != SZ_OK)
        break;

      packTotal += packSize;
      if (outBuf)
        *outBufSize = (size_t)packTotal;
      
      res = Progress(progress, unpackTotal + p->srcPos, packTotal);
      if (res != SZ_OK)
        break;

      /*
      if (LzmaEnc_GetNumAvailableBytes(p->enc) == 0)
        break;
      */

      if (packSize == 0)
        break;
    }
    
    LzmaEnc_Finish(p->enc);
    
    unpackTotal += p->srcPos;
    
    RINOK(res)

    if (p->srcPos != (inStream ? limitedInStream.processed : inSizeCur))
      return SZ_ERROR_FAIL;
    
    if (inStream ? limitedInStream.finished : (unpackTotal == inDataSize))
    {
      if (finished)
      {
        if (outBuf)
        {
          const size_t destPos = *outBufSize;
          if (destPos >= outLim)
            return SZ_ERROR_OUTPUT_EOF;
          outBuf[destPos] = LZMA2_CONTROL_EOF; // 0
          *outBufSize = destPos + 1;
        }
        else
        {
          const Byte b = LZMA2_CONTROL_EOF; // 0;
          if (ISeqOutStream_Write(outStream, &b, 1) != 1)
            return SZ_ERROR_WRITE;
        }
      }
      return SZ_OK;
    }
  }
}



#ifndef Z7_ST

static SRes Lzma2Enc_MtCallback_Code(void *p, unsigned coderIndex, unsigned outBufIndex,
    const Byte *src, size_t srcSize, int finished)
{
  CLzma2Enc *me = (CLzma2Enc *)p;
  size_t destSize = me->outBufSize;
  SRes res;
  CMtProgressThunk progressThunk;

  Byte *dest = me->outBufs[outBufIndex];

  me->outBufsDataSizes[outBufIndex] = 0;

  if (!dest)
  {
    dest = (Byte *)ISzAlloc_Alloc(me->alloc, me->outBufSize);
    if (!dest)
      return SZ_ERROR_MEM;
    me->outBufs[outBufIndex] = dest;
  }

  MtProgressThunk_CreateVTable(&progressThunk);
  progressThunk.mtProgress = &me->mtCoder.mtProgress;
  progressThunk.inSize = 0;
  progressThunk.outSize = 0;

  res = Lzma2Enc_EncodeMt1(me,
      &me->coders[coderIndex],
      NULL, dest, &destSize,
      NULL, src, srcSize,
      finished,
      &progressThunk.vt);

  me->outBufsDataSizes[outBufIndex] = destSize;

  return res;
}


static SRes Lzma2Enc_MtCallback_Write(void *p, unsigned outBufIndex)
{
  CLzma2Enc *me = (CLzma2Enc *)p;
  size_t size = me->outBufsDataSizes[outBufIndex];
  const Byte *data = me->outBufs[outBufIndex];
  
  if (me->outStream)
    return ISeqOutStream_Write(me->outStream, data, size) == size ? SZ_OK : SZ_ERROR_WRITE;
  
  if (size > me->outBuf_Rem)
    return SZ_ERROR_OUTPUT_EOF;
  memcpy(me->outBuf, data, size);
  me->outBuf_Rem -= size;
  me->outBuf += size;
  return SZ_OK;
}

#endif



SRes Lzma2Enc_Encode2(CLzma2EncHandle p,
    ISeqOutStreamPtr outStream,
    Byte *outBuf, size_t *outBufSize,
    ISeqInStreamPtr inStream,
    const Byte *inData, size_t inDataSize,
    ICompressProgressPtr progress)
{
  // GET_CLzma2Enc_p

  if (inStream && inData)
    return SZ_ERROR_PARAM;

  if (outStream && outBuf)
    return SZ_ERROR_PARAM;

  {
    unsigned i;
    for (i = 0; i < MTCODER_THREADS_MAX; i++)
      p->coders[i].propsAreSet = False;
  }

  #ifndef Z7_ST
  
  if (p->props.numBlockThreads_Reduced > 1)
  {
    IMtCoderCallback2 vt;

    if (!p->mtCoder_WasConstructed)
    {
      p->mtCoder_WasConstructed = True;
      MtCoder_Construct(&p->mtCoder);
    }

    vt.Code = Lzma2Enc_MtCallback_Code;
    vt.Write = Lzma2Enc_MtCallback_Write;

    p->outStream = outStream;
    p->outBuf = NULL;
    p->outBuf_Rem = 0;
    if (!outStream)
    {
      p->outBuf = outBuf;
      p->outBuf_Rem = *outBufSize;
      *outBufSize = 0;
    }

    p->mtCoder.allocBig = p->allocBig;
    p->mtCoder.progress = progress;
    p->mtCoder.inStream = inStream;
    p->mtCoder.inData = inData;
    p->mtCoder.inDataSize = inDataSize;
    p->mtCoder.mtCallback = &vt;
    p->mtCoder.mtCallbackObject = p;

    p->mtCoder.blockSize = (size_t)p->props.blockSize;
    if (p->mtCoder.blockSize != p->props.blockSize)
      return SZ_ERROR_PARAM; /* SZ_ERROR_MEM */

    {
      const size_t destBlockSize = p->mtCoder.blockSize + (p->mtCoder.blockSize >> 10) + 16;
      if (destBlockSize < p->mtCoder.blockSize)
        return SZ_ERROR_PARAM;
      if (p->outBufSize != destBlockSize)
        Lzma2Enc_FreeOutBufs(p);
      p->outBufSize = destBlockSize;
    }

    p->mtCoder.numThreadsMax = (unsigned)p->props.numBlockThreads_Max;
    p->mtCoder.numThreadGroups = p->props.numThreadGroups;
    p->mtCoder.expectedDataSize = p->expectedDataSize;
    
    {
      const SRes res = MtCoder_Code(&p->mtCoder);
      if (!outStream)
        *outBufSize = (size_t)(p->outBuf - outBuf);
      return res;
    }
  }

  #endif


  return Lzma2Enc_EncodeMt1(p,
      &p->coders[0],
      outStream, outBuf, outBufSize,
      inStream, inData, inDataSize,
      True, /* finished */
      progress);
}

#undef PRF
