#!/usr/bin/env python3
import os
from PIL import Image

APP_ICON_SIZE = 48
ICON_DIR = "app_icons"
OUTPUT_FILE = "kernel/app_icons_data.c"
HEADER_FILE = "include/app_icons.h"
TRANSPARENT_MARKER = "0xFF000000"

APP_ICONS = {
    "TERMINAL": "terminal.png",
    "EDITOR": "editor.png", 
    "EXPLORER": "explorer.png",
    "FLOPPYBIRD": "floppybird.png"
}

def convert_app_icons():
    if not os.path.exists(ICON_DIR):
        os.makedirs(ICON_DIR)
        print(f"Created {ICON_DIR}/ directory. Add PNG icons here.")
        return False
    
    # Generate header
    with open(HEADER_FILE, "w") as f:
        f.write("#ifndef APP_ICONS_H\n")
        f.write("#define APP_ICONS_H\n\n")
        f.write(f"#define APP_ICON_SIZE {APP_ICON_SIZE}\n")
        f.write(f"#define APP_ICON_COUNT {len(APP_ICONS)}\n\n")
        f.write("enum {\n")
        for i, name in enumerate(APP_ICONS.keys()):
            f.write(f"    APP_ICON_{name} = {i},\n")
        f.write("};\n\n")
        f.write(f"extern unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE];\n\n")
        f.write("#endif\n")
    
    # Generate C data file
    with open(OUTPUT_FILE, "w") as f:
        f.write('#include "app_icons.h"\n\n')
        f.write(f'unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE] = {{\n')
        
        for app_name, filename in APP_ICONS.items():
            icon_path = os.path.join(ICON_DIR, filename)
            ICON_CONTENT_SIZE = 32
            
            if not os.path.exists(icon_path):
                print(f"Warning: {filename} not found, skipping...")
                continue
            
            img_original = Image.open(icon_path).convert("RGBA")
            img_content = img_original.resize((ICON_CONTENT_SIZE, ICON_CONTENT_SIZE), Image.Resampling.LANCZOS)
            img = Image.new("RGBA", (APP_ICON_SIZE, APP_ICON_SIZE), (0, 0, 0, 0))
            offset = (APP_ICON_SIZE - ICON_CONTENT_SIZE) // 2
            img.paste(img_content, (offset, offset))
            pixels = img.load()
            
            f.write(f'    [APP_ICON_{app_name}] = {{\n        ')
            
            for y in range(APP_ICON_SIZE):
                for x in range(APP_ICON_SIZE):
                    r, g, b, a = pixels[x, y]
                    if a < 128:
                        f.write(f"{TRANSPARENT_MARKER}, ")
                    else:
                        f.write(f"0x00{r:02X}{g:02X}{b:02X}, ")
                if y < APP_ICON_SIZE - 1:
                    f.write("\n        ")
            f.write('\n    },\n')
        f.write('};\n')
    
    print(f"Generated: {HEADER_FILE} and {OUTPUT_FILE}")
    print(f"Converted {len(APP_ICONS)} icons successfully.")
    return True

if __name__ == "__main__":
    print("Converting desktop icons to C arrays...")
    convert_app_icons()
    print("Done. Run 'make' to rebuild.")