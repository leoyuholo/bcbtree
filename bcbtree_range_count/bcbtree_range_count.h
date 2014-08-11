#ifndef BCBTREE_RANGE_COUNT_H_
#define BCBTREE_RANGE_COUNT_H_
	
	#include "bcbtree.h"
	
	#define BLOCK_SIZE	4096
	#define INTERMEDIATE_DATA_FILE "range_count_intermediate_data"
	
	/* Description: 
	 * Building a BCB-tree range count structure on the file named 
	 * [output_file_name], which the input data are stored in the 
	 * files named as [input_file_name]
	 * 
	 * Remark:
	 * input data need to be sorted parimarily in ascending order of 
	 * x-axis coordinate and secondarily in y-axis coordinate in advance
	 * 
	 * Return value: 
	 * the number of blocks occupied for the built structure, -1 on fail
	 */
	int build_BCBtree_range_count(const char* input_file_name, const char* output_file_name);
	
	/* Description: 
	 * Query a BCB-tree range count structure stored in the file named 
	 * [input_file_name] with [query_x] as x-axis key, [query_y] as y-axis key.
	 * 
	 * Return value: 
	 * number of points included, -1 on fail
	 */
	int query_BCBtree_range_count(const char* input_file_name, int query_x, int query_y);
	
#endif
