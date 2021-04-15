#include "pml_hash.h"
/**
 * PMLHash::PMLHash 
 * 
 * @param  {char*} file_path : the file path of data file
 * if the data file exist, open it and recover the hash
 * if the data file does not exist, create it and initial the hash
 */
PMLHash::PMLHash(const char* file_path) {
	//initial the pointer;

	start_addr = pmem_map_file(file_path , FILE_SIZE , PMEM_FILE_CREATE ,0666, NULL , NULL);
	if(start_addr == NULL){
		perror("pmem_map_file() fail!\n");
		exit(1);
	}

	overflow_addr = (void*)((size_t*)start_addr  + 1024*1024); //overflow_addr start at 8M

	meta = (metadata*)start_addr;					//meat start at 0

	bitmap = (size_t*)(meta+1);				//bitmap start at 32 , length = 4*1024

	table_arr = (pm_table*)((char*)start_addr + 32 + 4*1024 );  // table_arr start at 32 + 4*1024

	//table_arr = (pm_table*)(meta+1);		//table_arr start at 32


	/////////// initial meta
	//cout<<"meta->level="<<meta->level<<endl;
	//if(meta->level == 0 && meta->next == 0){
		meta->level = 0;
		meta->next = 0;
		meta->size = HASH_SIZE;
		meta->overflow_num = 0;
		//////////////initial bitmap
		
		for(int i=0; i<BITMAP ; i++){
			bitmap[i] = 0;
		}

		//////////////////////initial table_arr
		
		pm_table* ptable = NULL;
		for(int i=0; i<HASH_SIZE; i++){
			ptable = table_arr + i;
			ptable->fill_num = 0;
			ptable->next_offset = 0;
			for(int j=0; j<TABLE_SIZE; j++){
				(ptable->kv_arr[j]).key = 0;
				(ptable->kv_arr[j]).value = 0;
			}
		}
	//}
	

}
/**
 * PMLHash::~PMLHash 
 * 
 * unmap and close the data file
 */
PMLHash::~PMLHash() {

	///////////////persist the data to persistent memeory

	pmem_persist(start_addr , FILE_SIZE);

	//////////////

    pmem_unmap(start_addr, FILE_SIZE);

    start_addr = NULL;
    overflow_addr = NULL;
    meta = NULL;
    table_arr = NULL;
    bitmap = NULL;
    ///////////end
}
/**
 * PMLHash 
 * 
 * split the hash table indexed by the meta->next
 * update the metadata
 */
void PMLHash::split()
{
	/////////initial variable

	size_t N_level  = HASH_SIZE * pow(2 , meta->level);
	size_t  N_level_plus = HASH_SIZE * pow(2 , meta->level + 1);
	entry temp;
	size_t index0=0 , index1=0 , index2=0;
	pm_table* spiltTable = table_arr + meta->next;		//split table
	pm_table* newTable = table_arr + meta->next + N_level; //new table
	pm_table* p0 = NULL;
	pm_table* p1 = spiltTable;
	pm_table* p2 = newTable;
	////////////exception handling

	if( (pm_table*)spiltTable > (pm_table*) overflow_addr || (pm_table*)newTable > (pm_table*)overflow_addr ){
		perror("virtual address of Hash Table Array over the overflow_addr!\n");
		exit(1);
	}
	///////////initial new table

	newTable->fill_num = 0;
	newTable->next_offset = 0;
	for(size_t i=0; i<TABLE_SIZE; i++){
		(newTable->kv_arr[i]).key = 0;
		(newTable->kv_arr[i]).value = 0;
	}


	///////split, p1,p0->split_table  , p2->new_table
	
	for(p0 = spiltTable ; p0 ; p0 = pNext(p0->next_offset) ){

		p0->fill_num = 0;

		for(index0=0 ; index0 < TABLE_SIZE ; index0++){

			temp = p0->kv_arr[index0];

			(p0->kv_arr[index0]).key = 0;		//灏嗚繖涓綅缃厓绱犲綊闆?
			(p0->kv_arr[index0]).value = 0;
			if(temp.key != 0){

				if(hashFunc(temp.key , N_level_plus) == meta->next ){	//褰撳厓绱犵洿杩樻槸灞炰簬next鏃跺€?

					if(index1 < TABLE_SIZE){						//娌℃湁婊?

						p1->kv_arr[index1++] = temp;
						(p1->fill_num)++;

					}else{

						p1 = pNext(p1->next_offset);
						index1 = 0;
						p1->kv_arr[index1++] = temp;
						(p1->fill_num)++;

					}

				}else{			//褰撳厓绱犵洿灞炰簬鏂扮殑妗舵椂鍊?

					if(index2 < TABLE_SIZE){

						p2->kv_arr[index2++] = temp;
						(p2->fill_num)++;

					}else{
						p2 = newOverflowTable(p2->next_offset);
						index2 = 0;
						p2->kv_arr[index2++] = temp;
						(p2->fill_num)++;
					}
				}

			}
		}

	}

	//recycle the overflow space

	while(Recycle(spiltTable));	

    // update the next of metadata

    (meta->size)++;
    
    if( meta->next == N_level - 1){
    	(meta->level)++;
    	meta->next = 0;
    }else{
    	(meta->next)++;
    }

}
/**
 * PMLHash 
 * 
 * @param  {uint64_t} key     : key
 * @param  {size_t} hash_size : the N in hash func: idx = hash % N
 * @return {uint64_t}         : index of hash table array
 * 
 * need to hash the key with proper hash function first
 * then calculate the index by N module
 */
