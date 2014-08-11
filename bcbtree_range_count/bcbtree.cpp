#include "bcbtree.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <vector>

#define BLOCK_SIZE_8	(BLOCK_SIZE / 8)

// #define DEBUG

using namespace std;

int build_BCBtree(const char* input_file_name, const char* output_file_name, int block_offset){
	
	/* variables for the whole build_BCBtree function */
	int current_output_block_id = block_offset + 1;
	int current_input_block_id = 0;
	int current_tmp_block_id = 0;
	int btree_leaf_start_block_id = 0;
	int btree_leaf_end_block_id = 0;
	int btree_root_block_id = 0;
	int last_key = 0;
	int triple_block_written_bit_cnt = 0;
	int read_cnt = 0;
	int tmp_int = 0;
	int record_cnt = 0;
	vector< int > key_relay;	//interval alpha of each fat block
	int val_relay[BLOCK_SIZE_8] = {0};	//relay set
	char tmp_storage_file_name[200] = {0};
	
	/* block buffer */
	char read_block[BLOCK_SIZE] = {0};
	char triple_block[3 * BLOCK_SIZE] = {0};
	char relay_block[BLOCK_SIZE] = {0};
	char metadata_block[BLOCK_SIZE] = {0};
	char tmp_storage_block[BLOCK_SIZE] = {0};
	
	/* read data and build structure */
	char* read_block_cur_pos = read_block;
	int no_of_records = 0;
	int64_t triple_key = 0;	//^£_(j)
	int triple_i = 0;		//i
	int64_t triple_val = 0;	//£_(kj, i)
	int key_bit_cnt = 0;
	int i_bit_cnt = log2_ceiling(BLOCK_SIZE_8 - 1);
	int val_bit_cnt = 0;
	char key_tmp_buf[17] = {0};
	unsigned int i_tmp_buf = {0};
	char val_tmp_buf[17] = {0};
	
	// create randomness to temporary storage file name
	srand(time(NULL));
	while(1){
		sprintf(tmp_storage_file_name, "%s_%d", TMP_STORAGE_FILE, rand());
		if(FILE* test_file_ptr = fopen(tmp_storage_file_name, "r")){
			fclose(test_file_ptr);
		}else{
			break;
		}
	}
	
	/* disk storage */
	disk input_storage = disk(BLOCK_SIZE, input_file_name);		//input storage
	disk output_storage = disk(BLOCK_SIZE, output_file_name);	//output storage
	disk tmp_storage = disk(BLOCK_SIZE, tmp_storage_file_name);	//temporary storage
	
	while(1){
		
		if(current_input_block_id >= input_storage.get_max_block_id())	break;
		
		input_storage.read_block(current_input_block_id, read_block);
		current_input_block_id++;
		
		read_block_cur_pos = read_block;
		
		no_of_records = *(int*)read_block_cur_pos;
		read_block_cur_pos += 4;
		
		record_cnt += no_of_records;
		
		for(int i = 0;i < no_of_records;i++){
			i_tmp_buf = -1;
			
			// read data, convert key to delta key and val to delta val
			triple_key = *(int*)(read_block_cur_pos) - last_key;
			triple_i = *(int*)(read_block_cur_pos + 4);
			triple_val = *(int*)(read_block_cur_pos + 8) - val_relay[triple_i];
			
			// compress delta key, i, delta val
			key_bit_cnt = eliasGammaEncode(triple_key, key_tmp_buf);
			i_tmp_buf &= endian_swap(triple_i << (sizeof(unsigned int) * 8 - i_bit_cnt));
			val_bit_cnt = eliasGammaEncode(triple_val, val_tmp_buf);
			
			// flush triple_block buffer if full
			if((triple_block_written_bit_cnt + key_bit_cnt + i_bit_cnt + val_bit_cnt) > (3 * BLOCK_SIZE * 8)){
				
				key_relay.push_back(last_key);
				
				while(key_relay.size() >= (BLOCK_SIZE / 4)){
					
					for(int i = 0;i < (BLOCK_SIZE / 4);i++){
						tmp_int = key_relay.at(i);
						memcpy(tmp_storage_block + i * 4, &tmp_int, 4);
					}
					
					if(tmp_storage.append_write_block(current_tmp_block_id, tmp_storage_block)){
						return -1;
					}else{
						memset(tmp_storage_block, 0, BLOCK_SIZE);
						current_tmp_block_id++;
					}
					
					key_relay.clear();
				}
				
				for(int i = 0;i < 3;i++){
					if(output_storage.append_write_block(current_output_block_id, triple_block + i * BLOCK_SIZE)){
						return -1;
					}else{
						memset(triple_block + i * BLOCK_SIZE, 0, BLOCK_SIZE);
						current_output_block_id++;
					}
				}
				
				triple_block_written_bit_cnt = 0;
				
				if(output_storage.append_write_block(current_output_block_id, relay_block)){
					return -1;
				}else{
					memset(relay_block, 0, BLOCK_SIZE);
					current_output_block_id++;
				}
				
				// prepare relay_block for next flush
				for(int i = 0;i < BLOCK_SIZE_8;i++){
					memcpy(relay_block + 4 * i, &(val_relay[i]), 4);
				}
			}
			
			// write delta_key
			for(int i = 0;i < key_bit_cnt;i++){
				write_bit(triple_block, triple_block_written_bit_cnt++, read_bit(key_tmp_buf, i));
			}
			
			// write i
			for(int i = 0;i < i_bit_cnt;i++){
				write_bit(triple_block, triple_block_written_bit_cnt++, read_bit((char*)&i_tmp_buf, i));
			}
			
			// write delta_val
			for(int i = 0;i < val_bit_cnt;i++){
				write_bit(triple_block, triple_block_written_bit_cnt++, read_bit(val_tmp_buf, i));
			}
			
			// update last_key
			last_key += triple_key;
			// update relay value
			val_relay[triple_i] += triple_val;
			
			read_block_cur_pos += 12;
		}
	}
	
	// flush non-fully-filled triple_block buffer
	if(triple_block_written_bit_cnt > 0){
		
		/* indicate end of triple data with consecutive two bits set to 1 if 
		 * only 1 bit left, set it to 1 */
		if(triple_block_written_bit_cnt + 1 <= (3 * BLOCK_SIZE * 8)){
			write_bit(triple_block, triple_block_written_bit_cnt++, 1);
		}
		
		if(triple_block_written_bit_cnt < (3 * BLOCK_SIZE * 8)){
			write_bit(triple_block, triple_block_written_bit_cnt++, 1);
		}
		
		key_relay.push_back(last_key);
		
		for(int i = 0;i < 3;i++){
			if(output_storage.append_write_block(current_output_block_id, triple_block + i * BLOCK_SIZE)){
				return -1;
			}else{
				memset(triple_block + i * BLOCK_SIZE, 0, BLOCK_SIZE);
				current_output_block_id++;
			}
		}
		
		triple_block_written_bit_cnt = 0;
		
		if(output_storage.append_write_block(current_output_block_id, relay_block)){
			return -1;
		}else{
			memset(relay_block, 0, BLOCK_SIZE);
			current_output_block_id++;
		}
	}
	
	btree_leaf_start_block_id = current_output_block_id;	//record the start of B-tree leaf nodes
	
	/* write B-tree */
	int btree_value = block_offset + 1;
	char btree_buf[BLOCK_SIZE] = {0};
	char read_buf[BLOCK_SIZE] = {0};
	vector< int > routing_node[2];
	
	/* read from temporary file and write leaf nodes of B-tree to structure file */
	for(int i = 0, j = 0;i < current_tmp_block_id;i++){
		
		tmp_storage.read_block(i, read_buf);
		
		for(int k = 0;k < (BLOCK_SIZE / 4);k++){
			
			// leaf node key
			memcpy(btree_buf + j * 8, read_buf + k * 4, 4);
			// leaf node value, same as block id of corresponding structure block
			memcpy(btree_buf + j * 8 + 4, &btree_value, 4);
			
			btree_value += 4;
			
			j++;
			
			// if btree_buf is full
			if(j >= (BLOCK_SIZE / 8)){
				if(output_storage.append_write_block(current_output_block_id, btree_buf)){
					return -1;
				}else{
					memset(btree_buf, 0, BLOCK_SIZE);
					current_output_block_id++;
				}
				
				// record the last node of leaf node block as key of routing node element
				routing_node[0].push_back(*(int*)(read_buf + k * 4));
				j = 0;
			}
		}
	}
	
	remove(tmp_storage_file_name);
	
	/* write leaf nodes from key_relay to structure file */
	for(int i = 0, j = 0;i < key_relay.size();i++){
		
		tmp_int = key_relay.at(i);
		memcpy(btree_buf + j * 8, &tmp_int, 4);
		memcpy(btree_buf + j * 8 + 4, &btree_value, 4);
		
		btree_value += 4;
		
		j++;
		
		// if btree_buf is full or reach last element of key_relay
		if(j >= (BLOCK_SIZE / 8) || i == (key_relay.size() - 1)){
			if(output_storage.append_write_block(current_output_block_id, btree_buf)){
				return -1;
			}else{
				memset(btree_buf, 0, BLOCK_SIZE);
				current_output_block_id++;
			}
			
			routing_node[0].push_back(tmp_int);
			
			j = 0;
		}
	}
	
	btree_leaf_end_block_id = current_output_block_id - 1;	//record the end of B-tree leaf nodes
	
	/* write routing node to structure file */
	int routing_node_flag = 0;
	
	while((routing_node[0].size() > 1) || (routing_node[1].size() > 1)){
		
		for(int i = 0, j = 0;i < routing_node[routing_node_flag].size();i++){
			
			tmp_int = routing_node[routing_node_flag].at(i);
			memcpy(btree_buf + i * 8, &tmp_int, 4);	//routing node key
			memcpy(btree_buf + i * 8 + 4, &btree_value, 4);	//routing node value
			
			btree_value++;
			
			j++;
			
			if(j >= (BLOCK_SIZE / 8) || i == (routing_node[routing_node_flag].size() - 1)){
				if(output_storage.append_write_block(current_output_block_id, btree_buf)){
					return -1;
				}else{
					memset(btree_buf, 0, BLOCK_SIZE);
					current_output_block_id++;
				}
				
				routing_node[(routing_node_flag + 1) % 2].push_back(tmp_int);
				
				j = 0;
			}
		}
		
		routing_node[routing_node_flag].clear();
		routing_node_flag = (routing_node_flag + 1) % 2;
	}
	
	btree_root_block_id = current_output_block_id - 1;	//record the root of B-tree
	
	/* write metadata */
	tmp_int = BLOCK_SIZE_8;
	memcpy(metadata_block, &tmp_int, 4);						//number of bundles
	memcpy(metadata_block + 4, &btree_leaf_start_block_id, 4);	//start of B-tree leaf node
	memcpy(metadata_block + 8, &btree_leaf_end_block_id, 4);	//end of B-tree leaf node
	memcpy(metadata_block + 12, &btree_root_block_id, 4);		//root node of B-tree
	memcpy(metadata_block + 16, &record_cnt, 4);				//number of reocrds
	
	if(output_storage.append_write_block(block_offset, metadata_block)){
		return -1;
	}
	
	return (current_output_block_id - block_offset);
}

