#!/bin/bash 

if [ ! -d "$1" ]; then
    echo "Must specify a larva directory to copy from, and directory must exist" 
    exit
fi

if [ ! -d "$2" ]; then
    echo "Must specify a tadpole directory to copy to, and directory must exist" 
    exit
fi

LARVA_DIR=$1
TADPOLE_DIR=$2

declare -a DIRS=("/hw/mcu/native" "/hw/bsp/native" "/hw/hal" "/libs/os" "/compiler/sim") 
for dir in "${DIRS[@]}"
do
    echo "Copying $dir"
    find "$LARVA_DIR/$dir" -type d -name "bin" -prune -exec rm -rf {} \;
    find "$LARVA_DIR/$dir" -type d -name "obj" -prune -exec rm -rf {} \;

    rm -rf "$TADPOLE_DIR/$dir"
    cp -rf "$LARVA_DIR/$dir" "$TADPOLE_DIR/$dir"
done

