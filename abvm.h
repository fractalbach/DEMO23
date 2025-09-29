#include <stdint.h>

const int ABVM_DpadUp = 0;
const int ABVM_DpadDown = 1;
const int ABVM_DpadRight = 2;
const int ABVM_DpadLeft = 3;

typedef struct {
	int buttons;
} AbvmGamepad;

void abvmShow( uint32_t* , int, int);
void abvmGamepad( int, AbvmGamepad* );
