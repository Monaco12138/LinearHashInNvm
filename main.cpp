#include "pml_hash.h"
#include <cstdlib>
#include <ctime>
vector<string> split(const string &str, const string &pattern);

int main(){
     PMLHash hash("/mnt/pmemdir/pmem0");
    fstream infile;
	infile.open("/home/chaibli/Database/Datatest/10w-rw-25-75-load.txt");
	string line;
	vector<string> sdata;
	string operate;
	int count = 0;
	size_t key;
	size_t value;
	int success;
	////////////////////////////////build the hash table
	clock_t start = clock();
	while(getline(infile , line)){
		sdata = split(line , " ");
		operate = sdata[0];
		key = stol(sdata[1]);
		
		if(operate == "INSERT"){
			//if(hash.search(key , value) != 0)
			 	hash.insert(key , key);
		}else if(operate == "READ"){
			 hash.search(key , value);
		}
	}
	clock_t end = clock();
	cout<<"the total time of load:"<<(end-start)*1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
	//cout<<"count="<<count<<endl;
	infile.close();

	infile.open("/home/chaibli/Database/Datatest/10w-rw-25-75-run.txt");
	///////////////////////////////////build the hash table
	start = clock();
	while(getline(infile , line)){
		sdata = split(line , " ");
		operate = sdata[0];
		key = stol(sdata[1]);
		
		if(operate == "INSERT"){
		  //if(hash.search(key , value) != 0)
			hash.insert(key , key);
		}else if(operate == "READ"){
			success = hash.search(key , value);
			if(success != 0){
				count++;
			}
		}
	}
	end = clock();
	cout<<"the total time of run:"<<(end-start)*1000.0 / CLOCKS_PER_SEC<<"ms"<<endl;
	cout<<"the fail count ="<<count<<endl;
	infile.close();


	
     cout<<"meta:"<<endl;
     cout<<"meta->size = "<<hash.meta->size<<endl;
     cout<<"meta->level = "<<hash.meta->level<<endl;
     cout<<"meta->next = "<<hash.meta->next<<endl;
     cout<<"meta->overflow_num = "<<hash.meta->overflow_num<<endl;
   
     /*
     size_t num[(HASH_SIZE + 1) * TABLE_SIZE];
     for(size_t i=0; i<(HASH_SIZE + 1) * TABLE_SIZE ; i++){
     	num[i] = rand() % 999 + 1;
     }

     
     for(size_t i=0 ; i<HASH_SIZE+1 ; i++){
     	cout<<"insert" <<i<<"*****************"<<endl;
     	for(size_t j=0; j<TABLE_SIZE ; j++){
     		hash.insert( num[i*HASH_SIZE + j]  , num[i*HASH_SIZE + j] );
     	}
     	hash.Show_all();
     	cout<<endl;
     }
     
     for(size_t i=0 ; i<HASH_SIZE+1 ; i++){
     	cout<<"remove" <<i<<"*****************"<<endl;
     	for(size_t j=0; j<TABLE_SIZE ; j++){
     		hash.remove( num[i*HASH_SIZE + j] );
     	}
     	hash.Show_all();
     	cout<<endl;
     }
     for(size_t i=0 ; i<HASH_SIZE+1 ; i++){
     	cout<<"insert" <<i<<"*****************"<<endl;
     	for(size_t j=0; j<TABLE_SIZE ; j++){
     		hash.insert( num[i*HASH_SIZE + j]  , num[i*HASH_SIZE + j] );
     	}
     	hash.Show_all();
     	cout<<endl;
     }*/
     return 0;
}

vector<string> split(const string &str, const string &pattern)
{
	vector<string> res;
	if (str == "")
		return res;
	//在字符串末尾也加入分隔符，方便截取最后一段
	string strs = str + pattern;
	size_t pos = strs.find(pattern);

	while (pos < strs.size())
	{
		string temp = strs.substr(0, pos);
		res.push_back(temp);
		//去掉已分割的字符串,在剩下的字符串中进行分割
		strs = strs.substr(pos + 1, strs.size());
		pos = strs.find(pattern);
	}

	return res;
}

