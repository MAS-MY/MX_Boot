#include "main.h"

static int clean_f(void)
{
	static int clean = 50;
	while(clean--){
		printf("mmh >\r\n");
	}
}

struct command clean_cmd = {
    "clean",
	
    "Clean the screen\r\n",
	
    "Usage:\r\n"
    "    Clean the screen\r\n",
	
	clean_f,
};
