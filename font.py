#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

# --- FONT CONFIGURATION ---
FONT_DIR = "fonts"
FONT_FILE = "font.ttf"
HEADER_OUT = "include/character.h"
SOURCE_OUT = "kernel/character.c"

# Bitmap dimensions
CHARACTER_WIDTH = 10
CHARACTER_HEIGHT = 18
FONT_SIZE = 12
CHARACTER_NUMBER = 96
START_CHAR = 0x20

def render_char_to_bitmap(font, char):
    # Larger canvas to prevent clipping during initial render
    canvas_size = 32
    img = Image.new('L', (canvas_size, canvas_size), color=255)
    draw = ImageDraw.Draw(img)
    
    try:
        # Get glyph dimensions
        bbox = draw.textbbox((0, 0), char, font=font)
        text_width = bbox[2] - bbox[0]
        text_height = bbox[3] - bbox[1]
        
        # Center glyph within the target cell
        x = (CHARACTER_WIDTH - text_width) // 2 - bbox[0]
        y = (CHARACTER_HEIGHT - text_height) // 2 - bbox[1]
        
        draw.text((x, y), char, font=font, fill=0)
    except:
        pass
    
    # Crop to exact kernel specifications
    img = img.crop((0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT))
    pixels = img.load()
    
    bitmap = []
    for y in range(CHARACTER_HEIGHT):
        row = [255 - pixels[x, y] for x in range(CHARACTER_WIDTH)]
        bitmap.append(row)
    return bitmap

def generate_files(font_path):
    if not os.path.exists(font_path):
        return False
    
    try:
        font = ImageFont.truetype(font_path, FONT_SIZE)
    except:
        return False
    
    # 1. Write Header File
    with open(HEADER_OUT, 'w') as f:
        f.write('#ifndef CHARACTER_H\n#define CHARACTER_H\n\n')
        f.write(f'#define CHARACTER_WIDTH {CHARACTER_WIDTH}\n')
        f.write(f'#define CHARACTER_HEIGHT {CHARACTER_HEIGHT}\n')
        f.write(f'#define CHARACTER_NUMBER {CHARACTER_NUMBER}\n\n')
        f.write('extern unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH];\n\n')
        f.write('#endif\n')
    
    # 2. Write Source File
    with open(SOURCE_OUT, 'w') as f:
        f.write('#include "character.h"\n\n')
        f.write(f'unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH] = {{\n')
        
        for i in range(CHARACTER_NUMBER - 1):
            char = chr(START_CHAR + i)
            
            # Escape labels for C comments
            label = char
            if char == '\\': label = "Backslash"
            elif char == ' ': label = "Space"
            elif char == '*': label = "Asterisk"
            
            bitmap = render_char_to_bitmap(font, char)
            f.write(f'  {{ // Index {i}: {label}\n')
            for y in range(CHARACTER_HEIGHT):
                f.write('    {')
                f.write(', '.join(map(str, bitmap[y])))
                f.write('}' + (',' if y < CHARACTER_HEIGHT - 1 else '') + '\n')
            f.write('  }' + (',' if i < CHARACTER_NUMBER - 2 else '') + '\n')
        f.write('};\n')
    
    return True

if __name__ == "__main__":
    path = os.path.join(FONT_DIR, FONT_FILE)
    if generate_files(path):
        print(f"Update successful: {CHARACTER_WIDTH}x{CHARACTER_HEIGHT}")