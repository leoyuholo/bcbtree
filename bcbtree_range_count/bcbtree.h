#ifndef BCBTREE_H_
#define BCBTREE_H_

	#include "disk.h"
	#include "gammaEliasCode.h"
	
	#define BLOCK_SIZE	4096
	#define TMP_STORAGE_FILE "bcbtree_temporary"
	
	/* Description: 
	 * Building a BCB-tree structure on [block_offset]-th block of 
	 * the file named [output_file_name], which the input data 
	 * are stored in the files named as [input_file_name]
	 * 
	 * Remark:
	 * input data need to be sorted in ascending order of key in advance
	 * 
	 * Return value: 
	 * the number of blocks occupied for the built structure, -1 on fail
	 */
	int build_BCBtree(const char* input_file_name, const char* output_file_name, int block_offset);
	
	/* Description: 
	 * Query a BCB-tree structure, which started on [block_offset] of the 
	 * file named [input_file_name], with [query] as key. The query result 
	 * will be stored in a newly created int array, which the address of the 
	 * first element of the array will be stored in [output_list], with same 
	 * order as the file list when the structure was built. 
	 * 
	 * Return value: 
	 * number of elements in output_list, -1 on fail
	 */
	int query_BCBtree(const char* input_file_name, int block_offset, int** output_list, int query);
	
#endif
