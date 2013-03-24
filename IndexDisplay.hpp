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

template<class FbType>
class IndexDisplay : public LedMatrixAnimation<FbType> {
public:
	IndexDisplay(LedMatrixFont &font)
		: messageAnim(font)
	{
		reset();
	}

	bool update(FbType &fb) {
		bool done = false;
		fillX = fillY = 0;
		for(int i=0;i<next_project;i++) {
			LedMatrixColor color(32, 0x00, 0x00);
			if( project_status[i] == 1) {
				color = LedMatrixColor(0x00, 32, 0x00);
			} else if( project_status[i] == 2) {
				color = LedMatrixColor(0x00, fadeLevel, 0x00);
			} else if( project_status[i] == 3) {
				color = LedMatrixColor(fadeLevel, 0x00, 0x00);
			}

			fb.putPixel(fillX, fillY, color);
			fillX++;
			if( fillX > fb.getColCount() ) {
				fillY++;
				fillX=0;
			}
		}
		fadeLevel += fadeInc;
		if( fadeLevel > 32 ) {
			fadeInc = -fadeInc;
		}
		if( fadeLevel > 64 ) {
			fadeLevel = 0;
		}

		messageTick++;
		if( error_project_count > 0 && messageTick > 1) {
			messageTick = 0;
			if( counter == -1 ) {
				messageAnim.setMessage(error_projects[0]);
				counter=0;
			}
			if( messageAnim.update(fb) ) {
				counter++;
				if( counter >= error_project_count ) {
					counter = -1;
				} else {
					messageAnim.setMessage(error_projects[counter]);
				}
			}
		}
		return done;
	}

	void reset() {
		counter = -1;
		fillX = fillY = 0;
		fadeLevel = 0;
		fadeInc = 2;
	}


private:
	LedMatrixScrollAnimation<FbType> messageAnim;
	int		   	counter;
	int			fillX, fillY;
	uint16_t		fadeLevel;
	int			fadeInc;
	uint8_t			messageTick;
};

#endif
