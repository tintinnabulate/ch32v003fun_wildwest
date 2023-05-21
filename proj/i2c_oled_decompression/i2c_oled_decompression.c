/*
 * Example for using I2C with 128x32 graphic OLED
 * 03-29-2023 E. Brombaugh
 */

// Could be defined here, or in the processor defines.
#define SYSTEM_CORE_CLOCK 48000000
#define APB_CLOCK SYSTEM_CORE_CLOCK

// what type of OLED - uncomment just one
//#define SSD1306_64X32
#define SSD1306_128X32
//#define SSD1306_128X64

#include "../../ch32v003fun/ch32v003fun.h"
#include <stdio.h>
#include "ssd1306_i2c.h"
#include "ssd1306.h"

#define MALLOC_OVERRIDE 0

#if MALLOC_OVERRIDE > 0
#include <stdlib.h>
#endif

#define COMP_PACKBITS 0
#define COMP_HEATSHRINK 1
#if (COMP_PACKBITS == 1) && (COMP_HEATSHRINK == 1)
#error "please only enable packbits OR heatshrink"
#endif
#if (COMP_PACKBITS == 0) && (COMP_HEATSHRINK == 0)
#error "please enable packbits or heatshrink"
#endif

#include "bomb_i_packed.h"
#include "bomb_i_heatshrunk.h"
#include "rocket_i_packed.h"
#include "rocket_i_heatshrunk.h"

#include "compression.h"

#include "../../examples/systick_irq_millis/systick.h"

#define STDOUT_UART
#define LOGimage 0



void draw_image(uint8_t* input, uint8_t width, uint8_t height, uint8_t x, uint8_t y, uint8_t color_mode) {
	uint8_t x_absolute;
	uint8_t y_absolute;
	uint8_t pixel;
	uint8_t bytes_to_draw = width / 8;
	uint16_t buffer_addr;

	for (uint8_t line = 0; line < height; line++) {
		y_absolute = y + line;
		if (y_absolute >= SSD1306_H) {
			break;
		}

		// SSD1306 is in vertical mode, yet we want to draw horizontally, which necessitates assembling the output bytes from the input data
		// bitmask for current pixel in vertical (output) byte
		uint8_t v_mask = 1 << (y_absolute & 7);

		for (uint8_t byte = 0; byte < bytes_to_draw; byte++) {
			uint8_t input_byte = input[byte + line * bytes_to_draw];

			for (pixel = 0; pixel < 8; pixel++) {
				x_absolute = x + 8 * (bytes_to_draw - byte) + pixel;
				if (x_absolute >= SSD1306_W) {
					break;
				}
				// looking at the horizontal display, we're drawing bytes bottom to top, not left to right, hence y / 8
				buffer_addr = x_absolute + SSD1306_W * (y_absolute / 8);
				// state of current pixel
				uint8_t input_pixel = input_byte & (1 << pixel);

				switch (color_mode) {
					case 0:
						// write pixels as they are
						ssd1306_buffer[buffer_addr] = (ssd1306_buffer[buffer_addr] & ~v_mask) | (input_pixel ? v_mask : 0);
						break;
					case 1:
						// write pixels after inversion
						ssd1306_buffer[buffer_addr] = (ssd1306_buffer[buffer_addr] & ~v_mask) | (!input_pixel ? v_mask : 0);
						break;
					case 2:
						// 0 clears pixel
						ssd1306_buffer[buffer_addr] &= input_pixel ? 0xFF : ~v_mask;
						break;
					case 3:
						// 1 sets pixel
						ssd1306_buffer[buffer_addr] |= input_pixel ? v_mask : 0;
						break;
					case 4:
						// 0 sets pixel
						ssd1306_buffer[buffer_addr] |= !input_pixel ? v_mask : 0;
						break;
					case 5:
						// 1 clears pixel
						ssd1306_buffer[buffer_addr] &= input_pixel ? ~v_mask : 0xFF;
						break;
				}
			}
			#if LOGimage == 1
			printf("%02x ", input_byte);
			#endif
		}
		#if LOGimage == 1
		printf("\n\r");
		#endif
	}
}



