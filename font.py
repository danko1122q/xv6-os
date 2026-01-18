#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

# ==================== CONFIGURATION ====================
FONT_DIR = "fonts"
FONT_FILE = "font.ttf"
OUTPUT_FILE = "include/character.h"

# Grid tetap 8x16 agar kompatibel dengan user_gui.c
CHARACTER_WIDTH = 8
CHARACTER_HEIGHT = 16
# FONT_SIZE diturunkan ke 12 agar teks tidak terlalu memenuhi kotak (tidak terlalu gede)
FONT_SIZE = 12 

CHARACTER_NUMBER = 96
START_CHAR = 0x20
# =======================================================

def render_char_to_bitmap(font, char):
    # Gunakan padding yang cukup agar huruf memiliki ruang napas
    padding = 4
    img_width = CHARACTER_WIDTH + padding * 2
    img_height = CHARACTER_HEIGHT + padding * 2
    
    img = Image.new('L', (img_width, img_height), color=255)
    draw = ImageDraw.Draw(img)
    
    try:
        bbox = draw.textbbox((0, 0), char, font=font)
        text_width = bbox[2] - bbox[0]
        text_height = bbox[3] - bbox[1]
        
        # Penempatan di tengah secara horizontal, agak ke bawah secara vertical
        x = (img_width - text_width) // 2 - bbox[0]
        y = (img_height - text_height) // 2 - bbox[1] + 1 
        
        draw.text((x, y), char, font=font, fill=0)
    except:
        pass
    
    img = img.crop((padding, padding, padding + CHARACTER_WIDTH, padding + CHARACTER_HEIGHT))
    pixels = img.load()
    
    bitmap = []
    for y in range(CHARACTER_HEIGHT):
        row = []
        for x in range(CHARACTER_WIDTH):
            # Threshold 145 memberikan ketebalan yang pas untuk DejaVu Sans Mono
            pixel_value = 1 if pixels[x, y] < 145 else 0
            row.append(pixel_value)
        bitmap.append(row)
    return bitmap

def generate_character_header(font_path):
    if not os.path.exists(font_path):
        print(f"Error: Font tidak ditemukan di {font_path}")
        return False
    
    font = ImageFont.truetype(font_path, FONT_SIZE)
    
    # Generate include/character.h
    with open(OUTPUT_FILE, 'w') as f:
        f.write('#ifndef CHARACTER_H\n#define CHARACTER_H\n\n')
        f.write(f'#define CHARACTER_WIDTH {CHARACTER_WIDTH}\n')
        f.write(f'#define CHARACTER_HEIGHT {CHARACTER_HEIGHT}\n')
        f.write(f'#define CHARACTER_NUMBER {CHARACTER_NUMBER}\n\n')
        f.write('extern unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH];\n\n')
        f.write('#endif\n')
    
    # Generate include/character.c
    c_file = OUTPUT_FILE.replace('.h', '.c')
    with open(c_file, 'w') as f:
        f.write('#include "character.h"\n\n')
        f.write('unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH] = {\n')
        
        for i in range(CHARACTER_NUMBER - 1):
            char = chr(START_CHAR + i)
            
            # Pengamanan karakter backslash untuk menghindari error compiler
            label = char
            if char == '\\': label = "backslash"
            elif char == ' ': label = "space"
            
            bitmap = render_char_to_bitmap(font, char)
            f.write(f'  {{ // {label}\n')
            for y in range(CHARACTER_HEIGHT):
                f.write('    {')
                f.write(', '.join(map(str, bitmap[y])))
                f.write('}' + (',' if y < CHARACTER_HEIGHT - 1 else '') + '\n')
            f.write('  }' + (',' if i < CHARACTER_NUMBER - 2 else '') + '\n')
        f.write('};\n')
    
    print(f"✓ Selesai: {OUTPUT_FILE} (Ukuran Font: {FONT_SIZE} pada Grid: {CHARACTER_WIDTH}x{CHARACTER_HEIGHT})")
    return True

if __name__ == "__main__":
    font_path = os.path.join(FONT_DIR, FONT_FILE)
    generate_character_header(font_path)