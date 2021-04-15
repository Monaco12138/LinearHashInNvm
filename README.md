# Linear Hash in NVM

#### [仓库地址](https://github.com/Monaco12138/LinearHashInNvm)  
#### 0.实验环境  
ubuntu20.0 , 虚拟机使用VM_Ware_Player
### 1.根据[Intel的教程](https://software.intel.com/content/www/us/en/develop/articles/how-to-emulate-persistent-memory-on-an-intel-architecture-server.html)，利用普通内存模拟NVM环境并测试是否配置正确
#### 1)配置grub:
            vi /etc/default/grub

<img src="https://github.com/smellsx/-/blob/main/images/persistent_memory_1.png" width = "75%">

#### 2)更新grub:
            sudo update-grub
            reboot

<img src="https://github.com/smellsx/-/blob/main/images/persistent_memory_2.png" width = "75%">

#### 3)输入指令make nconfig
#### 4)进入到Device Drivers,并且在其中找到NVDIMN Support(注意NVDIMN Support不在第一页）
#### 5)配置文件系统DAX
#### 6)编译以及安装内核
            make -j8
            make modules_install install
#### 7)打印e820表格
            dmesg | grep e820

<img src="https://github.com/smellsx/-/blob/main/images/persistent_memory_3.png" width = "75%">

#### 8)配置以及模拟可持续化内存
            GRUB_CMDLINE_LINUX="memmap=4G!4G"
#### 9)使用命令查看是否配置成功
            dmesg | grep user
<img src="https://github.com/smellsx/-/blob/main/images/NVM02.png" width = "75%">      
<img src="https://github.com/smellsx/-/blob/main/images/NVM01.png" width = "75%">      
​                                                                                                                                                                          
​    

### 2.根据PMDK的README安装教程进行库安装
#### 1)libdaxctl-devel依赖安装: 
            sudo apt-get install libdaxctl-dev
#### 2)pandoc命令安装: 
            sudo apt-get install pandoc
#### 3)m4命令安装: 
            sudo apt-get install m4
#### 4)libfabric依赖安装: 
            sudo apt-get install libfabric-dev

<img src="https://github.com/smellsx/-/blob/main/images/PMDK_1.png" width = "75%">

#### 5)PMDK测试: 
            cp src/test/testconfig.sh.example src/test/testconfig.sh
            make test
            make check
#### 测试结果：
<img src="https://github.com/smellsx/-/blob/main/images/test1.png" width = "75%">    


​配置好了NVM空间,PMDK库后在程序开始时输入如下指令以便可以使用：
```Markdown
    ##挂载
	sudo mkfs.ext4 /dev/pmem0
	sudo mount -o dax /dev/pmem0 /mnt/pmemdir

    ##更改权限
        sudo touch /mnt/pmemdir/pmem0
        sudo chmod 777 /mnt/pmemdir/pmem0
```
                                                                                                                                                                                       
### 3. PML-Hash设计细节

### 数据结构设计
PMLHash的主要结构已经在头文件中给出，主要包括元数据，存储的哈希表数组数据以及溢出哈希表数据，默认将所有数据放在一个16MB的文件中存储，结构如下：
```
// PMLHash
| Metadata | Hash Table Array | Overflow Hash Tables |
+---------- 8 MB -------------+------- 8 MB ---------+

// Metadata
| size | level | next | overflow_num |

// Hash Table Array
| hash table[0] | hash table[1] | ... | hash table[n] | 

// Overflow Hash Tables
| hash table[0] | hash table[1] | ... | hash table[m] |

// Hash Table
| key | value | fill_num | next_offset |

//bitmap 512*64=4K ，开始时全置为0表示所有溢出页面都可用，每用一个将相应位数置为1表示不可用
|size_t[0] | size_t[1] |... |size_t[512]|  
```

### 操作详解
#### Insert
插入操作用于插入一个新的键值对，首先要找到相应的哈希桶，然后插入。插入的时候永远插入第一个空的槽位，维持桶的元素的连续性。可能原本的哈希桶本身已经满了且有溢出桶，那就插入溢出桶。插入后桶满，就出发分裂操作，分裂的桶被metadata中的next标识。

产生新的溢出桶从数据文件的Overflow Hash Tables区域获取空间，其空闲的起始位置可以通过determine_location函数算出

#### Remove
删除操作用于移除目标键值对，为了位置哈希桶的连续性，采用将移除键值位置后的其他键值向前移动的方式进行覆盖，然后更新桶的fill_num指示桶的最后一个元素位置，达到删除的目的。若删除完该元素后该哈希桶正好为空，就将该桶移除，并通过set_zero函数将位图中相应的位数置为0表示该位可用


#### Update
更新操作修改目标键值对的值，先找到对应的位置，然后修改再persist即可。

#### Search
查找操作根据给定的键找对应的值然后返回

