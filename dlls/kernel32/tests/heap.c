/*
 * Unit test suite for heap functions
 *
 * Copyright 2002 Geoffrey Hausheer
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2006 Detlef Riekenberg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/test.h"

/* some undocumented flags (names are made up) */
#define HEAP_PRIVATE          0x00001000
#define HEAP_PAGE_ALLOCS      0x01000000
#define HEAP_VALIDATE         0x10000000
#define HEAP_VALIDATE_ALL     0x20000000
#define HEAP_VALIDATE_PARAMS  0x40000000

/* use function pointers to avoid warnings for invalid parameter tests */
static LPVOID (WINAPI *pHeapAlloc)(HANDLE,DWORD,SIZE_T);
static LPVOID (WINAPI *pHeapReAlloc)(HANDLE,DWORD,LPVOID,SIZE_T);
static BOOL (WINAPI *pGetPhysicallyInstalledSystemMemory)( ULONGLONG * );

#define MAKE_FUNC(f) static typeof(f) *p ## f
MAKE_FUNC( HeapQueryInformation );
MAKE_FUNC( GlobalFlags );
MAKE_FUNC( RtlGetNtGlobalFlags );
#undef MAKE_FUNC

static void load_functions(void)
{
    HMODULE kernel32 = GetModuleHandleW( L"kernel32.dll" );
    HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );

#define LOAD_FUNC(m, f) p ## f = (void *)GetProcAddress( m, #f );
    LOAD_FUNC( kernel32, HeapAlloc );
    LOAD_FUNC( kernel32, HeapReAlloc );
    LOAD_FUNC( kernel32, HeapQueryInformation );
    LOAD_FUNC( kernel32, GetPhysicallyInstalledSystemMemory );
    LOAD_FUNC( kernel32, GlobalFlags );
    LOAD_FUNC( ntdll, RtlGetNtGlobalFlags );
#undef LOAD_FUNC
}

struct heap
{
    UINT_PTR unknown1[2];
    UINT     ffeeffee;
    UINT     auto_flags;
    UINT_PTR unknown2[7];
    UINT     unknown3[2];
    UINT_PTR unknown4[3];
    UINT     flags;
    UINT     force_flags;
};


