import numpy as np

# Define the image dimensions and bit depth
width, height, bpp = 160, 120, 16

# Create an empty numpy array with the correct shape and data type
# The shape is (height, width, 3) for the RGB channels
# The data type is np.uint16 for 16 bits per pixel
image = np.zeros((height, width, 3), dtype=np.uint16)

# Define the color bar
color_bar = np.array([
    [255, 0, 0],  # Red
    [0, 255, 0],  # Green
    [0, 0, 255],  # Blue
    [255, 255, 0],  # Yellow
    [0, 255, 255],  # Cyan
    [255, 0, 255],  # Magenta
    [255, 255, 255],  # White
    [0, 0, 0],  # Black
])

# Fill the image with the color bar
for i in range(8):
    image[:, i*width//8:(i+1)*width//8] = color_bar[i]

# Convert the image to RGB565 format
r = (image[..., 0] * 31 / 255).astype(np.uint16)
g = (image[..., 1] * 63 / 255).astype(np.uint16)
b = (image[..., 2] * 31 / 255).astype(np.uint16)
rgb565 = (r << 11) | (g << 5) | b

# Save the image data to a binary file
rgb565.tofile('output_ok.bin')
