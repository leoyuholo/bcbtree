#include "gammaEliasCode.h"

unsigned int endian_swap(unsigned int x){
    return	(x>>24) | 
			((x<<8) & 0x00FF0000) |
			((x>>8) & 0x0000FF00) |
			(x<<24);
}

void write_bit(char* dest, int pos, int val){
	int index = pos >> 3;
	if(val == 0){
		//set false
		dest[index] &= (~(0x01 << (7-(pos % 8))));
	}else{
		//set true
		dest[index] |= (0x01 << (7-(pos % 8)));
	}
	
	return;
}

int read_bit(char* source, int pos){
	int index = pos >> 3;
	
	return ((source[index] & (0x01 << (7-(pos % 8)))) != 0);
}

int log2_ceiling(int64_t source){
	int result = 0;
	int64_t bitmask = ~0x00;
	
	while((source & bitmask) != 0){
		bitmask <<= 1;
		result++;
	}
	
	return result;
}

int eliasGammaEncode(int64_t source, char* dest){
	
	//hard code for source equals zero
	if(source == 0){
		dest[0] = 0x40;
		return 2;
	}
	
	//set sign
	char sign = 0;
	if(source < 0){
		sign = 1;
		source *= -1;
	}
	
	//count source length in bits
	int source_length = log2_ceiling(source);
	
	memset(dest, 0, 17);
	
	write_bit(dest, 0, sign);
	
	source <<= (64 - source_length);
	
	int i;
	int64_t bit_mask = ~(((u_int64_t) -1) >> 1);
	for(i = 0;i < source_length;i++){
		write_bit(dest, i + 1 + source_length, (source & bit_mask) >> 63);
		source <<= 1;
	}
	
	return i + 1 + source_length;
}

int eliasGammaDecode(char* source, int64_t* dest){
	
	//hard code for second bit is 1, whcih equals zero
	if((source[0] & 0x40) != 0x00){
		*dest = 0;
		return 2;
	}
	
	//read sign
	char sign = 1;
	if((source[0] & 0x80) != 0x00){
		sign = -1;
	}
	
	int dest_length = 2;
	while(!read_bit(source, dest_length)){
		dest_length++;
	}
	dest_length -= 2;
	
	*dest = 0x00;
	
	int i;
	for(i = 0;i <= dest_length;i++){
		*dest |= (int64_t)read_bit(source, i + 2 + dest_length) << (dest_length - i);
	}
	*dest *= sign;
	
	return i + 2 + dest_length;
}
