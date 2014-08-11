/*
 * disk.h
 *
 *  Created on: 4 Oct, 2011
 *      Author: xchu
 */

#ifndef DISK_H_
#define DISK_H_

typedef unsigned block_t;

#include <cstdio>

class disk
{
private:
	const size_t BLOCK_SIZE;
	const char* file_name;
	FILE* file_pointer;
	block_t max_block_id;
	long long io_count;

public:
	disk(size_t, const char*);
	~disk();

	size_t get_block_size() const;
	block_t get_max_block_id() const;
	long long get_io_count() const;
	int append_new_block(); // 0 - success; 1 - error
	int write_block(block_t block_id, const void* content); // same as above
	int append_write_block(block_t block_id, const void* content); // same as above
	int read_block(block_t block_id, void* target); // same as above
};


#endif /* DISK_H_ */
