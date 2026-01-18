#!/usr/bin/env python3
"""
Script untuk mengkonversi icon PNG aplikasi desktop menjadi array C
Icon akan disimpan di kernel/app_icons_data.c
"""
import os
from PIL import Image

# Konfigurasi
APP_ICON_SIZE = 48  # Sesuai dengan ICON_SIZE di desktop.c
ICON_DIR = "app_icons"
OUTPUT_FILE = "kernel/app_icons_data.c"
HEADER_FILE = "include/app_icons.h"
TRANSPARENT_MARKER = "0xFF000000"

# Daftar aplikasi dan file icon mereka
APP_ICONS = {
    "TERMINAL": "terminal.png",
    "EDITOR": "editor.png", 
    "EXPLORER": "explorer.png",
    "FLOPPYBIRD": "floppybird.png"
}

def convert_app_icons():
    """Konversi semua icon aplikasi menjadi array C"""
    
    # Buat direktori app_icons jika belum ada
    if not os.path.exists(ICON_DIR):
        os.makedirs(ICON_DIR)
        print(f"Direktori {ICON_DIR} dibuat. Silakan tambahkan file icon PNG.")
        return False
    
    # Generate header file
    with open(HEADER_FILE, "w") as f:
        f.write("#ifndef APP_ICONS_H\n")
        f.write("#define APP_ICONS_H\n\n")
        f.write(f"#define APP_ICON_SIZE {APP_ICON_SIZE}\n")
        f.write(f"#define APP_ICON_COUNT {len(APP_ICONS)}\n\n")
        
        # Enum untuk icon IDs
        f.write("enum {\n")
        for i, name in enumerate(APP_ICONS.keys()):
            f.write(f"    APP_ICON_{name} = {i},\n")
        f.write("};\n\n")
        
        f.write(f"extern unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE];\n\n")
        f.write("#endif\n")
    
    print(f"✓ Header file: {HEADER_FILE}")
    
    # Generate C file dengan data icon
    with open(OUTPUT_FILE, "w") as f:
        f.write('#include "app_icons.h"\n\n')
        f.write(f'unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE] = {{\n')
        
        for app_name, filename in APP_ICONS.items():
            icon_path = os.path.join(ICON_DIR, filename)
            
            # Ukuran icon sebenarnya (lebih kecil dari canvas 48x48)
            ICON_CONTENT_SIZE = 32  # Icon akan 32x32, dengan padding 8px di setiap sisi
            
            # Skip jika file tidak ada
            if not os.path.exists(icon_path):
                print(f"✗ Error: {filename} tidak ditemukan, skip...")
                continue
            
            # Load dan resize icon ke ukuran content
            img_original = Image.open(icon_path).convert("RGBA")
            img_content = img_original.resize((ICON_CONTENT_SIZE, ICON_CONTENT_SIZE), Image.Resampling.LANCZOS)
            
            # Buat canvas transparan 48x48
            img = Image.new("RGBA", (APP_ICON_SIZE, APP_ICON_SIZE), (0, 0, 0, 0))
            
            # Paste icon di tengah canvas (offset 8px dari setiap sisi)
            offset = (APP_ICON_SIZE - ICON_CONTENT_SIZE) // 2
            img.paste(img_content, (offset, offset))
            
            pixels = img.load()
            
            f.write(f'    [APP_ICON_{app_name}] = {{ // {filename}\n        ')
            
            # Konversi pixel ke format RGBA
            for y in range(APP_ICON_SIZE):
                for x in range(APP_ICON_SIZE):
                    r, g, b, a = pixels[x, y]
                    
                    # Gunakan threshold alpha untuk transparansi
                    if a < 128:
                        f.write(f"{TRANSPARENT_MARKER}, ")
                    else:
                        # Format: 0x00RRGGBB (alpha channel diabaikan, handled terpisah)
                        f.write(f"0x00{r:02X}{g:02X}{b:02X}, ")
                
                if y < APP_ICON_SIZE - 1:
                    f.write("\n        ")
            
            f.write('\n    },\n')
        
        f.write('};\n')
    
    print(f"✓ Data file: {OUTPUT_FILE}")
    print(f"\n✓ Konversi selesai! {len(APP_ICONS)} icon aplikasi berhasil dikonversi.")
    return True

if __name__ == "__main__":
    print("=" * 60)
    print("Konversi Icon Aplikasi Desktop ke Array C")
    print("=" * 60)
    convert_app_icons()
    print("\nCara penggunaan:")
    print("1. Letakkan file PNG icon (48x48 px) di folder 'app_icons/'")
    print("2. Jalankan: python3 convert_icons.py")
    print("3. Compile ulang dengan: make clean && make")