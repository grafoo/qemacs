#!/bin/sh

mkdir -p patches

case "$1" in
    new)
        git format-patch -o patches --to=qemacs-devel@nongnu.org -1 "$2"
        git send-email patches/$(ls -lhrt patches | tail -n 1 | perl -lane 'print $F[-1]')
        ;;
    use)
        git send-email "$2"
        ;;
    *)
        echo "Usage: ${0} new <commit> | use <patch-filepath>"
        ;;
esac
