#ifndef GAMMAELIASCODE_H_
#define GAMMAELIASCODE_H_

	#include <sys/types.h>
	#include <string.h>
	
	unsigned int endian_swap(unsigned int x);
	
	void write_bit(char* dest, int pos, int val);
	
	int read_bit(char* source, int pos);
	
	int log2_ceiling(int64_t source);
	
	/* input: 64bit int [source] and 17-byte char array [dest]
	   return value: length of encoded value in bits, ranging from 2 to 130 */
	int eliasGammaEncode(int64_t source, char* dest);

	/* input: char array [source] and 64bit int pointer [dest]
	   return value: length of bits decoded */
	int eliasGammaDecode(char* source, int64_t* dest);
	
#endif
