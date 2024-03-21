import numpy as np
from PIL import Image

def hexdump_to_binary(hexdump_file, binary_file):
    with open(hexdump_file, 'r') as f:
        hexdump = f.read().replace('\n', '')

    binary_data = bytes.fromhex(hexdump)

    with open(binary_file, 'wb') as f:
        f.write(binary_data)

# Define input and output files
input_file = 'data_1.log'
output_file = 'output_mcu_1.bin'

# Usage
hexdump_to_binary(input_file, output_file)

# # Define width and height
w, h = 160, 120

# Use dtype=np.uint16 because the image is 16bpp
d = np.fromfile(output_file, dtype=np.uint16, count=w*h)

# # Convert RGB565 to RGB888
# r = ((d & 0xF800) >> 11) / 31.0
# g = ((d & 0x07E0) >> 5) / 63.0
# b = (d & 0x001F) / 31.0

# Convert BGR565 to RGB888
b = ((d & 0xF800) >> 11) / 31.0
g = ((d & 0x07E0) >> 5) / 63.0
r = (d & 0x001F) / 31.0

# Reshape the data into an image
rgb = np.zeros((h, w, 3))
rgb[..., 0] = r.reshape(h, w)
rgb[..., 1] = g.reshape(h, w)
rgb[..., 2] = b.reshape(h, w)

# Make into PIL Image and save
PILimage = Image.fromarray(np.uint8(rgb*255))
PILimage.save('result.png')
