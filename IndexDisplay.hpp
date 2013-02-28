#ifndef _INDEX_DISPLAY_HPP
#define _INDEX_DISPLAY_HPP

#include "led-matrix-lib/LedMatrix.hpp"

#define MAX_ERROR_PROJECTS 	2
#define MAX_PROJECTS		30
#define MAX_NAME_LEN 		30

extern "C" {
	extern uint8_t project_status[30];
	extern uint8_t next_project;
	extern uint8_t error_project_count;
	extern char error_projects[MAX_ERROR_PROJECTS][MAX_NAME_LEN];
}

class IndexDisplay : public LedMatrixAnimation {
public:
	IndexDisplay(LedMatrixFont &font)
		: messageAnim(font)
	{
		reset();
	}

	bool update(AbstractLedMatrixFrameBuffer &fb) {
		bool done = false;
		switch(mode) {
			case INDEX:
				counter++;
				fillX = fillY = 0;
				fb.clear();
				for(int i=0;i<next_project;i++) {
					LedMatrixColor color(0x3f, 0x00, 0x00);
					if( project_status[i] == 1) {
						color = LedMatrixColor(0x00, 0x3f, 0x00);
					} else if( project_status[i] == 2) {
						if( flashOn )
							color = LedMatrixColor(0x00, 0x3f);
						else
							color = LedMatrixColor(0x00, 0x00);
					}

					fb[fillY][fillX] = color.getValue();
					fillX++;
					if( fillX > fb.getColCount() ) {
						fillY++;
						fillX=0;
					}
				}
				flashOn = !flashOn;
				if( counter > 20 && error_project_count > 0 ) {
					counter = 0;
					messageAnim.setMessage(error_projects[counter]);
					fb.clear();
					mode = ERROR_NAMES;
				}
			break;
			case ERROR_NAMES:
				if( messageAnim.update(fb) ) {
					counter++;
					if( counter >= error_project_count ) {
						counter = 0;
						mode = INDEX;
					} else {
						messageAnim.setMessage(error_projects[counter]);
					}
				}
			break;
		}
		return done;
	}

	void reset() {
		counter = 0;
		fillX = fillY = 0;
		mode = INDEX;
		flashOn = true;
	}


private:
	enum Mode {INDEX, ERROR_NAMES};

	LedMatrixScrollAnimation messageAnim;
	Mode		   	mode;
	int		   	counter;
	int			fillX, fillY;
	bool			flashOn;
};

#endif
