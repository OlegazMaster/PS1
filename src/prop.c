#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* The properties of the entities (and other items with properties). */

/* A property of an item (an entity, a camera path...). */
typedef struct {
    s16 tag;
    u16 offset; /* 0x2: of its data, from item + 0xC */
    u8 flags; /* 0x4: type (bits 0..4), 0x20: has keys, 0x40: a count per row */
    u8 size; /* 0x5: of a value */
    u16 rows; /* 0x6 */
} Prop;
#define LERP(a, b, f) (((((b) - (a)) * (f) >> 7) + 1 >> 1) + (a))
u8 *func_80032710(u8 *item, s32 tag, s32 pos, s32 index, s32 *outcount, s32 mode, u8 *buf, s32 count,
                  Prop **hdr);
#ifdef NON_MATCHING
/* Looks up property tag of item (or of the items chained after it, at +8):
 * the row of key pos >> 8 (mode 0: the one at or before it, 1: exactly it,
 * 2: row pos) from value index, or (mode 3) the count values from index
 * interpolated between the rows around pos, put in buf. outcount gets the
 * values in the row, hdr the property. NULL if not found.
 * Equivalent; the original is laid out more compactly. */
u8 *func_80032710(u8 *item, s32 tag, s32 pos, s32 index, s32 *outcount, s32 mode, u8 *buf, s32 count,
                  Prop **hdr) {
    Prop *props;
    Prop *prop;
    s32 lo;
    s32 hi;
    s32 mid;
    u8 *base;
    u16 *counts;
    s16 *keys;
    u8 *data;
    u8 *next;
    u8 *dst;
    s32 rows;
    s32 n;
    s32 row;
    s32 key;
    s32 off;
    s32 i;
    s32 f;
    s32 k0;
    s32 type;

    do {
        hi = *(s16 *)(item + 0xC);
        base = item + 0xC;
        if (hi > 0) {
            hi--;
            lo = 0;
            props = (Prop *)(item + 0x10);
            for (;;) {
                mid = (lo + hi) >> 1;
                prop = &props[mid];
                if (prop->tag == tag) {
                    goto found;
                }
                if (prop->tag < tag) {
                    lo = mid + 1;
                } else {
                    hi = mid - 1;
                }
                if (hi < lo) {
                    break;
                }
            }
        }
        item = *(u8 **)(item + 8);
    } while (item != NULL);
    return NULL;
found:
    if (hdr != NULL) {
        *hdr = prop;
    }
    rows = prop->rows;
    counts = (u16 *)(base + prop->offset);
    if (prop->flags & 0x40) {
        keys = (s16 *)(counts + rows);
    } else {
        keys = (s16 *)(counts + 1);
        n = *counts;
        counts = NULL;
    }
    if (prop->flags & 0x20) {
        data = (u8 *)(keys + rows);
    } else {
        data = (u8 *)keys;
        keys = NULL;
    }
    data = (u8 *)(((s32)data + 3) & ~3);
    if (mode == 2) {
        if (pos >= rows) {
            return NULL;
        }
        row = pos;
        goto exact;
    }
    if (keys == NULL) {
        return NULL;
    }
    key = pos >> 8;
    if (key < keys[0]) {
        return NULL;
    }
    hi = prop->rows - 1;
    if (key >= keys[hi]) {
        row = hi;
    } else {
        lo = 0;
        for (;;) {
            mid = (lo + hi) >> 1;
            if (key < keys[mid]) {
                hi = mid;
            } else if (key < keys[mid + 1]) {
                row = mid;
                break;
            } else {
                lo = mid + 1;
            }
        }
    }
    if (mode == 0) {
        goto exact;
    }
    if (mode == 1) {
        if (keys[row] == key) {
            goto exact;
        }
        return NULL;
    }
    if (counts != NULL) {
        n = counts[row];
    }
    if (count == 0) {
        count = n;
    }
    if (n < index + count) {
        return NULL;
    }
    if (outcount != NULL) {
        *outcount = n;
    }
    off = index;
    if (row > 0) {
        if (counts != NULL) {
            for (i = 0, off = 0; i < row; i++) {
                off += *counts++;
            }
            off += index;
        } else {
            off = n * row + index;
        }
    }
    if (off != 0) {
        data += off * prop->size;
    }
    dst = buf;
    if (row == prop->rows - 1 || (counts != NULL && counts[1] != n)) {
        goto copy;
    }
    next = data + n * prop->size;
    k0 = keys[row] << 8;
    f = (keys[row + 1] << 8) - k0;
    pos -= k0;
    if (pos == 0) {
        goto copy;
    }
    f = (pos << 8) / f;
    type = prop->flags & 0x1F;
    switch (type) {
    case 1:
    case 0x11:
        while (count-- != 0) {
            if (type == 1) {
                *dst = LERP(*data, *next, f);
            } else {
                *dst = LERP(*(s8 *)data, *(s8 *)next, f);
            }
            data++;
            next++;
            dst++;
        }
        break;
    case 2:
    case 0x12:
        while (count-- != 0) {
            if (type == 2) {
                *(u16 *)dst = LERP(*(u16 *)data, *(u16 *)next, f);
            } else {
                *(s16 *)dst = LERP(*(s16 *)data, *(s16 *)next, f);
            }
            data += 2;
            next += 2;
            dst += 2;
        }
        break;
    case 3:
    case 4:
    case 5:
    case 0x13:
        while (count-- != 0) {
            *(s32 *)dst = LERP(*(s32 *)data, *(s32 *)next, f);
            data += 4;
            next += 4;
            dst += 4;
        }
        break;
    case 6:
        while (count-- != 0) {
            ((s16 *)dst)[0] = LERP(((s16 *)data)[0], ((s16 *)next)[0], f);
            ((s16 *)dst)[1] = LERP(((s16 *)data)[1], ((s16 *)next)[1], f);
            ((s16 *)dst)[2] = LERP(((s16 *)data)[2], ((s16 *)next)[2], f);
            data += 6;
            next += 6;
            dst += 6;
        }
        break;
    case 7:
        while (count-- != 0) {
            ((s32 *)dst)[0] = LERP(((s32 *)data)[0], ((s32 *)next)[0], f);
            ((s32 *)dst)[1] = LERP(((s32 *)data)[1], ((s32 *)next)[1], f);
            ((s32 *)dst)[2] = LERP(((s32 *)data)[2], ((s32 *)next)[2], f);
            data += 12;
            next += 12;
            dst += 12;
        }
        break;
    }
    return buf;
copy:
    for (i = count * prop->size; i-- != 0;) {
        *dst++ = *data++;
    }
    return buf;
exact:
    if (counts != NULL) {
        n = counts[row];
    }
    if (index >= n) {
        return NULL;
    }
    if (outcount != NULL) {
        *outcount = n;
    }
    off = index;
    if (row > 0) {
        if (counts != NULL) {
            for (i = 0, off = 0; i < row; i++) {
                off += *counts++;
            }
            off += index;
        } else {
            off = n * row + index;
        }
    }
    if (off != 0) {
        data += off * prop->size;
    }
    return data;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/prop", func_80032710);
#endif

/* The value of property b of the item a (see func_80032710), or def if it has
 * none. */

s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 def) {
    u8 buf[16];
    Prop *hdr;
    u8 *v;

    v = func_80032710((u8 *)a, b, c, d, (s32 *)e, f, buf, 1, &hdr);
    if (v == NULL) {
        return def;
    }
    switch (hdr->flags & 0x1F) {
    case 1:
        return *(u8 *)v;
    case 0x11:
        return *(s8 *)v;
    case 2:
        return *(u16 *)v;
    case 0x12:
        return *(s16 *)v;
    default:
        return *(s32 *)v;
    }
}

/* The count of values of property b of a (0 if none). */

s32 func_80032EDC(s32 a, s32 b, s32 c, s32 f) {
    u8 buf[16];
    s32 count;

    if (func_80032710((u8 *)a, b, c, 0, &count, f, buf, 1, NULL) == NULL) {
        return 0;
    }
    return count;
}
