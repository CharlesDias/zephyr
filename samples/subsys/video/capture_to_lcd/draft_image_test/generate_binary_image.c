#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Define the image dimensions
#define WIDTH 160
#define HEIGHT 120

// Define the color bar
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define YELLOW (RED | GREEN)
#define CYAN (GREEN | BLUE)
#define MAGENTA (RED | BLUE)
#define WHITE (RED | GREEN | BLUE)
#define BLACK 0x0000

uint16_t color_bar[8] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK};

void generate_color_bar_image(uint16_t *image_data) {
	// Check if the memory allocation was successful
	if (image_data == NULL) {
		printf("Failed to allocate memory for image data\n");
		return;
	}

	// Fill the array with the color bar
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			int color_index = (x * 8) / WIDTH;
			image_data[y * WIDTH + x] = color_bar[color_index];
		}
	}
}

int main() {
	// Allocate memory for the image data
	uint16_t *image_data = malloc(WIDTH * HEIGHT * sizeof(uint16_t));

	// Generate the color bar image
	generate_color_bar_image(image_data);

	// Open the output file
	FILE *file = fopen("output_ok.bin", "wb");
	if (file == NULL) {
		printf("Failed to open output file\n");
		free(image_data);
		return -1;
	}

	// Write the image data to the file
	fwrite(image_data, sizeof(uint16_t), WIDTH * HEIGHT, file);

	// Close the file
	fclose(file);

	// Free the memory allocated for the image data
	free(image_data);

	return 0;
}