​       



### 4.根据项目框架和需求实现代码并运行，测试每个功能运行并截图相应结果

​      

#### 测试代码 insert & remove


```C

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
```

因结果太长不便显示，此测试结果见上传的附件Insert&Remove.txt中


- ####  Insert功能测试

```C
for (uint64_t i = HASH_SIZE * TABLE_SIZE + 1; 
          i <= (HASH_SIZE + 1) * TABLE_SIZE; i++) {
         hash.insert(num[i-1] , num[i-1]);
}
     hash.Show_all();
     cout<<"*******************"<<endl;
for(size_t i=HASH_SIZE*TABLE_SIZE ; i<(HASH_SIZE+1)*TABLE_SIZE ; i++){
         hash.remove(num[i]);
}
     hash.Show_all();
     cout<<"******************"<<endl;

```
第一次Insert:  

<img src="https://github.com/smellsx/-/blob/main/images/insert1.PNG" width = "75%">    

第二次Insert:  

<img src="https://github.com/smellsx/-/blob/main/images/insert2.PNG" width = "75%">  


- #### Remove功能测试
```C
for(size_t i=HASH_SIZE*TABLE_SIZE ; i<(HASH_SIZE+1)*TABLE_SIZE ; i++){
         hash.remove(num[i]);
}
hash.Show_all();
cout<<"******************"<<endl;
for(size_t i = 0 ; i < HASH_SIZE*TABLE_SIZE ; i++ ){
         hash.remove(num[i]);
}
hash.Show_all();

```
第一次Remove：   

<img src="https://github.com/smellsx/-/blob/main/images/remove1.PNG" width = "75%">      
   
第二次Remove： 

<img src="https://github.com/smellsx/-/blob/main/images/remove2.PNG" width = "50%">  


#### 测试代码 Search


```C
for(size_t i=0 ; i < (HASH_SIZE + 1) * TABLE_SIZE ; i++){
         size_t val;
         hash.search(num[i] , val);
         cout<<"******"<<endl;
         cout << "key: " << num[i] << "\nvalue: " << val << endl;
         if(num[i] != val){
             cout<<"error!"<<endl;
             break;
         }
}
```
因结果太长不便显示，此测试结果见上传的附件Search.txt中




  



### 5.自行编写YCSB测试，运行给定的Benchmark数据集并测试OPS(Operations per second)和延迟两个性能指标
|(有回收溢出空间)| time  | 溢出页面 |  OPS|
|------| -------|------------| -----------|
|0-100 load+run | 127.168ms+12.618ms| 2145|  791366|
|25-75 load+run | 110.218ms+10.751ms| 2124|916666|
|50-50 load+run | 136.775ms+12.087ms| 2108|743243|
|75-25 load+run | 125.662ms+11.35ms | 2097|808823|
|100-0 load+run | 138.051ms+10.521ms| 2084|  743243|


------------------------  

|(没有回收溢出空间)| time |溢出页面 |  OPS |
|------| -------|------------| -----------|
|0-100 load+run | 107.168ms+10.876ms| 6489|  940170| 
|25-75 load+run | 111.077ms+10.541ms| 6427|909090|
|50-50 load+run | 109.068ms+10.595ms| 6355|924369|
|75-24 load+run | 111.921ms+10.729ms | 6259|909090|
|100-0 load+run | 109.418ms+10.611ms| 6165|  924369|

### 改进:(2020-12-24)
更改了pmem_persist()规则，原来是只有析构函数时调用，现在是每次更改数据后都调用。
更改后的代码为pml_hash_new.cpp , 更改前为pml_hash_old.cpp
指标如下
|(有回收溢出空间)| time  | 溢出页面 |  OPS|
|------| -------|------------| -----------|
|0-100 load+run | 136.392ms+13.418ms| 2145|  738255|
|25-75 load+run | 160.223ms+15.283ms| 2124|628571|
|50-50 load+run | 129.86ms+11.825ms| 2108|785714|
|75-25 load+run | 132.291ms+11.735ms | 2097|769230|
|100-0 load+run | 135.117ms+11.26ms| 2084|  753424|

可见更改后总体运行时间稍微慢了10多ms左右，溢出页面数目没变

### 6.加分项



#### 溢出桶空间回收（10）

采用位图bitmap方式实现  
两个关键函数  
```C
size_t determine_location(size_t* arr);   //返回bitmap中第一个非零元素的位置  

void set_zero(size_t num,size_t* arr);  //将给定位置的元素在位图中置为1
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
```

采用了512*64共4K的空间存储位图，开始时全部置为0,每次要申请一个溢出空间时就调用determin_location获得第一个非零元素的位置，据此来申请溢出空间。每次释放溢出空间就调用set_zero将相应位置置为0.  
有测试结果可知，大概回收了三倍左右的溢出页面，效果良好。
