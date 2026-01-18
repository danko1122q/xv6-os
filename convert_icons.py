#!/usr/bin/env python3
"""
Script to convert desktop application PNG icons to C arrays
Icons will be saved in kernel/app_icons_data.c
"""
import os
from PIL import Image

# Configuration
APP_ICON_SIZE = 48  # Matches ICON_SIZE in desktop.c
ICON_DIR = "app_icons"
OUTPUT_FILE = "kernel/app_icons_data.c"
HEADER_FILE = "include/app_icons.h"
TRANSPARENT_MARKER = "0xFF000000"

# List of applications and their icon files
APP_ICONS = {
    "TERMINAL": "terminal.png",
    "EDITOR": "editor.png", 
    "EXPLORER": "explorer.png",
    "FLOPPYBIRD": "floppybird.png"
}

def convert_app_icons():
    """Convert all application icons to C arrays"""
    
    # Create app_icons directory if it doesn't exist
    if not os.path.exists(ICON_DIR):
        os.makedirs(ICON_DIR)
        print(f"Directory {ICON_DIR} created. Please add PNG icon files.")
        return False
    
    # Generate header file
    with open(HEADER_FILE, "w") as f:
        f.write("#ifndef APP_ICONS_H\n")
        f.write("#define APP_ICONS_H\n\n")
        f.write(f"#define APP_ICON_SIZE {APP_ICON_SIZE}\n")
        f.write(f"#define APP_ICON_COUNT {len(APP_ICONS)}\n\n")
        
        # Enum for icon IDs
        f.write("enum {\n")
        for i, name in enumerate(APP_ICONS.keys()):
            f.write(f"    APP_ICON_{name} = {i},\n")
        f.write("};\n\n")
        
        f.write(f"extern unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE];\n\n")
        f.write("#endif\n")
    
    # Generate C file with icon data
    with open(OUTPUT_FILE, "w") as f:
        f.write('#include "app_icons.h"\n\n')
        f.write(f'unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE] = {{\n')
        
        for app_name, filename in APP_ICONS.items():
            icon_path = os.path.join(ICON_DIR, filename)
            
            # Actual icon size (smaller than 48x48 canvas)
            ICON_CONTENT_SIZE = 32  # Icon will be 32x32, with 8px padding on each side
            
            # Skip if file doesn't exist
            if not os.path.exists(icon_path):
                print(f"Error: {filename} not found, skipping...")
                continue
            
            # Load and resize icon to content size
            img_original = Image.open(icon_path).convert("RGBA")
            img_content = img_original.resize((ICON_CONTENT_SIZE, ICON_CONTENT_SIZE), Image.Resampling.LANCZOS)
            
            # Create transparent 48x48 canvas
            img = Image.new("RGBA", (APP_ICON_SIZE, APP_ICON_SIZE), (0, 0, 0, 0))
            
            # Paste icon in center of canvas (8px offset from each side)
            offset = (APP_ICON_SIZE - ICON_CONTENT_SIZE) // 2
            img.paste(img_content, (offset, offset))
            
            pixels = img.load()
            
            f.write(f'    [APP_ICON_{app_name}] = {{ // {filename}\n        ')
            
            # Convert pixels to RGBA format
            for y in range(APP_ICON_SIZE):
                for x in range(APP_ICON_SIZE):
                    r, g, b, a = pixels[x, y]
                    
                    # Use alpha threshold for transparency
                    if a < 128:
                        f.write(f"{TRANSPARENT_MARKER}, ")
                    else:
                        # Format: 0x00RRGGBB (alpha channel ignored, handled separately)
                        f.write(f"0x00{r:02X}{g:02X}{b:02X}, ")
                
                if y < APP_ICON_SIZE - 1:
                    f.write("\n        ")
            
            f.write('\n    },\n')
        
        f.write('};\n')
    
    print(f"Converted {len(APP_ICONS)} icons successfully.")
    return True

if __name__ == "__main__":
    convert_app_icons()
