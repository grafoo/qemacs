#!/bin/sh
git cvsimport \
    -v -k -u \
    -d :pserver:anonymous@cvs.savannah.nongnu.org:/sources/qemacs \
    -A cvs-author-conversion \
    -r cvs \
    qemacs
