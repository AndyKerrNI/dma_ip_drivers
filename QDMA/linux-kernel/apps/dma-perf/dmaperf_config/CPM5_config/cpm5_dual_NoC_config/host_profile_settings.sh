#!/bin/bash

# Usage: ./script.sh <qdma_device>
# Example: ./script.sh qdma71000

if [ -z "$1" ]; then
    echo "Usage: $0 <qdma_device>"
    echo "Example: $0 qdma71000"
    exit 1
fi

QDMA_DEV="$1"

/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x2c8 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x308 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x824 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x828 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x82C 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x830 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x834 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x838 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x83C 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x840 0xffffffff
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x804 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x808 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x80C 0x00000000
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x810 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x814 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x818 0x00000000
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x81C 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x820 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x844 0x34
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x2c8 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x308 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x804 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x808 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x80C 0x40000000
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x810 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x814 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x818 0x00040000
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x81C 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x820 0x0
/usr/local/sbin/dma-ctl "$QDMA_DEV" reg write bar 0 0x844 0xb4