int query_BCBtree(const char* input_file_name, int block_offset, int** output_list, int query){
	
	/* disk storage */
	disk storage = disk(BLOCK_SIZE, input_file_name);
	
	/* variables */
	int read_block_id = block_offset;
	int no_of_bundles = 0;
	int btree_leaf_start_block_id = 0;
	int btree_leaf_end_block_id = 0;
	int btree_root_block_id = 0;
	int record_cnt = 0;
	int element_key = 0;
	int last_key = 0;
	
	/* buffer */
	char metadata_block[BLOCK_SIZE] = {0};
	char triple_block[3 * BLOCK_SIZE] = {0};
	char relay_block[BLOCK_SIZE] = {0};
	char btree_block[BLOCK_SIZE] = {0};
	char* block_buf_cur_pos = NULL;
	
	/* return int list */
	int* int_list;
	
	/* read metadata */
	if(storage.read_block(read_block_id, metadata_block)){
		return -1;
	}
	no_of_bundles = *(int*)metadata_block;
	btree_leaf_start_block_id = *(int*)(metadata_block + 4);
	btree_leaf_end_block_id = *(int*)(metadata_block + 8);
	btree_root_block_id = *(int*)(metadata_block + 12);
	record_cnt = *(int*)(metadata_block + 16);
	
	if(no_of_bundles < 0 || btree_leaf_start_block_id < 0 || btree_leaf_end_block_id < 0 || btree_root_block_id < 0 ||record_cnt < 0){
		return -1;
	}
	
	/* read B-tree */
	read_block_id = btree_root_block_id;
	while(read_block_id >= btree_leaf_start_block_id){
		
		if(storage.read_block(read_block_id, btree_block)){
			return -1;
		}
		block_buf_cur_pos = btree_block;
		
		while(1){
			
			last_key = element_key;
			element_key = *(int*)block_buf_cur_pos;
			
			if(element_key > query || (block_buf_cur_pos + 8) >= (btree_block + BLOCK_SIZE)){
				read_block_id = *(int*)(block_buf_cur_pos + 4);
				break;
			}else if(*(int*)(block_buf_cur_pos + 12) == 0){
				read_block_id = *(int*)(block_buf_cur_pos + 4);
				break;
			}else{
				block_buf_cur_pos += 8;
			}
		}
	}
	
	int_list = (int*) malloc(sizeof(int) * no_of_bundles);
	memset(int_list, 0, sizeof(int) * no_of_bundles);
	
	if(read_block_id == block_offset){
		*output_list = int_list;
		
		return no_of_bundles;
	}
	
	if(read_block_id == (block_offset + 1)){
		last_key = 0;
	}
	
	// read triple block
	for(int i = 0;i < 3;i++){
		if(storage.read_block(read_block_id, triple_block + i * BLOCK_SIZE)){
			return -1;
		}
		read_block_id++;
	}
	
	// read relay block
	if(storage.read_block(read_block_id, relay_block)){
		return -1;
	}
	read_block_id++;
	
	for(int i = 0;i < no_of_bundles;i++){
		int_list[i] = *(int*)(relay_block + i * 4);
	}
	
	// decode
	int i_bit_cnt = log2_ceiling(no_of_bundles - 1);
	
	int bit_iter = 0;
	int64_t triple_key = 0;
	int triple_i = 0;
	int64_t triple_val = 0;
	char decode_tmp_buf[17] = {0};
	unsigned int tmp_uint = 0;
	while(1){
		
		triple_key = 0;
		triple_i = 0;
		triple_val = 0;
		
		if(read_bit(triple_block, bit_iter) && bit_iter >= (3 * BLOCK_SIZE * 8)){
			break;
		}
		
		if(read_bit(triple_block, bit_iter) && read_bit(triple_block, bit_iter + 1)){
			break;
		}
		
		// decode delta key
		memset(decode_tmp_buf, 0, 17);
		for(int i = 0;i < 130 && i < (8 * 3 * BLOCK_SIZE - bit_iter);i++){
			write_bit(decode_tmp_buf, i, read_bit(triple_block, bit_iter + i));
		}
		bit_iter += eliasGammaDecode(decode_tmp_buf, &triple_key);
		
		// decode i
		memset(decode_tmp_buf, 0, 17);
		for(int i = 0;i < i_bit_cnt && i < (8 * 3 * BLOCK_SIZE - bit_iter);i++){
			write_bit(decode_tmp_buf, i, read_bit(triple_block, bit_iter + i));
		}
		bit_iter += i_bit_cnt;
		tmp_uint = endian_swap(*(unsigned int*)decode_tmp_buf);
		triple_i |= (tmp_uint >> (sizeof(unsigned int) * 8 - i_bit_cnt));
		
		// decode delta val
		memset(decode_tmp_buf, 0, 17);
		for(int i = 0;i < 130 && i < (8 * 3 * BLOCK_SIZE - bit_iter);i++){
			write_bit(decode_tmp_buf, i, read_bit(triple_block, bit_iter + i));
		}
		bit_iter += eliasGammaDecode(decode_tmp_buf, &triple_val);
		
		// get key
		triple_key += last_key;
		last_key = triple_key;
		// get val
		triple_val = triple_val + int_list[triple_i];
		
		if(triple_key <= query){
			int_list[triple_i] = triple_val;
		}
		
		if(triple_key > query || (bit_iter >= (3 * BLOCK_SIZE * 8)))	break;
	}
	
	*output_list = int_list;
	
	return no_of_bundles;
}
