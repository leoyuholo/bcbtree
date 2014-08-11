#include "bcbtree_range_count.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <algorithm>

#define BLOCK_SIZE_8	(BLOCK_SIZE / 8)

using namespace std;

typedef struct Triple{
	int key;
	int i;
	int val;
} Triple;

class triple_minheap_comparator{
public:
	bool operator()(Triple t1, Triple t2){
		if(t1.key < t2.key)	return true;
		else if(t1.key == t2.key)	return (t1.i < t2.i);
		else	return false;
	}
};

class triple_minheap_comparator2{
public:
	bool operator()(Triple t1, Triple t2){
		if(t1.key > t2.key)	return true;
		else if(t1.key == t2.key)	return (t1.i > t2.i);
		else return false;
	}
};

int build_BCBtree_range_count(const char* input_file_name, const char* output_file_name){
	
	/* input file */
	FILE* input_file_ptr = fopen(input_file_name, "r");
	if(input_file_ptr == NULL)	return -1;
	
	/* disk storage */
	disk output_storage = disk(BLOCK_SIZE, output_file_name);	//output storage
	
	/* variables for the whole build_BCBtree function */
	int block_offset = 0;
	int current_output_block_id = block_offset + 1;
	int current_input_block_id = 0;
	int current_intermediate_block_id = 0;
	int leaf_start_block_id = 0;
	int leaf_end_block_id = 0;
	int btree_root_block_id = 0;
	int btree_block_cnt = current_output_block_id;
	int tmp_int = 0;
	int ret_val = 0;
	int scan_cnt = 0;
	int64_t total_records = 0;
	vector< int > btree_key[2];
	vector< int > btree_val[2];
	vector< Triple > data;
	Triple tmp_triple;
	vector< disk* > intermediate_storage[2];
	disk* tmp_disk;
	char tmp_str[2000] = {0};
	int cnt = 0;
	int* record_cnt;
	record_cnt = (int*) malloc(sizeof(int) * BLOCK_SIZE_8);
	int* no_of_records;
	no_of_records = (int*) malloc(sizeof(int) * BLOCK_SIZE_8);
	
	/* block buffer */
	char read_block[BLOCK_SIZE] = {0};
	char output_block[BLOCK_SIZE] = {0};
	char metadata_block[BLOCK_SIZE] = {0};
	char tmp_storage_block[BLOCK_SIZE] = {0};
	char intermediate_data_block[BLOCK_SIZE] = {0};
	
	/* buffer position pointer */
	char* output_block_cur_pos = output_block;
	char* intermediate_data_block_cur_pos = intermediate_data_block;
	
	/* read file and build leaf blocks and create first set of intermediate data */
	int tmp_x = 0;
	int tmp_y = 0;
	tmp_triple.val = 0;
	
	char intermediate_data[2][500] = {0};
	// create randomness to intermediate data file name
	srand(time(NULL));
	for(int i = 0;i < 2;i++){
		while(1){
			tmp_int = rand();
			sprintf(tmp_str, "%s_%d_%d", INTERMEDIATE_DATA_FILE, tmp_int, 0);
			if(FILE* test_file_ptr = fopen(tmp_str, "r")){
				fclose(test_file_ptr);
			}else{
				sprintf(intermediate_data[i], "%s_%d", INTERMEDIATE_DATA_FILE, tmp_int);
				break;
			}
		}
	}
	
	leaf_start_block_id = current_output_block_id;
	
	while(!feof(input_file_ptr)){
		
		scan_cnt = fscanf(input_file_ptr, "%d %d\n", &tmp_x, &tmp_y);
		
		if(scan_cnt != 2){
			printf("build_BCBtree_range_count(): input file format error\n");
			return -1;
		}
		
		total_records++;
		
		memcpy(output_block_cur_pos, &tmp_x, 4);
		output_block_cur_pos += 4;
		memcpy(output_block_cur_pos, &tmp_y, 4);
		output_block_cur_pos += 4;
		
		// if output block is full or last record in input file
		if(output_block_cur_pos > (output_block + BLOCK_SIZE - 8) || feof(input_file_ptr)){
			btree_key[0].push_back(tmp_x);
			btree_val[0].push_back(btree_block_cnt);
			btree_block_cnt++;
			if(output_storage.append_write_block(current_output_block_id, output_block)){
				return -1;
			}else{
				memset(output_block, 0, BLOCK_SIZE);
				current_output_block_id++;
			}
			output_block_cur_pos = output_block;
		}
		
		tmp_triple.key = tmp_y;
		tmp_triple.i = cnt / (BLOCK_SIZE_8);
		cnt++;
		
		data.push_back(tmp_triple);
		
		/* if it is the last record in input file, or the number of records in 
		 * data is already enough to construct a BCBtree */
		if(data.size() >= (BLOCK_SIZE_8 * BLOCK_SIZE_8) || feof(input_file_ptr)){
			
			for(int i = 0;i < BLOCK_SIZE_8;i++){
				record_cnt[i] = 1;
			}
			
			sprintf(tmp_str, "%s_%d", intermediate_data[0], intermediate_storage[0].size());
			tmp_disk = new disk(BLOCK_SIZE, tmp_str);
			
			intermediate_storage[0].push_back(tmp_disk);
			current_intermediate_block_id = 0;
			
			sort(data.begin(), data.end(), triple_minheap_comparator());
			
			intermediate_data_block_cur_pos = intermediate_data_block + 4;
			
			for(int i = 0;i < data.size();i++){
				
				memcpy(intermediate_data_block_cur_pos, &(data.at(i).key), 4);
				intermediate_data_block_cur_pos += 4;
				memcpy(intermediate_data_block_cur_pos, &(data.at(i).i), 4);
				intermediate_data_block_cur_pos += 4;
				memcpy(intermediate_data_block_cur_pos, &(record_cnt[data.at(i).i]), 4);
				intermediate_data_block_cur_pos += 4;
				record_cnt[data.at(i).i]++;
				
				if(intermediate_data_block_cur_pos > (intermediate_data_block + BLOCK_SIZE - 12) || i == (data.size() - 1)){
					
					tmp_int = (intermediate_data_block_cur_pos - intermediate_data_block - 4) / 12;
					memcpy(intermediate_data_block, &tmp_int, 4);
					
					if(tmp_disk->append_write_block(current_intermediate_block_id, intermediate_data_block)){
						return -1;
					}else{
						memset(intermediate_data_block, 0, BLOCK_SIZE);
						current_intermediate_block_id++;
					}
					
					intermediate_data_block_cur_pos = intermediate_data_block + 4;
				}
			}
			cnt = 0;
			data.clear();
		}
	}
	
	leaf_end_block_id = current_output_block_id - 1;
	
	/* build internal nodes:
	 * build a layer (or level) of BCBtrees on intermediate files, 
	 * then combine the intermediate files for next layer (or level) */
	int intermediate_storage_flag = 0;
	int intermediate_block_id[BLOCK_SIZE_8] = {0};
	char* intermediate_read_block;
	intermediate_read_block = (char*) malloc(sizeof(char) * BLOCK_SIZE_8 * BLOCK_SIZE);
	char* intermediate_read_block_cur_pos[BLOCK_SIZE_8];
	memset(no_of_records, 0, BLOCK_SIZE_8 * 4);
	
	while(1){
		
		for(int i = 0, j = 0;i < intermediate_storage[intermediate_storage_flag].size();i++){
			
			if(intermediate_storage[intermediate_storage_flag].size() == 1){
				btree_root_block_id = current_output_block_id;
			}
			
			/* write routing node */
			for(j = 0;j < BLOCK_SIZE_8 && (i * BLOCK_SIZE_8 + j) < btree_key[intermediate_storage_flag].size();j++){
				
				memcpy(output_block_cur_pos, &btree_key[intermediate_storage_flag].at(i * BLOCK_SIZE_8 + j), 4);
				output_block_cur_pos += 4;
				memcpy(output_block_cur_pos, &btree_val[intermediate_storage_flag].at(i * BLOCK_SIZE_8 + j), 4);
				output_block_cur_pos += 4;
			}
			
			btree_key[(intermediate_storage_flag + 1) % 2].push_back(btree_key[intermediate_storage_flag].at(i * BLOCK_SIZE_8 + j - 1));
			btree_val[(intermediate_storage_flag + 1) % 2].push_back(current_output_block_id);
			
			if(output_storage.append_write_block(current_output_block_id, output_block)){
				return -1;
			}else{
				memset(output_block, 0, BLOCK_SIZE);
				current_output_block_id++;
			}
			
			output_block_cur_pos = output_block;
			
			
			/* build BCBtree */
			sprintf(tmp_str, "%s_%d", intermediate_data[intermediate_storage_flag], i);
			ret_val = build_BCBtree(tmp_str, output_file_name, current_output_block_id);
			
			if(ret_val < 0){
				return -1;
			}else{
				current_output_block_id += ret_val;
			}
		}
		
		btree_key[intermediate_storage_flag].clear();
		btree_val[intermediate_storage_flag].clear();
		
		/* combine files */
		if(intermediate_storage[intermediate_storage_flag].size() <= 1){
			delete(intermediate_storage[intermediate_storage_flag].at(0));
			intermediate_storage[intermediate_storage_flag].clear();
			sprintf(tmp_str, "%s_%d", intermediate_data[intermediate_storage_flag], 0);
			remove(tmp_str);
			break;
		}else{
			// combine every BLOCK_SIZE_8 number of files into one single file
			for(int i = 0;i < intermediate_storage[intermediate_storage_flag].size();i += BLOCK_SIZE_8){
				
				for(int k = 0;k < BLOCK_SIZE_8;k++){
					record_cnt[k] = 1;
					no_of_records[k] = 0;
					intermediate_block_id[k] = 0;
					intermediate_read_block_cur_pos[k] = intermediate_read_block + k * BLOCK_SIZE;
				}
				
				sprintf(tmp_str, "%s_%d", intermediate_data[(intermediate_storage_flag + 1) % 2], intermediate_storage[(intermediate_storage_flag + 1) % 2].size());
				tmp_disk = new disk(BLOCK_SIZE, tmp_str);
				
				intermediate_storage[(intermediate_storage_flag + 1) % 2].push_back(tmp_disk);
				
				current_intermediate_block_id = 0;
				intermediate_data_block_cur_pos = intermediate_data_block + 4;
				
				// for each file, read a record into list
				for(int j = 0;(j < BLOCK_SIZE_8) && (i + j) < intermediate_storage[intermediate_storage_flag].size();j++){
					
					intermediate_storage[intermediate_storage_flag].at(i + j)->read_block(intermediate_block_id[j], intermediate_read_block + j * BLOCK_SIZE);
					intermediate_block_id[j]++;
					
					no_of_records[j] = *(int*)(intermediate_read_block_cur_pos[j]);
					intermediate_read_block_cur_pos[j] += 4;
					
					tmp_triple.key = *(int*)(intermediate_read_block_cur_pos[j]);
					intermediate_read_block_cur_pos[j] += 12;
					
					tmp_triple.i = j;
					data.push_back(tmp_triple);
				}
				
				make_heap(data.begin(), data.end(), triple_minheap_comparator2());
				
				// loop to merge data
				while(1){
					
					tmp_triple = data.front();
					
					memcpy(intermediate_data_block_cur_pos, &tmp_triple.key, 4);
					intermediate_data_block_cur_pos += 4;
					memcpy(intermediate_data_block_cur_pos, &tmp_triple.i, 4);
					intermediate_data_block_cur_pos += 4;
					memcpy(intermediate_data_block_cur_pos, &record_cnt[tmp_triple.i], 4);
					intermediate_data_block_cur_pos += 4;
					record_cnt[tmp_triple.i]++;
					
					no_of_records[tmp_triple.i]--;
					
					pop_heap(data.begin(), data.end(), triple_minheap_comparator2());
					data.pop_back();
					
					// if the current read block is fully consumed
					if(intermediate_read_block_cur_pos[tmp_triple.i] >= (intermediate_read_block + tmp_triple.i * BLOCK_SIZE + BLOCK_SIZE) || no_of_records[tmp_triple.i] <= 0){
						
						if(intermediate_storage[intermediate_storage_flag].at(i + tmp_triple.i)->read_block(intermediate_block_id[tmp_triple.i], intermediate_read_block + tmp_triple.i * BLOCK_SIZE) == 1){
							sprintf(tmp_str, "%s_%d", intermediate_data[intermediate_storage_flag], i + tmp_triple.i);
							remove(tmp_str);
						}else{
							intermediate_block_id[tmp_triple.i]++;
							
							intermediate_read_block_cur_pos[tmp_triple.i] = intermediate_read_block + tmp_triple.i * BLOCK_SIZE;
							
							no_of_records[tmp_triple.i] = *(int*)(intermediate_read_block_cur_pos[tmp_triple.i]);
							intermediate_read_block_cur_pos[tmp_triple.i] += 4;
							
							tmp_triple.key = *(int*)(intermediate_read_block_cur_pos[tmp_triple.i]);
							intermediate_read_block_cur_pos[tmp_triple.i] += 12;
							
							data.push_back(tmp_triple);
							push_heap(data.begin(), data.end(), triple_minheap_comparator2());
						}
					}else{
						
						tmp_triple.key = *(int*)(intermediate_read_block_cur_pos[tmp_triple.i]);
						intermediate_read_block_cur_pos[tmp_triple.i] += 12;
						data.push_back(tmp_triple);
						push_heap(data.begin(), data.end(), triple_minheap_comparator2());
					}
					
					if(intermediate_data_block_cur_pos > (intermediate_data_block + BLOCK_SIZE - 12) || data.size() == 0){
						
						tmp_int = (intermediate_data_block_cur_pos - intermediate_data_block - 4)/ 12;
						memcpy(intermediate_data_block, &tmp_int, 4);
						if(tmp_disk->append_write_block(current_intermediate_block_id, intermediate_data_block)){
							return -1;
						}else{
							memset(intermediate_data_block, 0, BLOCK_SIZE);
							current_intermediate_block_id++;
						}
						
						intermediate_data_block_cur_pos = intermediate_data_block + 4;
					}
					
					if(data.size() == 0){
						for(int j = 0;j < intermediate_storage[intermediate_storage_flag].size();j++){
							delete(intermediate_storage[intermediate_storage_flag].at(j));
						}
						intermediate_storage[intermediate_storage_flag].clear();
						break;
					}
				}
			}
		}
		
		intermediate_storage_flag = (intermediate_storage_flag + 1) % 2;
	}
	
	/* write metadata */
	memcpy(metadata_block, &total_records, 4);					//total number of records
	memcpy(metadata_block + 4, &leaf_start_block_id, 4);		//start of B-tree leaf node
	memcpy(metadata_block + 8, &leaf_end_block_id, 4);			//end of B-tree leaf node
	memcpy(metadata_block + 12, &btree_root_block_id, 4);		//root node of B-tree
	
	if(output_storage.append_write_block(block_offset, metadata_block)){
		return -1;
	}
	
	return (current_output_block_id - block_offset);
}

