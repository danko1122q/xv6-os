import os
from PIL import Image

ICON_SIZE = 30
ICON_NUMBER = 5
ICON_NAMES = ["ICON_CLOSE", "ICON_MINIMIZE", "ICON_WINDOWS", "ICON_FOLDER", "ICON_CHESS"]
ICON_DIR = "icon"
ICON_FILES = {
    "ICON_CLOSE": "close.png",
    "ICON_MINIMIZE": "minus.png",
    "ICON_WINDOWS": "logo.png",
    "ICON_FOLDER": "logo.png",
    "ICON_CHESS": "logo.png"
}
OUTPUT_FILE = "kernel/icons_data.c"
TRANSPARENT_MARKER = "0xFF000000"

def convert():
    if not os.path.exists(ICON_DIR):
        os.makedirs(ICON_DIR)
        return

    with open(OUTPUT_FILE, "w") as f:
        f.write('#include "icons.h"\n\n')
        f.write(f'unsigned int icons_data[ICON_NUMBER][ICON_SIZE * ICON_SIZE] = {{\n')
        
        for name in ICON_NAMES:
            file_name = ICON_FILES.get(name)
            path = os.path.join(ICON_DIR, file_name)
            
            if not os.path.exists(path):
                path = os.path.join(ICON_DIR, "logo.png")
            
            # Load original image
            img_original = Image.open(path).convert("RGBA")
            
            # Determine icon content size (Close & Minimize are smaller)
            if name in ["ICON_CLOSE", "ICON_MINIMIZE"]:
                content_size = 20
            else:
                content_size = 30
            
            # Resize icon content
            img_resized = img_original.resize((content_size, content_size), Image.Resampling.LANCZOS)
            
            # Create transparent 30x30 canvas
            canvas = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
            offset = (ICON_SIZE - content_size) // 2
            canvas.paste(img_resized, (offset, offset))
            
            pixels = canvas.load()
            f.write(f'    [{name}] = {{\n        ')
            for y in range(ICON_SIZE):
                for x in range(ICON_SIZE):
                    r, g, b, a = pixels[x, y]
                    # Alpha threshold for transparency
                    if a < 150: 
                        f.write(f"{TRANSPARENT_MARKER}, ")
                    else:
                        f.write(f"0x00{r:02X}{g:02X}{b:02X}, ")
                f.write("\n        ")
            f.write('\n    },\n')
        
        f.write('};\n')
    
    print("Icon conversion completed.")

if __name__ == "__main__":
    convert()