#include "drawing.c"
#include "input.c"
#include "modem.c"

int main(void) {
	unsigned int frame = 0;
	unsigned short *const framebuffer_addr[2] = { framebufferMemory, framebufferMemory + 640 * 480 * 2 };
	unsigned int i;
	unsigned short lastButtons = 0xffff;
	
	unsigned int started = 0;
	unsigned int progress = 0;

	unsigned int selection = 0;
	unsigned char count = 0;
	char selections[256][64];

	enableModem();
	initStablization();

	while(1) {
		unsigned short *currentFramebuffer = framebuffer_addr[frame & 1];
		unsigned short newPress = BUTTONS & ~lastButtons;

		for(i = 0; i < 640 * 480; i++) {
			currentFramebuffer[i] = 0x03E0;
		}

		static_center_draw(currentFramebuffer, 80, 4, 0, "Homebrew Channel");

		//sendCharToModem('H');
		//modemInterrupt();

		if(started == 0) {
			static_center_draw(currentFramebuffer, 140, 2, 0, "Loading...");
			//static_center_draw(currentFramebuffer, 140 + (progress % 4) * 28, 2, 0, "Loading...");

			// Get up to 64 bytes per frame (chosen arbitrarily)
			for(i = 0; i < 64; i++) modemInterrupt();

			int r;
			while((r = stablizeGetCharFromModem()) != -1) {
				// Got a char, if count is not initialized that's first
				if(count == 0) count = r;
				else {
					selections[selection][progress++] = r;
					if(r == '\0') {
						selection++;
						progress = 0;
					}
					if(selection == count) {
						selection = 0;
						started++;
					}
				}
			}
		}
		
		else if(started == 1) {
			for(i = 0; i < count; i++) {
				draw(currentFramebuffer, 80, 140 + i * 28, 2, 0, selections[i]);
			}

			draw(currentFramebuffer, 60, 140 + selection * 28, 2, -6, "->");

			if((newPress & JOY_UP) && selection > 0) selection--;
			if((newPress & JOY_DOWN) && selection + 1 < count) selection++;

			if(newPress & (JOY_A | JOY_START)) {
				sendCharToModem(selection);
				modemInterrupt();

				frame %= 2;
				started++;
			}
		}

		else {
			// For initial PoC, just show a progress bar - don't download
			if(progress < 99) progress = frame % 100;
			
			draw(currentFramebuffer, 60, 140, 2, 0, selections[selection]);

			for(i = 0; i < 100; i++) {
				currentFramebuffer[320 - 50 + i + 180 * 640] = 0;
				currentFramebuffer[320 - 50 + i + 181 * 640] = 0;
				currentFramebuffer[320 - 50 + i + 182 * 640] = (progress >= i) * 0x0500;
				currentFramebuffer[320 - 50 + i + 183 * 640] = (progress >= i) * 0x0500;
				currentFramebuffer[320 - 50 + i + 184 * 640] = 0;
				currentFramebuffer[320 - 50 + i + 185 * 640] = 0;
			}
		}

		// Vblank
		while(*(volatile unsigned int *)0xA4400010 != 0x1e0);
		while(*(volatile unsigned int *)0xA4400010 != 0x1e2);

		framebuffer_set = currentFramebuffer;
		frame++;

		lastButtons = BUTTONS;
		READ_INPUT();
	}
}