uint64_t PMLHash:: hashFunc(const uint64_t &key, const size_t &hash_size)
{
	return key % hash_size;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} offset : the file address offset of the overflow hash table
 *                             to the start of the whole file
 * @return {pm_table*}       : the virtual address of new overflow hash table
 */
pm_table* PMLHash:: newOverflowTable(uint64_t &offset)
{

	size_t index = determine_location(bitmap);
	//size_t index = meta->overflow_num;
	offset =  8*1024*1024 + index * sizeof(pm_table);
	(meta->overflow_num)++;


	if( offset + sizeof(pm_table) < FILE_SIZE ){

		pm_table* ptable = (pm_table*)( (char*)start_addr + offset );
		ptable->fill_num = 0;
		ptable->next_offset = 0;
		for(size_t i=0; i<TABLE_SIZE ;i++){
	     	(ptable->kv_arr[i]).key = 0;
			(ptable->kv_arr[i]).value = 0;
		}
		return ptable;

	}else{

		perror(" the space of overflow is over!\n");
		exit(1);

	}
}

pm_table* PMLHash:: pNext(uint64_t &offset)
{
	if(offset > 0){
		return (pm_table*)( (char*)start_addr + offset );
	}else{
		return NULL;
	}
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : inserted key
 * @param  {uint64_t} value : inserted value
 * @return {int}            : success: 0. fail: -1
 * 
 * insert the new kv pair in the hash
 * 
 * always insert the entry in the first empty slot
 * 
 * if the hash table is full then split is triggered
 */
int PMLHash::insert(const uint64_t &key, const uint64_t &value)
{
	////////initial variable

	size_t N_level  = HASH_SIZE * pow(2 , meta->level);
	size_t N_level_plus = HASH_SIZE * pow(2 , meta->level + 1);
	//entry temp;
	pm_table* ptable = NULL;
	uint64_t index = hashFunc(key , N_level);
	int success = -1;
	if(index < meta->next){
		index = hashFunc(key , N_level_plus);
	}

	pm_table* intable = table_arr + index;

	//////// if it will trigger split

	for( ptable = intable ; ptable ; ptable = pNext(ptable->next_offset) ){
		if(ptable->fill_num < TABLE_SIZE){
			break;
		}
	}

	/////////////// if ptable != NULL -> not split else split

	if(ptable){
		/*
		for(size_t i=0; i<TABLE_SIZE; i++){
			if( (ptable->kv_arr[i]).key == 0 ){
				(ptable->kv_arr[i]).key = key;
				(ptable->kv_arr[i]).value = value;
				(ptable->fill_num)++;
				break;
			}
		}*/
		size_t i = ptable->fill_num ;
		ptable->kv_arr[i].key = key;
		ptable->kv_arr[i].value = value;
		(ptable->fill_num)++;
		success = 0;

	}else{

		if(index == meta->next){
			index = hashFunc(key , N_level_plus);
		}

		split();

		Getvalue(key , value , table_arr + index);	//insert the data

		success = 0;
	}

	return success;
}


void PMLHash:: Getvalue(const uint64_t &key, const uint64_t &value , pm_table* table_arr)
{
	pm_table* ptable = table_arr;
	
	////// find the table to insert

	for( ; ptable ;   ptable = pNext(ptable->next_offset) ){
		if(ptable->fill_num < TABLE_SIZE){
			break;
		}else if(ptable->next_offset <=0){
			ptable = newOverflowTable(ptable->next_offset);
			break;
		}
	}

	////// insert the data to the table
	/*
	for(size_t i=0; i<TABLE_SIZE; i++){
		if( (ptable->kv_arr[i]).key == 0 ){
			(ptable->kv_arr[i]).key = key;
			(ptable->kv_arr[i]).value = value;
			(ptable->fill_num)++;
			break;
		}
	}*/

	size_t i = ptable->fill_num ;
	ptable->kv_arr[i].key = key;
	ptable->kv_arr[i].value = value;
	(ptable->fill_num)++;
}


/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : the searched key
 * @param  {uint64_t} value : return value if found
 * @return {int}            : 0 found, -1 not found
 * 
 * search the target entry and return the value
 */
int PMLHash::search(const uint64_t &key, uint64_t &value)
{
	//////initial variable

	size_t N_level  = HASH_SIZE * pow(2 , meta->level);
	size_t N_level_plus = HASH_SIZE * pow(2 , meta->level + 1);
	pm_table* ptable = NULL;
	uint64_t index = hashFunc(key , N_level);
	if(index < meta->next){
		index = hashFunc(key , N_level_plus);
	}
	int success = -1;
	pm_table* intable = table_arr + index;

	//////////find the data 

	for( ptable = intable ; ptable ; ptable = pNext(ptable->next_offset) ){
		for(size_t i=0; i < ptable->fill_num ; i++){
			if( (ptable->kv_arr[i]).key == key ){
				value = (ptable->kv_arr[i]).value;
				success = 0;
				break;
			}
		}
		if(success == 0){
			break;
		}
	}

	/////////end

	return success;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : target key
 * @param  {uint64_t} value : new value
 * @return {int}            : success: 0. fail: -1
 * 
 * update an existing entry
 */
int PMLHash::update(const uint64_t &key, const uint64_t &value)
{
	/////////////////initial variable

	size_t N_level  = HASH_SIZE * pow(2 , meta->level);
	size_t N_level_plus = HASH_SIZE * pow(2 , meta->level + 1);
	pm_table* ptable = NULL;
	uint64_t index = hashFunc(key , N_level);
	if(index < meta->next){
		index = hashFunc(key , N_level_plus);
	}
	int success = -1;
	pm_table* intable = table_arr + index;

	//////////find the data 

	for( ptable = intable ; ptable ; ptable = pNext(ptable->next_offset) ){
		for(size_t i=0; i < ptable->fill_num ; i++){
			if( (ptable->kv_arr[i]).key == key ){
				(ptable->kv_arr[i]).value = value;
				success = 0;
				break;
			}
		}
		if(success == 0){
			break;
		}
	}

	return success;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key : target key
 * @return {int}          : success: 0. fail: -1
 * 
 * remove the target entry, move entries after forward
 * if the overflow table is empty, remove it from hash
 */
int PMLHash::remove(const uint64_t &key)
{
	//////initial variable

	size_t N_level  = HASH_SIZE * pow(2 , meta->level);
	size_t N_level_plus = HASH_SIZE * pow(2 , meta->level + 1);
	pm_table* ptable = NULL;
	uint64_t index = hashFunc(key , N_level);
	if(index < meta->next){
		index = hashFunc(key , N_level_plus);
	}
	int success = -1;
	size_t in = 0;
	pm_table* intable = table_arr + index;
	pm_table* pretable = NULL;

	//////////find the data 

	for( ptable = intable ; ptable ; pretable = ptable , ptable = pNext(ptable->next_offset) ){
		for(size_t i=0; i < ptable->fill_num ; i++){
			if( (ptable->kv_arr[i]).key == key ){
				in = i;
				success = 1;
				break;
			}
		}
		if(success == 1){
			break;
		}
	}

	///////////////remove the data and move entries after forward
	if(ptable){
		size_t i,j;

		for( i = in , j= in+1 ; j < ptable->fill_num ; i++ , j++){
			(ptable->kv_arr[i]) = (ptable->kv_arr[j]);
		}

		(ptable->kv_arr[i]).key = 0;
		(ptable->kv_arr[i]).value = 0;
		(ptable->fill_num)--;

		/////////////// remove the hash table

		if(ptable->fill_num == 0 && pretable){
			size_t local;
			local = pretable->next_offset;

			pretable->next_offset = ptable->next_offset;

			size_t temp_index =  (local - 8*1024*1024)/sizeof(pm_table);

			set_zero(temp_index ,bitmap);

			(meta->overflow_num)--;
		}

		success = 0;
	}

	return success;
}


size_t PMLHash:: determine_location(size_t* arr)
{
	for(size_t i=0; i<BITMAP ; i++){	

        if(arr[i] < SIZE_MAX){	//鎵惧埌绗竴涓瓨鍦?鐨刡itmap

            size_t bitMask = 1;

            bitMask <<= 63;		//bitMask宸︾Щ63浣?

            for(size_t j=0; j<64; j++){

                if( (bitMask & arr[i]) == 0){	//鑻ョ粨鏋滀负0 璇存槑鎵惧埌浜嗕竴涓?浣嶇疆

                    arr[i] |= bitMask;			//鍋氭垨杩愮畻锛屽皢璇ヤ綅缃厓绱犵疆涓?

                    //pmem_persist(start_addr , FILE_SIZE);

                    return i*64 + j;			//杩斿洖搴忓彿鏁?
                }

                bitMask >>= 1;
            }
        }

    }
 
    return BITMAP*64;		//杩斿洖涓€涓渶澶ф暟锛岃〃绀虹┖闂寸敵璇峰け璐?
}


void PMLHash:: set_zero(size_t num,size_t* arr)
{
	size_t i = num/64;		//鎵惧埌鏄摢涓€涓綅缃?

    size_t j = num%64;

    size_t bitMask = 1;

    bitMask <<= 63;

    for(size_t index = 0; index < j ; index++){		//瀹氫綅

        bitMask >>= 1;

    }

    arr[i] ^= bitMask;			//鍋氬紓鎴栨搷浣滀娇瀵瑰簲浣嶆暟鍙樹负0

}


int PMLHash:: Recycle(pm_table* table_arr)
{
	pm_table* intable = table_arr;
	pm_table* pretable = NULL;

	for( ; intable ; pretable=intable , intable=pNext(intable->next_offset) ){
		if(intable->fill_num == 0){
			break;
		}
	}
	if(intable && pretable){
		size_t local;

		local = pretable->next_offset;

		pretable->next_offset = intable->next_offset;

		size_t index =  (local - 8*1024*1024)/sizeof(pm_table);

		set_zero(index ,bitmap);

		(meta->overflow_num)--;
		return 1;
	}else{
		return 0;
	}
}


void PMLHash:: Show_one(pm_table* ptable_arr)
{
	pm_table* ptable = ptable_arr;
	for(; ptable ; ptable = pNext(ptable->next_offset) ){
		cout<<"| ";
		for(size_t i=0; i<ptable->fill_num ; i++){

			cout<< (ptable->kv_arr[i].key)%999 <<" | ";

		}

		if(ptable->next_offset > 0){
			cout<<"--> ";
		}
	}
	cout<<endl;
}

void PMLHash:: Show_all()
{
	pm_table* ptable = table_arr;
	for(size_t i=0; i<meta->size ; i++){
		cout<<"| index="<<i<<" : ";
		Show_one(ptable + i);
		cout<<endl;
	}
}







