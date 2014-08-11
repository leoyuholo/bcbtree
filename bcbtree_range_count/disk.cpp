#include "disk.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace std;

disk::disk(size_t bsize, const char* name) : BLOCK_SIZE(bsize), file_name(name)
{
	if ((file_pointer = fopen(file_name, "rb+")) != NULL)
	{
		if (fseek(file_pointer, 0, SEEK_END))
			max_block_id = 0;
		else
			max_block_id = ftell(file_pointer) / BLOCK_SIZE;
	}
	else
	{
		file_pointer = fopen(file_name, "wb+");
		max_block_id = 0;
	}
	io_count=0;
}

disk::~disk()
{
	if (file_pointer != NULL)
	{
		fflush(file_pointer);
		fclose(file_pointer);
		file_pointer = NULL;
	}
}
size_t disk::get_block_size() const
{
	return BLOCK_SIZE;
}

block_t disk::get_max_block_id() const
{
	return max_block_id;
}

long long disk::get_io_count() const
{
	return io_count;
}

int disk::append_new_block()
{
	max_block_id++;
	return 0;
}

int disk::read_block(block_t block_id, void* target)
{
	if (block_id < 0 || block_id >= max_block_id)
		return 1;
	if (fseek(file_pointer, BLOCK_SIZE * block_id, SEEK_SET))
		return 1;
	if (fread(target, 1, BLOCK_SIZE, file_pointer) != BLOCK_SIZE)
		return 1;
	io_count++;
	return 0;
}

int disk::write_block(block_t block_id, const void* content)
{
	if (block_id >= max_block_id)
		return 1;
	if (fseek(file_pointer, BLOCK_SIZE * block_id, SEEK_SET))
		return 1;
	if (fwrite(content, 1, BLOCK_SIZE, file_pointer) != BLOCK_SIZE)
		return 1;
	io_count++;
	return 0;
}

int disk::append_write_block(block_t block_id, const void* content)
{
	while (block_id >= max_block_id){
		append_new_block();
	}
	if (fseek(file_pointer, BLOCK_SIZE * block_id, SEEK_SET))
		return 1;
	if (fwrite(content, 1, BLOCK_SIZE, file_pointer) != BLOCK_SIZE)
		return 1;
	io_count++;
	return 0;
}
