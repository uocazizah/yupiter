#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

#
# yupiter - prove that the built libraries are freestanding
#
# Usage:
#
#     sh test/check-freestanding.sh <libyupiter.a> [<libyupiter.so>]
#
# A static archive is never linked on its own, so a reference to the C
# library or to the C++ runtime inside one of its members could stay
# invisible until a program happens to pull that member in.  Inspecting
# the archive directly is the only way to prove the property for every
# member.
#
# What is verified for the archive:
#
#   1. Every undefined symbol is "main", the linker-defined
#      "_GLOBAL_OFFSET_TABLE_", or a "yupiter_"-prefixed symbol of the
#      library itself.  "__cxa_*", "_Unwind_*", "operator new",
#      "memset", "memcpy" or a compiler helper such as "__udivti3" is a
#      dependence the archive must not have.
#
#   2. No exported symbol carries a C++ mangled name ("_Z..."), and no
#      exported symbol is outside the C interface: the only allowed
#      names are the public "yupiter_*" declarations and the entry
#      point "_start".  The shared object is held to the smaller list
#      of documented declarations: internal helpers may be global in
#      the archive, where they link one member to another, but they are
#      hidden from the shared object.
#
#   3. No member has a section that exists only to run code before or
#      after the program: ".init_array", ".fini_array", ".preinit_array",
#      ".ctors", ".dtors", ".tdata" or ".tbss".  The freestanding entry
#      point walks no initialiser array.
#
# The shared object is checked the same way, plus: it must have no
# undefined dynamic symbols and no NEEDED entry, because it is meant to
# be loadable without a C library or a C++ runtime.

set -u

