#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
import os

FONT_DIR = "fonts"
FONT_FILE = "font.ttf"
OUTPUT_FILE = "include/character.h"

# Medium Dimension Configuration (10x14)
CHARACTER_WIDTH = 10
CHARACTER_HEIGHT = 14
FONT_SIZE = 11
CHARACTER_WIDTH = 9
CHARACTER_HEIGHT = 16
FONT_SIZE = 12
>>>>>>> af4e151e51145a9acdfcc780909cfb1f74a99fca
CHARACTER_NUMBER = 96
START_CHAR = 0x20

def render_char_to_bitmap(font, char):
    padding = 4
    img_width = CHARACTER_WIDTH + padding * 2
    img_height = CHARACTER_HEIGHT + padding * 2
    
    img = Image.new('L', (img_width, img_height), color=255)
    draw = ImageDraw.Draw(img)
    
    try:
        bbox = draw.textbbox((0, 0), char, font=font)
        text_width = bbox[2] - bbox[0]
        text_height = bbox[3] - bbox[1]
        
        # Center the character
        x = (img_width - text_width) // 2 - bbox[0]
        y = (img_height - text_height) // 2 - bbox[1]
        draw.text((x, y), char, font=font, fill=0)
    except:
        pass
    
    img = img.crop((padding, padding, padding + CHARACTER_WIDTH, padding + CHARACTER_HEIGHT))
    pixels = img.load()
    
    bitmap = []
    for y in range(CHARACTER_HEIGHT):
        row = []
        for x in range(CHARACTER_WIDTH):
            # Invert values: 0 for background, >0 for font intensity
            pixel_value = 255 - pixels[x, y]
            row.append(pixel_value)
        bitmap.append(row)
    return bitmap

def generate_character_header(font_path):
    if not os.path.exists(font_path):
        print(f"Error: Font {font_path} not found")
        return False
    
    font = ImageFont.truetype(font_path, FONT_SIZE)
    
    # 1. Generate .h file
    with open(OUTPUT_FILE, 'w') as f:
        f.write('#ifndef CHARACTER_H\n#define CHARACTER_H\n\n')
        f.write(f'#define CHARACTER_WIDTH {CHARACTER_WIDTH}\n')
        f.write(f'#define CHARACTER_HEIGHT {CHARACTER_HEIGHT}\n')
        f.write(f'#define CHARACTER_NUMBER {CHARACTER_NUMBER}\n\n')
        f.write('extern unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH];\n\n')
        f.write('#endif\n')
    
    # 2. Generate .c file
    c_file = OUTPUT_FILE.replace('.h', '.c')
    with open(c_file, 'w') as f:
        f.write('#include "character.h"\n\n')
        f.write('unsigned char character[CHARACTER_NUMBER - 1][CHARACTER_HEIGHT][CHARACTER_WIDTH] = {\n')
        
        for i in range(CHARACTER_NUMBER - 1):
            char = chr(START_CHAR + i)
            
            # FIX: Avoid "multi-line comment" errors by renaming problematic characters in labels
            label = char
            if char == '\\': label = "Backslash"
            elif char == ' ': label = "Space"
            elif char == '*': label = "Asterisk" # Avoid accidental */ sequence
            
            bitmap = render_char_to_bitmap(font, char)
            f.write(f'  {{ // Index {i}: {label}\n')
            for y in range(CHARACTER_HEIGHT):
                f.write('    {')
                f.write(', '.join(map(str, bitmap[y])))
                f.write('}' + (',' if y < CHARACTER_HEIGHT - 1 else '') + '\n')
            f.write('  }' + (',' if i < CHARACTER_NUMBER - 2 else '') + '\n')
        f.write('};\n')
    
    print(f"Done! New dimensions: {CHARACTER_WIDTH}x{CHARACTER_HEIGHT}")
    return True

if __name__ == "__main__":
    font_path = os.path.join(FONT_DIR, FONT_FILE)
    generate_character_header(font_path)