void unpack_image(uint8_t* input, uint16_t size_of_input, uint8_t width, uint8_t height, uint8_t x, uint8_t y, uint8_t color_mode) {
	uint16_t output_max_size = (width / 8) * height;
	#if MALLOC_OVERRIDE > 0
	uint8_t* output = malloc(output_max_size);
	#else
	uint8_t output[output_max_size];
	#endif

	#if COMP_PACKBITS == 1
	uint16_t unpack_error = decompress_packbits(input, size_of_input, output, &output_max_size);
	#elif COMP_HEATSHRINK == 1
	uint16_t unpack_error = decompress_heatshrink(input, size_of_input, output, &output_max_size);
	#endif
	#if LOGimage == 1
	printf("unpack return %u\n\r", unpack_error);
	printf("unpack size IN %u OUT %u\n\r", size_of_input, output_max_size);
	#endif
	draw_image(output, width, height, x, y, color_mode);
	#if MALLOC_OVERRIDE > 0
	free(output);
	#endif
}



void unpack_image_number(uint8_t* input, uint16_t size_of_input, uint8_t width, uint8_t height, uint8_t image_number, uint8_t x, uint8_t y, uint8_t color_mode) {
	uint16_t output_max_size = (width / 8) * height;
	#if MALLOC_OVERRIDE > 0
	uint8_t* output = malloc(output_max_size);
	#else
	uint8_t output[output_max_size];
	uint16_t start_byte_offset = output_max_size * image_number;
	#endif
	#if COMP_PACKBITS == 1
	uint16_t unpack_error = decompress_packbits_window(input, size_of_input, output, start_byte_offset, &output_max_size);
	#elif COMP_HEATSHRINK == 1
	uint16_t unpack_error = decompress_heatshrink_window(input, size_of_input, output, start_byte_offset, &output_max_size);
	#endif
	#if LOGimage == 1
	printf("unpack return %u\n\r", unpack_error);
	printf("unpack size IN %u OUT %u\n\r", size_of_input, output_max_size);
	#endif
	draw_image(output, width, height, x, y, color_mode);
	#if MALLOC_OVERRIDE > 0
	free(output);
	#endif
}



int main()
{
	// 48MHz internal clock
	SystemInit48HSI();
	systick_init();

	// start serial @ default 115200bps
#ifdef STDOUT_UART
	SetupUART( UART_BRR );
	systick_delay_ms( 100 );
#else
	SetupDebugPrintf();
#endif
	printf("\r\r\n\ni2c_oled example\n\r");

	// init i2c and oled
	systick_delay_ms( 100 );	// give OLED some more time
	printf("initializing i2c oled...");
	if(!ssd1306_i2c_init())
	{
		ssd1306_init();
		printf("done.\n\r");
		
		printf("Looping on test modes...");
		while(1)
		{
			for(uint8_t mode=0;mode<3;mode++)
			{
				// clear buffer for next mode
				ssd1306_setbuf(0);

				switch(mode)
				{
					case 0:
						printf("buffer fill with binary\n\r");
						for(int i=0;i<sizeof(ssd1306_buffer);i++)
							ssd1306_buffer[i] = i;
						break;
					case 1:
						printf("draw decompressed bomb\n\r");
						#if COMP_PACKBITS == 1
						printf("packbits\n\r");
						unpack_image(bomb_i_packed, bomb_i_packed_len, 32, 32, 16, 0, 0);
						#elif COMP_HEATSHRINK == 1
						printf("heatshrink\n\r");
						unpack_image(bomb_i_heatshrunk, bomb_i_heatshrunk_len, 32, 32, 16, 0, 0);
						#endif
						break;
					case 2:
						printf("launch a compressed rocket\n\r");
						const uint16_t frame_i = 33;
						uint32_t frame_t = 0;
						uint32_t decomp_t = 0;
						for (uint8_t loop = 0; loop < 6; loop++) {
							for (uint8_t image = 0;image < 26; image++) {
								while (millis() - frame_t < frame_i) {};
								//ssd1306_setbuf(0);
								frame_t = millis();		// missappropriate as stopwatch
								#if COMP_PACKBITS == 1
								unpack_image_number(rocket_i_packed, rocket_i_packed_len, 32, 32, image, 0, 0, loop % 6);
								#elif COMP_HEATSHRINK == 1
								unpack_image_number(rocket_i_heatshrunk, rocket_i_heatshrunk_len, 32, 32, image, 0, 0, loop % 6);
								#endif
								ssd1306_refresh();
								decomp_t += millis() - frame_t;	// time to decompress last frame
								frame_t = millis();		// restore scheduling functionality
							}
						}
						decomp_t /= 26 * 6;
						printf("spent %lu ms decompressing and writing each frame\n\r", decomp_t);
						break;
					default:
						break;
				}
				ssd1306_refresh();
			
				systick_delay_ms(2000);
			}
		}
	}
	else
		printf("failed.\n\r");
	
	printf("Stuck here forever...\n\r");
	while(1);
}