static void test_HeapCreate(void)
{
    SIZE_T alloc_size = 0x8000 * sizeof(void *), size, i;
    BYTE *ptr, *ptr1, *ptrs[128];
    HANDLE heap, heap1;
    UINT_PTR align;
    BOOL ret;

    /* check heap alignment */

    heap = HeapCreate( 0, 0, 0 );
    ok( !!heap, "HeapCreate failed, error %lu\n", GetLastError() );
    ok( !((ULONG_PTR)heap & 0xffff), "wrong heap alignment\n" );
    heap1 = HeapCreate( 0, 0, 0 );
    ok( !!heap, "HeapCreate failed, error %lu\n", GetLastError() );
    ok( !((ULONG_PTR)heap1 & 0xffff), "wrong heap alignment\n" );
    ret = HeapDestroy( heap1 );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );
    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );

    /* growable heap */

    heap = HeapCreate( 0, 0, 0 );
    ok( !!heap, "HeapCreate failed, error %lu\n", GetLastError() );
    ok( !((ULONG_PTR)heap & 0xffff), "wrong heap alignment\n" );

    /* test some border cases */

    ret = HeapFree( heap, 0, NULL );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ptr = HeapReAlloc( heap, 0, NULL, 1 );
    ok( !ptr, "HeapReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == NO_ERROR, "got error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, 0, 0 );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 0, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    ptr1 = pHeapReAlloc( heap, 0, ptr, ~(SIZE_T)0 - 7 );
    ok( !ptr1, "HeapReAlloc succeeded\n" );
    ptr1 = pHeapReAlloc( heap, 0, ptr, ~(SIZE_T)0 );
    ok( !ptr1, "HeapReAlloc succeeded\n" );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = pHeapAlloc( heap, 0, ~(SIZE_T)0 );
    ok( !ptr, "HeapAlloc succeeded\n" );

    ptr = HeapAlloc( heap, 0, 1 );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    ptr1 = HeapReAlloc( heap, 0, ptr, 0 );
    ok( !!ptr1, "HeapReAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr1 );
    ok( size == 0, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, 0, 5 * alloc_size + 1 );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, 0, alloc_size );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    ptr1 = HeapAlloc( heap, 0, 4 * alloc_size );
    ok( !!ptr1, "HeapAlloc failed, error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    /* test pointer alignment */

    align = 0;
    for (i = 0; i < ARRAY_SIZE(ptrs); ++i)
    {
        ptrs[i] = HeapAlloc( heap, 0, alloc_size );
        ok( !!ptrs[i], "HeapAlloc failed, error %lu\n", GetLastError() );
        align |= (UINT_PTR)ptrs[i];
    }
    ok( !(align & (2 * sizeof(void *) - 1)), "got wrong alignment\n" );
    ok( align & (2 * sizeof(void *)), "got wrong alignment\n" );
    for (i = 0; i < ARRAY_SIZE(ptrs); ++i)
    {
        ret = HeapFree( heap, 0, ptrs[i] );
        ok( ret, "HeapFree failed, error %lu\n", GetLastError() );
    }

    align = 0;
    for (i = 0; i < ARRAY_SIZE(ptrs); ++i)
    {
        ptrs[i] = HeapAlloc( heap, 0, 4 * alloc_size );
        ok( !!ptrs[i], "HeapAlloc failed, error %lu\n", GetLastError() );
        align |= (UINT_PTR)ptrs[i];
    }
    todo_wine_if( sizeof(void *) == 8 )
    ok( !(align & (8 * sizeof(void *) - 1)), "got wrong alignment\n" );
    todo_wine_if( sizeof(void *) == 8 )
    ok( align & (8 * sizeof(void *)), "got wrong alignment\n" );
    for (i = 0; i < ARRAY_SIZE(ptrs); ++i)
    {
        ret = HeapFree( heap, 0, ptrs[i] );
        ok( ret, "HeapFree failed, error %lu\n", GetLastError() );
    }

    /* test HEAP_ZERO_MEMORY */

    ptr = HeapAlloc( heap, HEAP_ZERO_MEMORY, 1 );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 1, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, HEAP_ZERO_MEMORY, (1 << 20) );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == (1 << 20), "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, HEAP_ZERO_MEMORY, alloc_size );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    ptr = HeapReAlloc( heap, HEAP_ZERO_MEMORY, ptr, 3 * alloc_size );
    ok( !!ptr, "HeapReAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 3 * alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    /* shrinking a small-ish block in place and growing back is okay */
    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, alloc_size * 3 / 2 );
    ok( ptr1 == ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY failed, error %lu\n", GetLastError() );
    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, 2 * alloc_size );
    ok( ptr1 == ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY failed, error %lu\n", GetLastError() );

    ptr = HeapReAlloc( heap, HEAP_ZERO_MEMORY, ptr, 1 );
    ok( !!ptr, "HeapReAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 1, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    ptr = HeapReAlloc( heap, HEAP_ZERO_MEMORY, ptr, (1 << 20) );
    ok( !!ptr, "HeapReAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == (1 << 20), "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    /* shrinking a very large block decommits pages and fail to grow in place */
    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, alloc_size * 3 / 2 );
    ok( ptr1 == ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY failed, error %lu\n", GetLastError() );
    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, 2 * alloc_size );
    todo_wine
    ok( ptr1 != ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY succeeded\n" );
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );

    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );


    /* fixed size heaps */

    heap = HeapCreate( 0, alloc_size, alloc_size );
    ok( !!heap, "HeapCreate failed, error %lu\n", GetLastError() );
    ok( !((ULONG_PTR)heap & 0xffff), "wrong heap alignment\n" );

    ptr = HeapAlloc( heap, 0, alloc_size - (0x400 + 0x80 * sizeof(void *)) );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == alloc_size - (0x400 + 0x80 * sizeof(void *)),
        "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ptr1 = HeapAlloc( heap, 0, alloc_size - (0x200 + 0x80 * sizeof(void *)) );
    todo_wine
    ok( !ptr1, "HeapAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );


    heap = HeapCreate( 0, 2 * alloc_size, 5 * alloc_size );
    ok( !!heap, "HeapCreate failed, error %lu\n", GetLastError() );
    ok( !((ULONG_PTR)heap & 0xffff), "wrong heap alignment\n" );

    ptr = HeapAlloc( heap, 0, 0 );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 0, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    /* cannot allocate large blocks from fixed size heap */

    SetLastError( 0xdeadbeef );
    ptr1 = HeapAlloc( heap, 0, 3 * alloc_size );
    todo_wine
    ok( !ptr1, "HeapAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, 0, alloc_size );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr1 = HeapAlloc( heap, 0, 4 * alloc_size );
    ok( !ptr1, "HeapAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ptr = HeapAlloc( heap, HEAP_ZERO_MEMORY, alloc_size );
    ok( !!ptr, "HeapAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    ptr = HeapReAlloc( heap, HEAP_ZERO_MEMORY, ptr, 2 * alloc_size );
    ok( !!ptr, "HeapReAlloc failed, error %lu\n", GetLastError() );
    size = HeapSize( heap, 0, ptr );
    ok( size == 2 * alloc_size, "HeapSize returned %#Ix, error %lu\n", size, GetLastError() );
    while (size) if (ptr[--size]) break;
    ok( !size && !ptr[0], "memory wasn't zeroed\n" );

    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, alloc_size * 3 / 2 );
    ok( ptr1 == ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY failed, error %lu\n", GetLastError() );
    ptr1 = HeapReAlloc( heap, HEAP_REALLOC_IN_PLACE_ONLY, ptr, 2 * alloc_size );
    ok( ptr1 == ptr, "HeapReAlloc HEAP_REALLOC_IN_PLACE_ONLY failed, error %lu\n", GetLastError() );
    ret = HeapFree( heap, 0, ptr1 );
    ok( ret, "HeapFree failed, error %lu\n", GetLastError() );

    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );
}


struct mem_entry
{
    UINT_PTR flags;
    void *ptr;
};

static struct mem_entry *mem_entry_from_HANDLE( HLOCAL handle )
{
    return CONTAINING_RECORD( handle, struct mem_entry, ptr );
}

static void test_GlobalAlloc(void)
{
    static const UINT flags_tests[] =
    {
        GMEM_FIXED | GMEM_NOTIFY,
        GMEM_FIXED | GMEM_DISCARDABLE,
        GMEM_MOVEABLE | GMEM_NOTIFY,
        GMEM_MOVEABLE | GMEM_DDESHARE,
        GMEM_MOVEABLE | GMEM_NOT_BANKED,
        GMEM_MOVEABLE | GMEM_NODISCARD,
        GMEM_MOVEABLE | GMEM_DISCARDABLE,
        GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_DISCARDABLE | GMEM_LOWER | GMEM_NOCOMPACT | GMEM_NODISCARD | GMEM_NOT_BANKED | GMEM_NOTIFY,
    };
    static const char zero_buffer[100000] = {0};
    static const SIZE_T buffer_size = ARRAY_SIZE(zero_buffer);
    const HGLOBAL invalid_mem = LongToHandle( 0xdeadbee0 + sizeof(void *) );
    void *const invalid_ptr = LongToHandle( 0xdeadbee0 );
    struct mem_entry *entry;
    HGLOBAL globals[0x10000];
    SIZE_T size, alloc_size;
    HGLOBAL mem, tmp_mem;
    BYTE *ptr, *tmp_ptr;
    UINT i, flags;
    BOOL ret;

    mem = GlobalFree( 0 );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );
    mem = GlobalReAlloc( 0, 10, GMEM_MOVEABLE );
    ok( !mem, "GlobalReAlloc succeeded\n" );

    for (i = 0; i < ARRAY_SIZE(globals); ++i)
    {
        mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 0 );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        globals[i] = mem;
    }

    SetLastError( 0xdeadbeef );
    mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 0 );
    ok( !mem, "GlobalAlloc succeeded\n" );
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    mem = LocalAlloc( LMEM_MOVEABLE | LMEM_DISCARDABLE, 0 );
    ok( !mem, "LocalAlloc succeeded\n" );
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_DISCARDABLE, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    for (i = 0; i < ARRAY_SIZE(globals); ++i)
    {
        mem = GlobalFree( globals[i] );
        ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );
    }

    mem = GlobalAlloc( GMEM_MOVEABLE, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    mem = GlobalReAlloc( mem, 10, GMEM_MOVEABLE );
    ok( !!mem, "GlobalReAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size >= 10 && size <= 16, "GlobalSize returned %Iu\n", size );
    mem = GlobalReAlloc( mem, 0, GMEM_MOVEABLE );
    ok( !!mem, "GlobalReAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size == 0, "GlobalSize returned %Iu\n", size );
    mem = GlobalReAlloc( mem, 10, GMEM_MOVEABLE );
    ok( !!mem, "GlobalReAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size >= 10 && size <= 16, "GlobalSize returned %Iu\n", size );
    tmp_mem = GlobalFree( mem );
    ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size == 0, "GlobalSize returned %Iu\n", size );

    mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    entry = mem_entry_from_HANDLE( mem );
    size = GlobalSize( mem );
    ok( size == 0, "GlobalSize returned %Iu\n", size );
    ret = HeapValidate( GetProcessHeap(), 0, entry );
    ok( !ret, "HeapValidate succeeded\n" );
    ok( entry->flags == 0xf, "got unexpected flags %#Ix\n", entry->flags );
    ok( !entry->ptr, "got unexpected ptr %p\n", entry->ptr );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_MOVEABLE, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    entry = mem_entry_from_HANDLE( mem );
    size = GlobalSize( mem );
    ok( size == 0, "GlobalSize returned %Iu\n", size );
    ret = HeapValidate( GetProcessHeap(), 0, entry );
    ok( !ret, "HeapValidate succeeded\n" );
    ok( entry->flags == 0xb, "got unexpected flags %#Ix\n", entry->flags );
    ok( !entry->ptr, "got unexpected ptr %p\n", entry->ptr );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    for (alloc_size = 1; alloc_size < 0x10000000; alloc_size <<= 1)
    {
        winetest_push_context( "size %#Ix", alloc_size );

        mem = GlobalAlloc( GMEM_FIXED, alloc_size );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        ok( !((UINT_PTR)mem & sizeof(void *)), "got unexpected ptr align\n" );
        ok( !((UINT_PTR)mem & (sizeof(void *) - 1)), "got unexpected ptr align\n" );
        ret = HeapValidate( GetProcessHeap(), 0, mem );
        ok( ret, "HeapValidate failed, error %lu\n", GetLastError() );
        tmp_mem = GlobalFree( mem );
        ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );

        mem = GlobalAlloc( GMEM_MOVEABLE, alloc_size );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        ok( ((UINT_PTR)mem & sizeof(void *)), "got unexpected entry align\n" );
        ok( !((UINT_PTR)mem & (sizeof(void *) - 1)), "got unexpected entry align\n" );

        entry = mem_entry_from_HANDLE( mem );
        ret = HeapValidate( GetProcessHeap(), 0, entry );
        ok( !ret, "HeapValidate succeeded\n" );
        ret = HeapValidate( GetProcessHeap(), 0, entry->ptr );
        todo_wine
        ok( ret, "HeapValidate failed, error %lu\n", GetLastError() );
        size = HeapSize( GetProcessHeap(), 0, entry->ptr );
        todo_wine
        ok( size == alloc_size, "HeapSize returned %Iu\n", size );

        ptr = GlobalLock( mem );
        ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
        ok( ptr != mem, "got unexpected ptr %p\n", ptr );
        ok( ptr == entry->ptr, "got unexpected ptr %p\n", ptr );
        ok( !((UINT_PTR)ptr & sizeof(void *)), "got unexpected ptr align\n" );
        ok( !((UINT_PTR)ptr & (sizeof(void *) - 1)), "got unexpected ptr align\n" );
        for (i = 1; i < 0xff; ++i)
        {
            ok( entry->flags == ((i<<16)|3), "got unexpected flags %#Ix\n", entry->flags );
            ptr = GlobalLock( mem );
            ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
        }
        ptr = GlobalLock( mem );
        ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
        ok( entry->flags == 0xff0003, "got unexpected flags %#Ix\n", entry->flags );
        for (i = 1; i < 0xff; ++i)
        {
            ret = GlobalUnlock( mem );
            ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
        }
        ret = GlobalUnlock( mem );
        ok( !ret, "GlobalUnlock succeeded, error %lu\n", GetLastError() );
        ok( entry->flags == 0x3, "got unexpected flags %#Ix\n", entry->flags );

        tmp_mem = GlobalFree( mem );
        ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );
        ok( !!entry->flags, "got unexpected flags %#Ix\n", entry->flags );
        ok( !((UINT_PTR)entry->flags & sizeof(void *)), "got unexpected ptr align\n" );
        ok( !((UINT_PTR)entry->flags & (sizeof(void *) - 1)), "got unexpected ptr align\n" );
        ok( !entry->ptr, "got unexpected ptr %p\n", entry->ptr );

        mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 0 );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        entry = mem_entry_from_HANDLE( mem );
        ok( entry->flags == 0xf, "got unexpected flags %#Ix\n", entry->flags );
        ok( !entry->ptr, "got unexpected ptr %p\n", entry->ptr );
        flags = GlobalFlags( mem );
        ok( flags == (GMEM_DISCARDED | GMEM_DISCARDABLE), "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
        mem = GlobalFree( mem );
        ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

        mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 1 );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        entry = mem_entry_from_HANDLE( mem );
        ok( entry->flags == 0x7, "got unexpected flags %#Ix\n", entry->flags );
        ok( !!entry->ptr, "got unexpected ptr %p\n", entry->ptr );
        flags = GlobalFlags( mem );
        ok( flags == GMEM_DISCARDABLE, "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
        mem = GlobalFree( mem );
        ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

        mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE | GMEM_DDESHARE, 1 );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        entry = mem_entry_from_HANDLE( mem );
        ok( entry->flags == 0x8007, "got unexpected flags %#Ix\n", entry->flags );
        ok( !!entry->ptr, "got unexpected ptr %p\n", entry->ptr );
        flags = GlobalFlags( mem );
        ok( flags == (GMEM_DISCARDABLE | GMEM_DDESHARE), "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
        mem = GlobalFree( mem );
        ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

        winetest_pop_context();
    }

    mem = GlobalAlloc( GMEM_MOVEABLE, 256 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    ptr = GlobalLock( mem );
    ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( ptr != mem, "got unexpected ptr %p\n", ptr );
    flags = GlobalFlags( mem );
    ok( flags == 1, "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
    tmp_ptr = GlobalLock( mem );
    ok( !!tmp_ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( tmp_ptr == ptr, "got ptr %p, expected %p\n", tmp_ptr, ptr );
    flags = GlobalFlags( mem );
    ok( flags == 2, "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
    ret = GlobalUnlock( mem );
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    flags = GlobalFlags( mem );
    ok( flags == 1, "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = GlobalUnlock( mem );
    ok( !ret, "GlobalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_SUCCESS, "got error %lu\n", GetLastError() );
    flags = GlobalFlags( mem );
    ok( !flags, "GlobalFlags returned %#x, error %lu\n", flags, GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = GlobalUnlock( mem );
    ok( !ret, "GlobalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );
    tmp_mem = GlobalFree( mem );
    ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_DDESHARE, 100 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = GlobalFree( mem );
    ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );
    if (sizeof(void *) != 8) /* crashes on 64-bit */
    {
        SetLastError( 0xdeadbeef );
        tmp_mem = GlobalFree( mem );
        ok( tmp_mem == mem, "GlobalFree succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        size = GlobalSize( (HGLOBAL)0xc042 );
        ok( size == 0, "GlobalSize succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    }

    /* freed handles are caught */
    mem = GlobalAlloc( GMEM_MOVEABLE, 256 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = GlobalFree( mem );
    ok( !tmp_mem, "GlobalFree failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalFree( mem );
    ok( tmp_mem == mem, "GlobalFree succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = GlobalFlags( mem );
    ok( flags == GMEM_INVALID_HANDLE, "GlobalFlags succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = GlobalSize( mem );
    ok( size == 0, "GlobalSize succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = GlobalLock( mem );
    ok( !ptr, "GlobalLock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = GlobalUnlock( mem );
    todo_wine
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
#if 0 /* corrupts Wine heap */
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalReAlloc( mem, 0, GMEM_MOVEABLE );
    todo_wine
    ok( !tmp_mem, "GlobalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
#endif

    /* invalid handles are caught */
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalFree( invalid_mem );
    ok( tmp_mem == invalid_mem, "GlobalFree succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = GlobalFlags( invalid_mem );
    ok( flags == GMEM_INVALID_HANDLE, "GlobalFlags succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = GlobalSize( invalid_mem );
    ok( size == 0, "GlobalSize succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = GlobalLock( invalid_mem );
    ok( !ptr, "GlobalLock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = GlobalUnlock( invalid_mem );
    todo_wine
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalReAlloc( invalid_mem, 0, GMEM_MOVEABLE );
    ok( !tmp_mem, "GlobalReAlloc succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );

    /* invalid pointers are caught */
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalFree( invalid_ptr );
    ok( tmp_mem == invalid_ptr, "GlobalFree succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOACCESS, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = GlobalFlags( invalid_ptr );
    todo_wine
    ok( flags == GMEM_INVALID_HANDLE, "GlobalFlags succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = GlobalSize( invalid_ptr );
    ok( size == 0, "GlobalSize succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = GlobalLock( invalid_ptr );
    ok( !ptr, "GlobalLock succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = GlobalUnlock( invalid_ptr );
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    ok( GetLastError() == 0xdeadbeef, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalReAlloc( invalid_ptr, 0, GMEM_MOVEABLE );
    ok( !tmp_mem, "GlobalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOACCESS, "got error %lu\n", GetLastError() );

    /* GMEM_FIXED block doesn't allow resize, though it succeeds with GMEM_MODIFY */
    mem = GlobalAlloc( GMEM_FIXED, 10 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = GlobalReAlloc( mem, 9, GMEM_MODIFY );
    todo_wine
    ok( !!tmp_mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem == mem, "got ptr %p, expected %p\n", tmp_mem, mem );
    size = GlobalSize( mem );
    ok( size == 10, "GlobalSize returned %Iu\n", size );
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalReAlloc( mem, 10, 0 );
    todo_wine
    ok( !tmp_mem || broken( tmp_mem == mem ) /* w1064v1507 / w1064v1607 */,
        "GlobalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY || broken( GetLastError() == 0xdeadbeef ) /* w1064v1507 / w1064v1607 */,
        "got error %lu\n", GetLastError() );
    if (tmp_mem) mem = tmp_mem;
    tmp_mem = GlobalReAlloc( mem, 1024 * 1024, GMEM_MODIFY );
    todo_wine
    ok( !!tmp_mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem == mem, "got ptr %p, expected %p\n", tmp_mem, mem );
    size = GlobalSize( mem );
    ok( size == 10, "GlobalSize returned %Iu\n", size );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    /* GMEM_FIXED block can be relocated with GMEM_MOVEABLE */
    mem = GlobalAlloc( GMEM_FIXED, 10 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = GlobalReAlloc( mem, 11, GMEM_MOVEABLE );
    ok( !!tmp_mem, "GlobalReAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem != mem || broken( tmp_mem == mem ) /* w1064v1507 / w1064v1607 */,
        "GlobalReAlloc didn't relocate memory\n" );
    ptr = GlobalLock( tmp_mem );
    ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( ptr == tmp_mem, "got ptr %p, expected %p\n", ptr, tmp_mem );
    GlobalFree( tmp_mem );

    mem = GlobalAlloc( GMEM_DDESHARE, 100 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    ret = GlobalUnlock( mem );
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    ret = GlobalUnlock( mem );
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_FIXED, 100 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    ret = GlobalUnlock( mem );
    ok( ret, "GlobalUnlock failed, error %lu\n", GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_FIXED, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size == 1, "GlobalSize returned %Iu\n", size );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    /* trying to lock empty memory should give an error */
    mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, 0 );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = GlobalLock( mem );
    ok( !ptr, "GlobalLock succeeded\n" );
    ok( GetLastError() == ERROR_DISCARDED, "got error %lu\n", GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( 0, buffer_size );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size >= buffer_size, "GlobalSize returned %Iu, error %lu\n", size, GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    mem = GlobalAlloc( GMEM_ZEROINIT, buffer_size );
    ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
    size = GlobalSize( mem );
    ok( size >= buffer_size, "GlobalSize returned %Iu, error %lu\n", size, GetLastError() );
    ptr = GlobalLock( mem );
    ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( ptr == mem, "got ptr %p, expected %p\n", ptr, mem );
    ok( !memcmp( ptr, zero_buffer, buffer_size ), "GlobalAlloc didn't clear memory\n" );

    /* Check that we can change GMEM_FIXED to GMEM_MOVEABLE */
    mem = GlobalReAlloc( mem, 0, GMEM_MODIFY | GMEM_MOVEABLE );
    ok( !!mem, "GlobalReAlloc failed, error %lu\n", GetLastError() );
    ok( mem != ptr, "GlobalReAlloc returned unexpected handle\n" );
    size = GlobalSize( mem );
    ok( size == buffer_size, "GlobalSize returned %Iu, error %lu\n", size, GetLastError() );

    ptr = GlobalLock( mem );
    ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( ptr != mem, "got unexpected ptr %p\n", ptr );
    ret = GlobalUnlock( mem );
    ok( !ret, "GlobalUnlock succeeded, error %lu\n", GetLastError() );
    ok( GetLastError() == NO_ERROR, "got error %lu\n", GetLastError() );

    tmp_mem = GlobalReAlloc( mem, 2 * buffer_size, GMEM_MOVEABLE | GMEM_ZEROINIT );
    ok( !!tmp_mem, "GlobalReAlloc failed\n" );
    ok( tmp_mem == mem || broken( tmp_mem != mem ) /* happens sometimes?? */,
        "GlobalReAlloc returned unexpected handle\n" );
    mem = tmp_mem;

    size = GlobalSize( mem );
    ok( size >= 2 * buffer_size, "GlobalSize returned %Iu, error %lu\n", size, GetLastError() );
    ptr = GlobalLock( mem );
    ok( !!ptr, "GlobalLock failed, error %lu\n", GetLastError() );
    ok( ptr != mem, "got unexpected ptr %p\n", ptr );
    ok( !memcmp( ptr, zero_buffer, buffer_size ), "GlobalReAlloc didn't clear memory\n" );
    ok( !memcmp( ptr + buffer_size, zero_buffer, buffer_size ),
        "GlobalReAlloc didn't clear memory\n" );

    tmp_mem = GlobalHandle( ptr );
    ok( tmp_mem == mem, "GlobalHandle returned unexpected handle\n" );
    /* Check that we can't discard locked memory */
    SetLastError( 0xdeadbeef );
    tmp_mem = GlobalDiscard( mem );
    ok( !tmp_mem, "GlobalDiscard succeeded\n" );
    ret = GlobalUnlock( mem );
    ok( !ret, "GlobalUnlock succeeded, error %lu\n", GetLastError() );
    ok( GetLastError() == NO_ERROR, "got error %lu\n", GetLastError() );

    tmp_mem = GlobalDiscard( mem );
    ok( tmp_mem == mem, "GlobalDiscard failed, error %lu\n", GetLastError() );
    mem = GlobalFree( mem );
    ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );

    for (i = 0; i < ARRAY_SIZE(flags_tests); i++)
    {
        mem = GlobalAlloc( flags_tests[i], 4 );
        ok( !!mem, "GlobalAlloc failed, error %lu\n", GetLastError() );
        flags = GlobalFlags( mem );
        ok( !(flags & ~(GMEM_DDESHARE | GMEM_DISCARDABLE)), "got flags %#x, error %lu\n", flags, GetLastError() );
        mem = GlobalFree( mem );
        ok( !mem, "GlobalFree failed, error %lu\n", GetLastError() );
    }
}

static void test_LocalAlloc(void)
{
    static const UINT flags_tests[] =
    {
        LMEM_FIXED,
        LMEM_FIXED | LMEM_DISCARDABLE,
        LMEM_MOVEABLE,
        LMEM_MOVEABLE | LMEM_NODISCARD,
        LMEM_MOVEABLE | LMEM_DISCARDABLE,
        LMEM_MOVEABLE | LMEM_DISCARDABLE | LMEM_NOCOMPACT | LMEM_NODISCARD,
    };
    static const char zero_buffer[100000] = {0};
    static const SIZE_T buffer_size = ARRAY_SIZE(zero_buffer);
    const HLOCAL invalid_mem = LongToHandle( 0xdeadbee0 + sizeof(void *) );
    void *const invalid_ptr = LongToHandle( 0xdeadbee0 );
    HLOCAL locals[0x10000];
    HLOCAL mem, tmp_mem;
    BYTE *ptr, *tmp_ptr;
    UINT i, flags;
    SIZE_T size;
    BOOL ret;

    mem = LocalFree( 0 );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );
    mem = LocalReAlloc( 0, 10, LMEM_MOVEABLE );
    ok( !mem, "LocalReAlloc succeeded\n" );

    for (i = 0; i < ARRAY_SIZE(locals); ++i)
    {
        mem = LocalAlloc( LMEM_MOVEABLE | LMEM_DISCARDABLE, 0 );
        ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
        locals[i] = mem;
    }

    SetLastError( 0xdeadbeef );
    mem = LocalAlloc( LMEM_MOVEABLE | LMEM_DISCARDABLE, 0 );
    ok( !mem, "LocalAlloc succeeded\n" );
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DISCARDABLE, 0 );
    ok( !mem, "GlobalAlloc succeeded\n" );
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "got error %lu\n", GetLastError() );

    mem = LocalAlloc( LMEM_DISCARDABLE, 0 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    for (i = 0; i < ARRAY_SIZE(locals); ++i)
    {
        mem = LocalFree( locals[i] );
        ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );
    }

    mem = LocalAlloc( LMEM_MOVEABLE, 0 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    mem = LocalReAlloc( mem, 10, LMEM_MOVEABLE );
    ok( !!mem, "LocalReAlloc failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size >= 10 && size <= 16, "LocalSize returned %Iu\n", size );
    mem = LocalReAlloc( mem, 0, LMEM_MOVEABLE );
    ok( !!mem, "LocalReAlloc failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size == 0, "LocalSize returned %Iu\n", size );
    mem = LocalReAlloc( mem, 10, LMEM_MOVEABLE );
    ok( !!mem, "LocalReAlloc failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size >= 10 && size <= 16, "LocalSize returned %Iu\n", size );
    tmp_mem = LocalFree( mem );
    ok( !tmp_mem, "LocalFree failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size == 0, "LocalSize returned %Iu\n", size );

    mem = LocalAlloc( LMEM_MOVEABLE, 256 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    ptr = LocalLock( mem );
    ok( !!ptr, "LocalLock failed, error %lu\n", GetLastError() );
    ok( ptr != mem, "got unexpected ptr %p\n", ptr );
    flags = LocalFlags( mem );
    ok( flags == 1, "LocalFlags returned %#x, error %lu\n", flags, GetLastError() );
    tmp_ptr = LocalLock( mem );
    ok( !!tmp_ptr, "LocalLock failed, error %lu\n", GetLastError() );
    ok( tmp_ptr == ptr, "got ptr %p, expected %p\n", tmp_ptr, ptr );
    flags = LocalFlags( mem );
    ok( flags == 2, "LocalFlags returned %#x, error %lu\n", flags, GetLastError() );
    ret = LocalUnlock( mem );
    ok( ret, "LocalUnlock failed, error %lu\n", GetLastError() );
    flags = LocalFlags( mem );
    ok( flags == 1, "LocalFlags returned %#x, error %lu\n", flags, GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_SUCCESS, "got error %lu\n", GetLastError() );
    flags = LocalFlags( mem );
    ok( !flags, "LocalFlags returned %#x, error %lu\n", flags, GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );
    tmp_mem = LocalFree( mem );
    ok( !tmp_mem, "LocalFree failed, error %lu\n", GetLastError() );

    /* freed handles are caught */
    mem = LocalAlloc( LMEM_MOVEABLE, 256 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = LocalFree( mem );
    ok( !tmp_mem, "LocalFree failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalFree( mem );
    ok( tmp_mem == mem, "LocalFree succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = LocalFlags( mem );
    ok( flags == LMEM_INVALID_HANDLE, "LocalFlags succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = LocalSize( mem );
    ok( size == 0, "LocalSize succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = LocalLock( mem );
    ok( !ptr, "LocalLock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
#if 0 /* corrupts Wine heap */
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalReAlloc( mem, 0, LMEM_MOVEABLE );
    todo_wine
    ok( !tmp_mem, "LocalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
#endif

    /* invalid handles are caught */
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalFree( invalid_mem );
    ok( tmp_mem == invalid_mem, "LocalFree succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = LocalFlags( invalid_mem );
    ok( flags == LMEM_INVALID_HANDLE, "LocalFlags succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = LocalSize( invalid_mem );
    ok( size == 0, "LocalSize succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = LocalLock( invalid_mem );
    ok( !ptr, "LocalLock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( invalid_mem );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalReAlloc( invalid_mem, 0, LMEM_MOVEABLE );
    ok( !tmp_mem, "LocalReAlloc succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );

    /* invalid pointers are caught */
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalFree( invalid_ptr );
    ok( tmp_mem == invalid_ptr, "LocalFree succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOACCESS, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    flags = LocalFlags( invalid_ptr );
    todo_wine
    ok( flags == LMEM_INVALID_HANDLE, "LocalFlags succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOACCESS, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = LocalSize( invalid_ptr );
    ok( size == 0, "LocalSize succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_INVALID_HANDLE, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = LocalLock( invalid_ptr );
    ok( !ptr, "LocalLock succeeded\n" );
    ok( GetLastError() == 0xdeadbeef, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( invalid_ptr );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalReAlloc( invalid_ptr, 0, LMEM_MOVEABLE );
    ok( !tmp_mem, "LocalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOACCESS, "got error %lu\n", GetLastError() );

    /* LMEM_FIXED block doesn't allow resize, though it succeeds with LMEM_MODIFY */
    mem = LocalAlloc( LMEM_FIXED, 10 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = LocalReAlloc( mem, 9, LMEM_MODIFY );
    todo_wine
    ok( !!tmp_mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem == mem, "got ptr %p, expected %p\n", tmp_mem, mem );
    size = LocalSize( mem );
    ok( size == 10, "LocalSize returned %Iu\n", size );
    SetLastError( 0xdeadbeef );
    tmp_mem = LocalReAlloc( mem, 10, 0 );
    todo_wine
    ok( !tmp_mem || broken( tmp_mem == mem ) /* w1064v1507 / w1064v1607 */,
        "LocalReAlloc succeeded\n" );
    todo_wine
    ok( GetLastError() == ERROR_NOT_ENOUGH_MEMORY || broken( GetLastError() == 0xdeadbeef ) /* w1064v1507 / w1064v1607 */,
        "got error %lu\n", GetLastError() );
    if (tmp_mem) mem = tmp_mem;
    tmp_mem = LocalReAlloc( mem, 1024 * 1024, LMEM_MODIFY );
    todo_wine
    ok( !!tmp_mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem == mem, "got ptr %p, expected %p\n", tmp_mem, mem );
    size = LocalSize( mem );
    ok( size == 10, "LocalSize returned %Iu\n", size );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    /* LMEM_FIXED block can be relocated with LMEM_MOVEABLE */
    mem = LocalAlloc( LMEM_FIXED, 10 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    tmp_mem = LocalReAlloc( mem, 11, LMEM_MOVEABLE );
    ok( !!tmp_mem, "LocalReAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem != mem || broken( tmp_mem == mem ) /* w1064v1507 / w1064v1607 */,
        "LocalReAlloc didn't relocate memory\n" );
    ptr = LocalLock( tmp_mem );
    ok( !!ptr, "LocalLock failed, error %lu\n", GetLastError() );
    ok( ptr == tmp_mem, "got ptr %p, expected %p\n", ptr, tmp_mem );
    LocalFree( tmp_mem );

    mem = LocalAlloc( LMEM_FIXED, 100 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded\n" );
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    mem = LocalAlloc( LMEM_FIXED, 0 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = LocalSize( mem );
    ok( size == 0, "LocalSize returned %Iu\n", size );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    /* trying to lock empty memory should give an error */
    mem = LocalAlloc( LMEM_MOVEABLE | LMEM_ZEROINIT, 0 );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ptr = LocalLock( mem );
    ok( !ptr, "LocalLock succeeded\n" );
    ok( GetLastError() == ERROR_DISCARDED, "got error %lu\n", GetLastError() );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    mem = LocalAlloc( 0, buffer_size );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size >= buffer_size, "LocalSize returned %Iu, error %lu\n", size, GetLastError() );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    mem = LocalAlloc( LMEM_ZEROINIT, buffer_size );
    ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
    size = LocalSize( mem );
    ok( size >= buffer_size, "LocalSize returned %Iu, error %lu\n", size, GetLastError() );
    ptr = LocalLock( mem );
    ok( !!ptr, "LocalLock failed, error %lu\n", GetLastError() );
    ok( ptr == mem, "got ptr %p, expected %p\n", ptr, mem );
    ok( !memcmp( ptr, zero_buffer, buffer_size ), "LocalAlloc didn't clear memory\n" );

    /* Check that we cannot change LMEM_FIXED to LMEM_MOVEABLE */
    mem = LocalReAlloc( mem, 0, LMEM_MODIFY | LMEM_MOVEABLE );
    ok( !!mem, "LocalReAlloc failed, error %lu\n", GetLastError() );
    todo_wine
    ok( mem == ptr, "LocalReAlloc returned unexpected handle\n" );
    size = LocalSize( mem );
    ok( size == buffer_size, "LocalSize returned %Iu, error %lu\n", size, GetLastError() );

    ptr = LocalLock( mem );
    ok( !!ptr, "LocalLock failed, error %lu\n", GetLastError() );
    todo_wine
    ok( ptr == mem, "got unexpected ptr %p\n", ptr );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded, error %lu\n", GetLastError() );
    todo_wine
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );

    tmp_mem = LocalReAlloc( mem, 2 * buffer_size, LMEM_MOVEABLE | LMEM_ZEROINIT );
    ok( !!tmp_mem, "LocalReAlloc failed\n" );
    ok( tmp_mem == mem || broken( tmp_mem != mem ) /* happens sometimes?? */,
        "LocalReAlloc returned unexpected handle\n" );
    mem = tmp_mem;

    size = LocalSize( mem );
    ok( size >= 2 * buffer_size, "LocalSize returned %Iu, error %lu\n", size, GetLastError() );
    ptr = LocalLock( mem );
    ok( !!ptr, "LocalLock failed, error %lu\n", GetLastError() );
    todo_wine
    ok( ptr == mem, "got unexpected ptr %p\n", ptr );
    ok( !memcmp( ptr, zero_buffer, buffer_size ), "LocalReAlloc didn't clear memory\n" );
    ok( !memcmp( ptr + buffer_size, zero_buffer, buffer_size ),
        "LocalReAlloc didn't clear memory\n" );

    tmp_mem = LocalHandle( ptr );
    ok( tmp_mem == mem, "LocalHandle returned unexpected handle\n" );
    tmp_mem = LocalDiscard( mem );
    todo_wine
    ok( !!tmp_mem, "LocalDiscard failed, error %lu\n", GetLastError() );
    todo_wine
    ok( tmp_mem == mem, "LocalDiscard returned unexpected handle\n" );
    ret = LocalUnlock( mem );
    ok( !ret, "LocalUnlock succeeded, error %lu\n", GetLastError() );
    todo_wine
    ok( GetLastError() == ERROR_NOT_LOCKED, "got error %lu\n", GetLastError() );

    tmp_mem = LocalDiscard( mem );
    ok( tmp_mem == mem, "LocalDiscard failed, error %lu\n", GetLastError() );
    mem = LocalFree( mem );
    ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );

    for (i = 0; i < ARRAY_SIZE(flags_tests); i++)
    {
        mem = LocalAlloc( flags_tests[i], 4 );
        ok( !!mem, "LocalAlloc failed, error %lu\n", GetLastError() );
        flags = LocalFlags( mem );
        ok( !(flags & ~LMEM_DISCARDABLE), "got flags %#x, error %lu\n", flags, GetLastError() );
        mem = LocalFree( mem );
        ok( !mem, "LocalFree failed, error %lu\n", GetLastError() );
    }
}

static void test_HeapQueryInformation(void)
{
    ULONG info;
    SIZE_T size;
    BOOL ret;

    if (!pHeapQueryInformation)
    {
        win_skip("HeapQueryInformation is not available\n");
        return;
    }

    if (0) /* crashes under XP */
    {
        size = 0;
        pHeapQueryInformation(0,
                                HeapCompatibilityInformation,
                                &info, sizeof(info), &size);
        size = 0;
        pHeapQueryInformation(GetProcessHeap(),
                                HeapCompatibilityInformation,
                                NULL, sizeof(info), &size);
    }

    size = 0;
    SetLastError(0xdeadbeef);
    ret = pHeapQueryInformation(GetProcessHeap(),
                                HeapCompatibilityInformation,
                                NULL, 0, &size);
    ok(!ret, "HeapQueryInformation should fail\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "expected ERROR_INSUFFICIENT_BUFFER got %lu\n", GetLastError());
    ok(size == sizeof(ULONG), "expected 4, got %Iu\n", size);

    SetLastError(0xdeadbeef);
    ret = pHeapQueryInformation(GetProcessHeap(),
                                HeapCompatibilityInformation,
                                NULL, 0, NULL);
    ok(!ret, "HeapQueryInformation should fail\n");
    ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "expected ERROR_INSUFFICIENT_BUFFER got %lu\n", GetLastError());

    info = 0xdeadbeaf;
    SetLastError(0xdeadbeef);
    ret = pHeapQueryInformation(GetProcessHeap(),
                                HeapCompatibilityInformation,
                                &info, sizeof(info) + 1, NULL);
    ok(ret, "HeapQueryInformation error %lu\n", GetLastError());
    ok(info == 0 || info == 1 || info == 2, "expected 0, 1 or 2, got %lu\n", info);
}

static void test_heap_checks( DWORD flags )
{
    BYTE old, *p, *p2;
    BOOL ret;
    SIZE_T i, size, large_size = 3000 * 1024 + 37;

    if (flags & HEAP_PAGE_ALLOCS) return;  /* no tests for that case yet */

    p = pHeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 17 );
    ok( p != NULL, "HeapAlloc failed\n" );

    ret = HeapValidate( GetProcessHeap(), 0, p );
    ok( ret, "HeapValidate failed\n" );

    size = HeapSize( GetProcessHeap(), 0, p );
    ok( size == 17, "Wrong size %Iu\n", size );

    ok( p[14] == 0, "wrong data %x\n", p[14] );
    ok( p[15] == 0, "wrong data %x\n", p[15] );
    ok( p[16] == 0, "wrong data %x\n", p[16] );

    if (flags & HEAP_TAIL_CHECKING_ENABLED)
    {
        ok( p[17] == 0xab, "wrong padding %x\n", p[17] );
        ok( p[18] == 0xab, "wrong padding %x\n", p[18] );
        ok( p[19] == 0xab, "wrong padding %x\n", p[19] );
    }

    p2 = HeapReAlloc( GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, p, 14 );
    if (p2 == p)
    {
        if (flags & HEAP_TAIL_CHECKING_ENABLED)
        {
            ok( p[14] == 0xab, "wrong padding %x\n", p[14] );
            ok( p[15] == 0xab, "wrong padding %x\n", p[15] );
            ok( p[16] == 0xab, "wrong padding %x\n", p[16] );
        }
        else
        {
            ok( p[14] == 0, "wrong padding %x\n", p[14] );
            ok( p[15] == 0, "wrong padding %x\n", p[15] );
        }
    }
    else skip( "realloc in place failed\n");

    ret = HeapFree( GetProcessHeap(), 0, p );
    ok( ret, "HeapFree failed\n" );

    p = pHeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 17 );
    ok( p != NULL, "HeapAlloc failed\n" );
    old = p[17];
    p[17] = 0xcc;

    if (flags & HEAP_TAIL_CHECKING_ENABLED)
    {
        ret = HeapValidate( GetProcessHeap(), 0, p );
        ok( !ret, "HeapValidate succeeded\n" );

        /* other calls only check when HEAP_VALIDATE is set */
        if (flags & HEAP_VALIDATE)
        {
            size = HeapSize( GetProcessHeap(), 0, p );
            ok( size == ~(SIZE_T)0 || broken(size == ~0u), "Wrong size %Iu\n", size );

            p2 = HeapReAlloc( GetProcessHeap(), 0, p, 14 );
            ok( p2 == NULL, "HeapReAlloc succeeded\n" );

            ret = HeapFree( GetProcessHeap(), 0, p );
            ok( !ret || broken(sizeof(void*) == 8), /* not caught on xp64 */
                "HeapFree succeeded\n" );
        }

        p[17] = old;
        size = HeapSize( GetProcessHeap(), 0, p );
        ok( size == 17, "Wrong size %Iu\n", size );

        p2 = HeapReAlloc( GetProcessHeap(), 0, p, 14 );
        ok( p2 != NULL, "HeapReAlloc failed\n" );
        p = p2;
    }

    ret = HeapFree( GetProcessHeap(), 0, p );
    ok( ret, "HeapFree failed\n" );

    p = HeapAlloc( GetProcessHeap(), 0, 37 );
    ok( p != NULL, "HeapAlloc failed\n" );
    memset( p, 0xcc, 37 );

    ret = HeapFree( GetProcessHeap(), 0, p );
    ok( ret, "HeapFree failed\n" );

    if (flags & HEAP_FREE_CHECKING_ENABLED)
    {
        ok( p[16] == 0xee, "wrong data %x\n", p[16] );
        ok( p[17] == 0xfe, "wrong data %x\n", p[17] );
        ok( p[18] == 0xee, "wrong data %x\n", p[18] );
        ok( p[19] == 0xfe, "wrong data %x\n", p[19] );

        ret = HeapValidate( GetProcessHeap(), 0, NULL );
        ok( ret, "HeapValidate failed\n" );

        old = p[16];
        p[16] = 0xcc;
        ret = HeapValidate( GetProcessHeap(), 0, NULL );
        ok( !ret, "HeapValidate succeeded\n" );

        p[16] = old;
        ret = HeapValidate( GetProcessHeap(), 0, NULL );
        ok( ret, "HeapValidate failed\n" );
    }

    /* now test large blocks */

    p = pHeapAlloc( GetProcessHeap(), 0, large_size );
    ok( p != NULL, "HeapAlloc failed\n" );

    ret = HeapValidate( GetProcessHeap(), 0, p );
    ok( ret, "HeapValidate failed\n" );

    size = HeapSize( GetProcessHeap(), 0, p );
    ok( size == large_size, "Wrong size %Iu\n", size );

    ok( p[large_size - 2] == 0, "wrong data %x\n", p[large_size - 2] );
    ok( p[large_size - 1] == 0, "wrong data %x\n", p[large_size - 1] );

    if (flags & HEAP_TAIL_CHECKING_ENABLED)
    {
        /* Windows doesn't do tail checking on large blocks */
        ok( p[large_size] == 0xab || broken(p[large_size] == 0), "wrong data %x\n", p[large_size] );
        ok( p[large_size+1] == 0xab || broken(p[large_size+1] == 0), "wrong data %x\n", p[large_size+1] );
        ok( p[large_size+2] == 0xab || broken(p[large_size+2] == 0), "wrong data %x\n", p[large_size+2] );
        if (p[large_size] == 0xab)
        {
            p[large_size] = 0xcc;
            ret = HeapValidate( GetProcessHeap(), 0, p );
            ok( !ret, "HeapValidate succeeded\n" );

            /* other calls only check when HEAP_VALIDATE is set */
            if (flags & HEAP_VALIDATE)
            {
                size = HeapSize( GetProcessHeap(), 0, p );
                ok( size == ~(SIZE_T)0, "Wrong size %Iu\n", size );

                p2 = HeapReAlloc( GetProcessHeap(), 0, p, large_size - 3 );
                ok( p2 == NULL, "HeapReAlloc succeeded\n" );

                ret = HeapFree( GetProcessHeap(), 0, p );
                ok( !ret, "HeapFree succeeded\n" );
            }
            p[large_size] = 0xab;
        }
    }

    ret = HeapFree( GetProcessHeap(), 0, p );
    ok( ret, "HeapFree failed\n" );

    /* test block sizes when tail checking */
    if (flags & HEAP_TAIL_CHECKING_ENABLED)
    {
        for (size = 0; size < 64; size++)
        {
            p = HeapAlloc( GetProcessHeap(), 0, size );
            for (i = 0; i < 32; i++) if (p[size + i] != 0xab) break;
            ok( i >= 8, "only %Iu tail bytes for size %Iu\n", i, size );
            HeapFree( GetProcessHeap(), 0, p );
        }
    }
}

static void test_debug_heap( const char *argv0, DWORD flags )
{
    char keyname[MAX_PATH];
    char buffer[MAX_PATH];
    PROCESS_INFORMATION	info;
    STARTUPINFOA startup;
    BOOL ret;
    DWORD err;
    HKEY hkey;
    const char *basename;

    if ((basename = strrchr( argv0, '\\' ))) basename++;
    else basename = argv0;

    sprintf( keyname, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",
             basename );
    if (!strcmp( keyname + strlen(keyname) - 3, ".so" )) keyname[strlen(keyname) - 3] = 0;

    err = RegCreateKeyA( HKEY_LOCAL_MACHINE, keyname, &hkey );
    if (err == ERROR_ACCESS_DENIED)
    {
        skip("Not authorized to change the image file execution options\n");
        return;
    }
    ok( !err, "failed to create '%s' error %lu\n", keyname, err );
    if (err) return;

    if (flags == 0xdeadbeef)  /* magic value for unsetting it */
        RegDeleteValueA( hkey, "GlobalFlag" );
    else
        RegSetValueExA( hkey, "GlobalFlag", 0, REG_DWORD, (BYTE *)&flags, sizeof(flags) );

    memset( &startup, 0, sizeof(startup) );
    startup.cb = sizeof(startup);

    sprintf( buffer, "%s heap.c 0x%lx", argv0, flags );
    ret = CreateProcessA( NULL, buffer, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info );
    ok( ret, "failed to create child process error %lu\n", GetLastError() );
    if (ret)
    {
        wait_child_process( info.hProcess );
        CloseHandle( info.hThread );
        CloseHandle( info.hProcess );
    }
    RegDeleteValueA( hkey, "GlobalFlag" );
    RegCloseKey( hkey );
    RegDeleteKeyA( HKEY_LOCAL_MACHINE, keyname );
}

static DWORD heap_flags_from_global_flag( DWORD flag )
{
    DWORD ret = 0;

    if (flag & FLG_HEAP_ENABLE_TAIL_CHECK)
        ret |= HEAP_TAIL_CHECKING_ENABLED;
    if (flag & FLG_HEAP_ENABLE_FREE_CHECK)
        ret |= HEAP_FREE_CHECKING_ENABLED;
    if (flag & FLG_HEAP_VALIDATE_PARAMETERS)
        ret |= HEAP_VALIDATE_PARAMS | HEAP_TAIL_CHECKING_ENABLED | HEAP_FREE_CHECKING_ENABLED;
    if (flag & FLG_HEAP_VALIDATE_ALL)
        ret |= HEAP_VALIDATE_ALL | HEAP_TAIL_CHECKING_ENABLED | HEAP_FREE_CHECKING_ENABLED;
    if (flag & FLG_HEAP_DISABLE_COALESCING)
        ret |= HEAP_DISABLE_COALESCE_ON_FREE;
    if (flag & FLG_HEAP_PAGE_ALLOCS)
        ret |= HEAP_PAGE_ALLOCS;
    return ret;
}

static void test_heap_layout( HANDLE handle, DWORD global_flag, DWORD heap_flags )
{
    DWORD force_flags = heap_flags & ~(HEAP_SHARED|HEAP_DISABLE_COALESCE_ON_FREE);
    struct heap *heap = handle;

    if (global_flag & FLG_HEAP_ENABLE_TAGGING) heap_flags |= HEAP_SHARED;
    if (!(global_flag & FLG_HEAP_PAGE_ALLOCS)) force_flags &= ~(HEAP_GROWABLE|HEAP_PRIVATE);

    todo_wine_if( force_flags & (HEAP_PRIVATE|HEAP_NO_SERIALIZE) )
    ok( heap->force_flags == force_flags, "got force_flags %#x\n", heap->force_flags );
    todo_wine_if( heap_flags & (HEAP_VALIDATE_ALL|HEAP_VALIDATE_PARAMS|HEAP_SHARED|HEAP_PRIVATE) )
    ok( heap->flags == heap_flags, "got flags %#x\n", heap->flags );

    if (heap->flags & HEAP_PAGE_ALLOCS)
    {
        struct heap expect_heap;
        memset( &expect_heap, 0xee, sizeof(expect_heap) );
        expect_heap.force_flags = heap->force_flags;
        expect_heap.flags = heap->flags;
        todo_wine
        ok( !memcmp( heap, &expect_heap, sizeof(expect_heap) ), "got unexpected data\n" );
    }
    else
    {
        todo_wine
        ok( heap->ffeeffee == 0xffeeffee, "got ffeeffee %#x\n", heap->ffeeffee );
        ok( heap->auto_flags == (heap_flags & HEAP_GROWABLE) || !heap->auto_flags,
            "got auto_flags %#x\n", heap->auto_flags );
    }
}

static void test_child_heap( const char *arg )
{
    char buffer[32];
    DWORD global_flags = strtoul( arg, 0, 16 ), type, size = sizeof(buffer);
    DWORD heap_flags;
    HANDLE heap;
    HKEY hkey;
    BOOL ret;

    if (global_flags == 0xdeadbeef)  /* global_flags value comes from Session Manager global flags */
    {
        ret = RegOpenKeyA( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager", &hkey );
        if (!ret)
        {
            skip( "Session Manager flags not set\n" );
            return;
        }

        ret = RegQueryValueExA( hkey, "GlobalFlag", 0, &type, (BYTE *)buffer, &size );
        ok( ret, "RegQueryValueExA failed, error %lu\n", GetLastError() );

        if (type == REG_DWORD) global_flags = *(DWORD *)buffer;
        else if (type == REG_SZ) global_flags = strtoul( buffer, 0, 16 );

        ret = RegCloseKey( hkey );
        ok( ret, "RegCloseKey failed, error %lu\n", GetLastError() );
    }
    if (global_flags && !pRtlGetNtGlobalFlags())  /* not working on NT4 */
    {
        win_skip( "global flags not set\n" );
        return;
    }

    heap_flags = heap_flags_from_global_flag( global_flags );
    trace( "testing global flags %#lx, heap flags %08lx\n", global_flags, heap_flags );

    ok( pRtlGetNtGlobalFlags() == global_flags, "got global flags %#lx\n", pRtlGetNtGlobalFlags() );

    test_heap_layout( GetProcessHeap(), global_flags, heap_flags|HEAP_GROWABLE );

    heap = HeapCreate( 0, 0, 0 );
    ok( heap != GetProcessHeap(), "got unexpected heap\n" );
    test_heap_layout( heap, global_flags, heap_flags|HEAP_GROWABLE|HEAP_PRIVATE );
    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );

    heap = HeapCreate( HEAP_NO_SERIALIZE, 0, 0 );
    ok( heap != GetProcessHeap(), "got unexpected heap\n" );
    test_heap_layout( heap, global_flags, heap_flags|HEAP_NO_SERIALIZE|HEAP_GROWABLE|HEAP_PRIVATE );
    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );

    heap = HeapCreate( 0, 0x1000, 0x10000 );
    ok( heap != GetProcessHeap(), "got unexpected heap\n" );
    test_heap_layout( heap, global_flags, heap_flags|HEAP_PRIVATE );
    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );

    heap = HeapCreate( HEAP_SHARED, 0, 0 );
    ok( heap != GetProcessHeap(), "got unexpected heap\n" );
    test_heap_layout( heap, global_flags, heap_flags|HEAP_GROWABLE|HEAP_PRIVATE );
    ret = HeapDestroy( heap );
    ok( ret, "HeapDestroy failed, error %lu\n", GetLastError() );

    test_heap_checks( heap_flags );
}

static void test_GetPhysicallyInstalledSystemMemory(void)
{
    MEMORYSTATUSEX memstatus;
    ULONGLONG total_memory;
    BOOL ret;

    if (!pGetPhysicallyInstalledSystemMemory)
    {
        win_skip("GetPhysicallyInstalledSystemMemory is not available\n");
        return;
    }

    SetLastError(0xdeadbeef);
    ret = pGetPhysicallyInstalledSystemMemory(NULL);
    ok(!ret, "GetPhysicallyInstalledSystemMemory should fail\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "expected ERROR_INVALID_PARAMETER, got %lu\n", GetLastError());

    total_memory = 0;
    ret = pGetPhysicallyInstalledSystemMemory(&total_memory);
    ok(ret, "GetPhysicallyInstalledSystemMemory unexpectedly failed (%lu)\n", GetLastError());
    ok(total_memory != 0, "expected total_memory != 0\n");

    memstatus.dwLength = sizeof(memstatus);
    ret = GlobalMemoryStatusEx(&memstatus);
    ok(ret, "GlobalMemoryStatusEx unexpectedly failed\n");
    ok(total_memory >= memstatus.ullTotalPhys / 1024,
       "expected total_memory >= memstatus.ullTotalPhys / 1024\n");
}

static void test_GlobalMemoryStatus(void)
{
    char buffer[sizeof(SYSTEM_PERFORMANCE_INFORMATION) + 16]; /* some Win 7 versions need a larger info */
    SYSTEM_PERFORMANCE_INFORMATION *perf_info = (void *)buffer;
    SYSTEM_BASIC_INFORMATION basic_info;
    MEMORYSTATUSEX memex = {0}, expect;
    MEMORYSTATUS mem = {0};
    VM_COUNTERS_EX vmc;
    NTSTATUS status;
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = GlobalMemoryStatusEx( &memex );
    ok( !ret, "GlobalMemoryStatusEx succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "got error %lu\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    GlobalMemoryStatus( &mem );
    ok( GetLastError() == 0xdeadbeef, "got error %lu\n", GetLastError() );

    status = NtQuerySystemInformation( SystemBasicInformation, &basic_info, sizeof(basic_info), NULL );
    ok( !status, "NtQuerySystemInformation returned %#lx\n", status );
    status = NtQuerySystemInformation( SystemPerformanceInformation, perf_info, sizeof(buffer), NULL );
    ok( !status, "NtQuerySystemInformation returned %#lx\n", status );
    status = NtQueryInformationProcess( GetCurrentProcess(), ProcessVmCounters, &vmc, sizeof(vmc), NULL );
    ok( !status, "NtQueryInformationProcess returned %#lx\n", status );
    mem.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus( &mem );
    memex.dwLength = sizeof(MEMORYSTATUSEX);
    ret = GlobalMemoryStatusEx( &memex );
    ok( ret, "GlobalMemoryStatusEx succeeded\n" );

    ok( basic_info.PageSize, "got 0 PageSize\n" );
    ok( basic_info.MmNumberOfPhysicalPages, "got 0 MmNumberOfPhysicalPages\n" );
    ok( !!basic_info.HighestUserAddress, "got 0 HighestUserAddress\n" );
    ok( !!basic_info.LowestUserAddress, "got 0 LowestUserAddress\n" );
    ok( perf_info->TotalCommittedPages, "got 0 TotalCommittedPages\n" );
    ok( perf_info->TotalCommitLimit, "got 0 TotalCommitLimit\n" );
    ok( perf_info->AvailablePages, "got 0 AvailablePages\n" );

    expect.dwMemoryLoad = (memex.ullTotalPhys - memex.ullAvailPhys) / (memex.ullTotalPhys / 100);
    expect.ullTotalPhys = (ULONGLONG)basic_info.MmNumberOfPhysicalPages * basic_info.PageSize;
    expect.ullAvailPhys = (ULONGLONG)perf_info->AvailablePages * basic_info.PageSize;
    expect.ullTotalPageFile = (ULONGLONG)perf_info->TotalCommitLimit * basic_info.PageSize;
    expect.ullAvailPageFile = (ULONGLONG)(perf_info->TotalCommitLimit - perf_info->TotalCommittedPages) * basic_info.PageSize;
    expect.ullTotalVirtual = (ULONG_PTR)basic_info.HighestUserAddress - (ULONG_PTR)basic_info.LowestUserAddress + 1;
    expect.ullAvailVirtual = expect.ullTotalVirtual - (ULONGLONG)vmc.WorkingSetSize /* approximate */;
    expect.ullAvailExtendedVirtual = 0;

/* allow some variability, info sources are not always in sync */
#define IS_WITHIN_RANGE(a, b) (((a) - (b) + (256 * basic_info.PageSize)) <= (512 * basic_info.PageSize))

    ok( memex.dwMemoryLoad == expect.dwMemoryLoad, "got dwMemoryLoad %lu\n", memex.dwMemoryLoad );
    ok( memex.ullTotalPhys == expect.ullTotalPhys, "got ullTotalPhys %#I64x\n", memex.ullTotalPhys );
    ok( IS_WITHIN_RANGE( memex.ullAvailPhys, expect.ullAvailPhys ), "got ullAvailPhys %#I64x\n", memex.ullAvailPhys );
    ok( memex.ullTotalPageFile == expect.ullTotalPageFile, "got ullTotalPageFile %#I64x\n", memex.ullTotalPageFile );
    ok( IS_WITHIN_RANGE( memex.ullAvailPageFile, expect.ullAvailPageFile ), "got ullAvailPageFile %#I64x\n", memex.ullAvailPageFile );
    ok( memex.ullTotalVirtual == expect.ullTotalVirtual, "got ullTotalVirtual %#I64x\n", memex.ullTotalVirtual );
    ok( memex.ullAvailVirtual <= expect.ullAvailVirtual, "got ullAvailVirtual %#I64x\n", memex.ullAvailVirtual );
    ok( memex.ullAvailExtendedVirtual == 0, "got ullAvailExtendedVirtual %#I64x\n", memex.ullAvailExtendedVirtual );

    ok( mem.dwMemoryLoad == memex.dwMemoryLoad, "got dwMemoryLoad %lu\n", mem.dwMemoryLoad );
    ok( mem.dwTotalPhys == min( ~(SIZE_T)0 >> 1, memex.ullTotalPhys ) ||
        broken( mem.dwTotalPhys == ~(SIZE_T)0 ) /* Win <= 8.1 with RAM size > 4GB */,
        "got dwTotalPhys %#Ix\n", mem.dwTotalPhys );
    ok( IS_WITHIN_RANGE( mem.dwAvailPhys, min( ~(SIZE_T)0 >> 1, memex.ullAvailPhys ) ) ||
        broken( mem.dwAvailPhys == ~(SIZE_T)0 ) /* Win <= 8.1 with RAM size > 4GB */,
        "got dwAvailPhys %#Ix\n", mem.dwAvailPhys );
#ifndef _WIN64
    todo_wine_if(memex.ullTotalPageFile > 0xfff7ffff)
#endif
    ok( mem.dwTotalPageFile == min( ~(SIZE_T)0, memex.ullTotalPageFile ), "got dwTotalPageFile %#Ix\n", mem.dwTotalPageFile );
    ok( IS_WITHIN_RANGE( mem.dwAvailPageFile, min( ~(SIZE_T)0, memex.ullAvailPageFile ) ), "got dwAvailPageFile %#Ix\n", mem.dwAvailPageFile );
    ok( mem.dwTotalVirtual == memex.ullTotalVirtual, "got dwTotalVirtual %#Ix\n", mem.dwTotalVirtual );
    ok( mem.dwAvailVirtual == memex.ullAvailVirtual, "got dwAvailVirtual %#Ix\n", mem.dwAvailVirtual );

#undef IS_WITHIN_RANGE
}

START_TEST(heap)
{
    int argc;
    char **argv;

    load_functions();

    argc = winetest_get_mainargs( &argv );
    if (argc >= 3)
    {
        test_child_heap( argv[2] );
        return;
    }

    test_HeapCreate();
    test_GlobalAlloc();
    test_LocalAlloc();

    test_HeapQueryInformation();
    test_GetPhysicallyInstalledSystemMemory();
    test_GlobalMemoryStatus();

    if (pRtlGetNtGlobalFlags)
    {
        test_debug_heap( argv[0], 0 );
        test_debug_heap( argv[0], FLG_HEAP_ENABLE_TAIL_CHECK );
        test_debug_heap( argv[0], FLG_HEAP_ENABLE_FREE_CHECK );
        test_debug_heap( argv[0], FLG_HEAP_VALIDATE_PARAMETERS );
        test_debug_heap( argv[0], FLG_HEAP_VALIDATE_ALL );
        test_debug_heap( argv[0], FLG_POOL_ENABLE_TAGGING );
        test_debug_heap( argv[0], FLG_HEAP_ENABLE_TAGGING );
        test_debug_heap( argv[0], FLG_HEAP_ENABLE_TAG_BY_DLL );
        test_debug_heap( argv[0], FLG_HEAP_DISABLE_COALESCING );
        test_debug_heap( argv[0], FLG_HEAP_PAGE_ALLOCS );
        test_debug_heap( argv[0], 0xdeadbeef );
    }
    else win_skip( "RtlGetNtGlobalFlags not found, skipping heap debug tests\n" );
}