if [ $# -lt 1 ] || [ $# -gt 2 ]
then
    echo "usage: check-freestanding.sh <archive> [<shared-object>]" >&2
    exit 1
fi

archive=$1
shared=${2:-}

# Binutils expands @response files even after --.  Give every relative
# artifact an explicit path before checking or passing it to a tool.
case "$archive" in
    /*) ;;
    *) archive=./$archive ;;
esac
case "$shared" in
    ''|/*) ;;
    *) shared=./$shared ;;
esac

if [ ! -f "$archive" ]
then
    echo "check-freestanding: \"$archive\" does not exist" >&2
    exit 1
fi

nm=${NM:-nm}
readelf=${READELF:-readelf}

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT INT TERM

status=0

# --- Undefined symbols of the archive ---------------------------------

if ! "$nm" --format=posix --extern-only --undefined-only -- "$archive" \
        > "$tmpdir/undefined" 2> "$tmpdir/nmerr"
then
    echo "check-freestanding: nm failed on \"$archive\":" >&2
    cat "$tmpdir/nmerr" >&2
    exit 1
fi

while IFS= read -r line
do
    # Archive member headers end with a colon and are not symbols.
    case "$line" in
        *:)
            continue
            ;;
    esac

    # POSIX format: "name U" for an undefined symbol.
    case "$line" in
        *" U"*)
            ;;
        *)
            continue
            ;;
    esac

    name=${line%% *}

    # "_GLOBAL_OFFSET_TABLE_" is synthesized by the linker for
    # position-independent code that reads a default-visible object
    # through the GOT.  It is not provided by a C library or a runtime.
    case "$name" in
        main|yupiter_*|_GLOBAL_OFFSET_TABLE_)
            ;;
        *)
            echo "check-freestanding: \"$archive\" refers to \"$name\"" >&2
            echo "  the library must not need a C library, a C++ runtime" >&2
            echo "  or a compiler runtime helper" >&2
            status=1
            ;;
    esac
done < "$tmpdir/undefined"

# --- Exported symbols of the archive ----------------------------------

if ! "$nm" --format=posix --extern-only --defined-only -- "$archive" \
        > "$tmpdir/defined" 2> "$tmpdir/nmerr"
then
    echo "check-freestanding: nm failed on \"$archive\":" >&2
    cat "$tmpdir/nmerr" >&2
    exit 1
fi

while IFS= read -r line
do
    case "$line" in
        *:)
            continue
            ;;
    esac

    # POSIX format: "name TYPE value [size]".
    set -- $line

    [ $# -ge 2 ] || continue

    name=$1
    type=$2

    case "$type" in
        T|D|B|R|W|V|C|G|S)
            ;;
        *)
            continue
            ;;
    esac

    case "$name" in
        _start|yupiter_*)
            ;;
        _Z*)
            echo "check-freestanding: \"$archive\" exports the mangled" >&2
            echo "  C++ name \"$name\"" >&2
            status=1
            ;;
        *)
            echo "check-freestanding: \"$archive\" exports \"$name\"," >&2
            echo "  which is outside the C interface" >&2
            status=1
            ;;
    esac
done < "$tmpdir/defined"

# --- Sections that run code outside the program -----------------------

for section in .init_array .fini_array .preinit_array .ctors .dtors \
               .tdata .tbss
do
    if "$readelf" --section-headers --wide -- "$archive" |
       grep -q -- "$section"
    then
        echo "check-freestanding: \"$archive\" has a \"$section\" section;" >&2
        echo "  the freestanding entry point would never run it" >&2
        status=1
    fi
done

# --- The shared object, when one was built ----------------------------

if [ -n "$shared" ] && [ -f "$shared" ]
then
    if ! "$nm" --format=posix -D --undefined-only -- "$shared" \
            > "$tmpdir/dynundef" 2> "$tmpdir/nmerr"
    then
        echo "check-freestanding: nm failed on \"$shared\":" >&2
        cat "$tmpdir/nmerr" >&2
        exit 1
    fi

    if grep -q ' U ' "$tmpdir/dynundef"
    then
        echo "check-freestanding: \"$shared\" has undefined dynamic" >&2
        echo "  symbols:" >&2
        grep ' U ' "$tmpdir/dynundef" >&2
        status=1
    fi

    if ! "$nm" --format=posix -D --defined-only -- "$shared" \
            > "$tmpdir/dyndefined" 2> "$tmpdir/nmerr"
    then
        echo "check-freestanding: nm failed on \"$shared\":" >&2
        cat "$tmpdir/nmerr" >&2
        exit 1
    fi

    while IFS= read -r line
    do
        set -- $line

        [ $# -ge 2 ] || continue

        name=$1
        type=$2

        case "$type" in
            T|D|B|R|W|V|C|G|S)
                ;;
            *)
                continue
                ;;
        esac

        case "$name" in
            yupiter_printf|yupiter_fprintf|yupiter_getchar|yupiter_fgetc|\
            yupiter_fgets|yupiter_fread|yupiter_ungetc|yupiter_feof|\
            yupiter_ferror|yupiter_clearerr|yupiter_fflush|yupiter_malloc|\
            yupiter_free|yupiter_malloc_safe|yupiter_free_safe|\
            yupiter_realloc|yupiter_realloc_safe|yupiter_free_checked|\
            yupiter_realloc_checked|yupiter_stdout|yupiter_stderr|\
            yupiter_stdin)
                ;;
            *)
                echo "check-freestanding: \"$shared\" exports \"$name\"," >&2
                echo "  which is not part of the public interface" >&2
                status=1
                ;;
        esac
    done < "$tmpdir/dyndefined"

    if "$readelf" -d -- "$shared" > "$tmpdir/dynamic"
    then
        if grep -q 'NEEDED' "$tmpdir/dynamic"
        then
            echo "check-freestanding: \"$shared\" names a shared library" >&2
            echo "  it needs:" >&2
            grep 'NEEDED' "$tmpdir/dynamic" >&2
            status=1
        fi
    else
        echo "check-freestanding: readelf failed on \"$shared\"" >&2
        exit 1
    fi

    for section in .init_array .fini_array .preinit_array .ctors .dtors \
                   .tdata .tbss
    do
        if "$readelf" --section-headers --wide -- "$shared" |
           grep -q -- "$section"
        then
            echo "check-freestanding: \"$shared\" has a \"$section\"" >&2
            echo "  section; the freestanding startup path runs none" >&2
            status=1
        fi
    done
fi

if [ "$status" -eq 0 ]
then
    echo "check-freestanding: \"$archive\"${shared:+ and \"$shared\"} are" \
         "freestanding"
fi

exit "$status"