int query_BCBtree_range_count(const char* input_file_name, int query_x, int query_y){
	
	/* disk storage */
	disk storage = disk(BLOCK_SIZE, input_file_name);
	
	/* variables */
	int block_offset = 0;
	int read_block_id = block_offset;
	int bcbtree_block_id = 0;
	int total_records = 0;
	int leaf_start_block_id = 0;
	int leaf_end_block_id = 0;
	int btree_root_block_id = 0;
	int index_key = 0;
	int last_key = 0;
	int key_cnt = 0;
	int ret_val = 0;
	int* int_list = NULL;
	
	/* buffer */
	char metadata_block[BLOCK_SIZE] = {0};
	char read_block[BLOCK_SIZE] = {0};
	char* read_block_cur_pos = NULL;
	
	/* return value */
	int result = 0;
	
	/* read metadata */
	storage.read_block(read_block_id, metadata_block);
	total_records = *(int*)metadata_block;
	leaf_start_block_id = *(int*)(metadata_block + 4);
	leaf_end_block_id = *(int*)(metadata_block + 8);
	btree_root_block_id = *(int*)(metadata_block + 12);
	
	/* read blocks */
	read_block_id = btree_root_block_id;
	while(read_block_id > leaf_end_block_id){
		
		bcbtree_block_id = read_block_id + 1;
		storage.read_block(read_block_id, read_block);
		
		read_block_cur_pos = read_block;
		
		while(1){
			if(*(int*)read_block_cur_pos > query_x){
				read_block_id = *(int*)(read_block_cur_pos + 4);
				break;
			}else if((read_block_cur_pos + 8) >= (read_block + BLOCK_SIZE)){
				read_block_id = -1;
				key_cnt++;
				break;
			}else if(*(int*)(read_block_cur_pos + 12) == 0){
				read_block_id = -1;
				key_cnt++;
				break;
			}else{
				read_block_cur_pos += 8;
				key_cnt++;
			}
		}
		
		if(key_cnt > 0){
			if(int_list != NULL){
				free(int_list);
			}
			ret_val = query_BCBtree(input_file_name, bcbtree_block_id, &int_list, query_y);
			if(ret_val == -1){
				return 0;
			}
			for(int i = 0;i < key_cnt;i++){
				result += int_list[i];
			}
			key_cnt = 0;
		}
	}
	
	if(read_block_id <= block_offset){
		return result;
	}
	
	storage.read_block(read_block_id, read_block);
	read_block_cur_pos = read_block;
	
	while(1){
		if(*(int*)read_block_cur_pos > query_x){
			break;
		}else if((read_block_cur_pos + 8) >= (read_block + BLOCK_SIZE)){
			if(*(int*)(read_block_cur_pos + 4) <= query_y){
				result++;
			}
			break;
		}else if(*(int*)(read_block_cur_pos + 12) == 0){
			if(*(int*)(read_block_cur_pos + 4) <= query_y){
				result++;
			}
			break;
		}else{
			if(*(int*)(read_block_cur_pos + 4) <= query_y){
				result++;
			}
			read_block_cur_pos += 8;
		}
	}
	
	return result;
}
