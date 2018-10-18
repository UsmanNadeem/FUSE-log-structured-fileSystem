#include <fuse.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <math.h>
using namespace std;

#define root_inode_number 0
#define block_size 1024  // 1 KB
#define inode_map_size 1024  // 1000 max files/inodes
#define block_number_bits 32  // 4 byte integer = 32 bits  2^32blocks * 1024bytes= 4TB max disc space or 2 tb if 31 bit
#define max_file_size 30*1024  // 30KB
#define pointer_per_inode max_file_size/block_size  // 30 pointers
// pointer is 4 bytes so a single inode can have 256 pointers
class inode {
public:
	int32_t pointers[pointer_per_inode];  // block of file to block on disk
	int32_t usage[pointer_per_inode];
	int32_t block_number;
	int32_t size;
	unsigned int inode_number;
	inode(int i);
	inode (int32_t i, bool readfromfile);
	void write();
	void set_pointer_to (int32_t* data);
	void set_usage_to (int32_t* data);
};
class superblock  // block 0 till 3
{
public:
	int32_t inode_map[inode_map_size];  // inode number to block number which contains inode
	superblock();
	superblock (string filename);
	void write(string filename);
	inode readInode (int32_t i_num);
	void readBlock (int32_t block_num, void* buff, int32_t byte_per_index, int32_t number_of_times);
	void write();
	void writeBlock (int32_t block_num, void* buff, int32_t byte_per_index, int32_t number_of_times);
	void write_newInode (int32_t i_num, int32_t* data);
	int32_t LAST_BLOCK();

	int32_t get_freeI_num() {
		for (int i = 1; i < inode_map_size; ++i)
			if (inode_map[i] == -1)
				return i;
		return -1;
	}
	int32_t* write_rootDIR(string text) {  // returns the block locations
		int32_t* pointers = new int32_t[pointer_per_inode*2];
		for (int i = 0; i < pointer_per_inode*2; ++i) { pointers[i] = -1; }
		// stringstream str;
		// str << "root.txt->0\n";
		// str << "hello->5\n";
		// str << "hello->1";
		// string text = str.str();
		// cout << text.size()<< "*******"<<endl;
		unsigned int actual_size = text.size();
		while ((text.size()%block_size) != 0) { text += '\0';}

		int i = 0;
		for (unsigned int j = 0; j < text.size(); j+=block_size,++i) {
			
			int32_t block_num = LAST_BLOCK();
			writeBlock (block_num, &(text[j]), 1, block_size);
			pointers[i] = block_num;

			if ((j+block_size) <= actual_size) {
				pointers[i+pointer_per_inode] = block_size;
			} else {
				pointers[i+pointer_per_inode] = actual_size % text.size();
			}
		}
		// cout << "\n\n\n\n\n***************write\n";
		// cout << text.size()<<endl;
		// cout << text;
		// cout << "***************\n\n\n\n\n";
		return pointers;
	}
	string read_rootDIR() {
		stringstream data_stream;
		inode root = readInode(root_inode_number);
		for (int i = 0; i < pointer_per_inode; ++i) {
			if (root.pointers[i] < 0) break;
			char buff[root.usage[i]+1];
			for (int i = 0; i < root.usage[i]+1; ++i) {
			 	buff[i] = '\0';
			}
			readBlock (root.pointers[i], buff, root.usage[i], 1);
			data_stream << string(buff).substr (0,root.usage[i]);
		}
		// cout << "\n\n\n\n\n***************read\n";
		// cout << root.usage[0]<< " " << data_stream.str().size()<<endl;
		// cout << data_stream.str();
		// cout << "***************\n\n\n\n\n";
		return data_stream.str();
	}
};


static string current_path = get_current_dir_name();
static struct fuse_operations lfs_operations;
static superblock* SUPER_BLOCK;
static string LFS_FILE;
int32_t superblock::LAST_BLOCK() {
	FILE* file_p = fopen (LFS_FILE.c_str(), "rb+");
	fseek (file_p, 0, SEEK_END);
	return ftell (file_p)/block_size;
}
inode::inode(int i) {
	inode_number = i;
	for (int j = 0; j < pointer_per_inode; ++j) {
		pointers[j] = -1;
		usage[j] = -1;
	}
	block_number = SUPER_BLOCK->LAST_BLOCK();
}
// read inode from file
inode::inode (int32_t i, bool readfromfile) {
	for (int j = 0; j < pointer_per_inode; ++j) {
		pointers[j] = -1;
		usage[j] = -1;
	}
	block_number = SUPER_BLOCK->inode_map[i];
	if (block_number == -1) return;
	int32_t buff[pointer_per_inode*2] = {-1};

	inode_number = i;
	SUPER_BLOCK->readBlock (block_number, buff, 4, pointer_per_inode*2);
	size = 0;
	for (int j = 0; j < pointer_per_inode; ++j) {
		pointers[j] = (int32_t)buff[j];
		usage[j] = (int32_t)buff[j+pointer_per_inode];
		if (pointers[j] != -1 && usage[j] != -1) {
			// cout << "*******************"<<pointers[j]<<"*"<<usage[j]<<"*"<< "in read inode" <<endl;
			size = size + usage[j];
			// cout << "*******************"<<pointers[j]<<"*"<<usage[j]<<"*"<<size <<" in read inode" <<endl;
		} 
		else {break;}
	}
	// cout << "*************"<<inode_number<<"  "<< size/1024<<"  " <<endl;
}
void inode::set_pointer_to (int32_t* data) {
	for (int j = 0; j < pointer_per_inode; ++j) {
		pointers[j] = data[j];
	}
}
void inode::set_usage_to (int32_t* data) {
	for (int j = 0; j < pointer_per_inode; ++j) {
		usage[j] = data[j];
	}
}

void inode::write() {
	int32_t buff[(block_size/4)] = {-1};
	for (int j = 0; j < pointer_per_inode; ++j) {
		buff[j] = pointers[j];
		buff[j+pointer_per_inode] = usage[j];
	}
	block_number = SUPER_BLOCK->LAST_BLOCK();
	SUPER_BLOCK -> writeBlock (block_number, buff, 4, block_size/4);
}

superblock::superblock() {
	for (int j = 0; j < inode_map_size; ++j) {
		inode_map[j] = -1;
	}
	write(LFS_FILE);

	stringstream str;
	str << "root.txt->0\n";
	// str << "hello->5\n";
	int32_t* root_file_block_info = SUPER_BLOCK -> write_rootDIR(str.str());

	inode root_inode(root_inode_number);
	root_inode.set_pointer_to (root_file_block_info);
	root_inode.set_usage_to (root_file_block_info+pointer_per_inode);
	root_inode.write();

	inode_map[root_inode_number] = root_inode.block_number;

	write(LFS_FILE);
}
superblock::superblock (string filename) {  // load inode_map from disk
	FILE* file_p = fopen (LFS_FILE.c_str(), "rb+");
	if (file_p == NULL) {
		cout << "File does not exist\n" << endl; exit(0);
	} else {
		cout << "Reading superblock from "<< filename <<"\n" << endl;
	}
	fseek (file_p, 0, SEEK_SET);

	for (int j = 0; j < inode_map_size; ++j) {
		// char buff[500];
		// fgets (buff, 500, file_p);
		// buff[strlen(buff)-1] = 0;
		fread ((void*)&(inode_map[j]), 4, 1, file_p);
	}
	fclose(file_p);	
}
void superblock::write(string filename) {  // write inode_map to disk
	FILE* file_p = fopen (filename.c_str(), "rb+");
	if (file_p == NULL) {
		cout << "File does not exist\n" << endl; exit(0);
	}
	fseek (file_p, 0, SEEK_SET);

	for (int j = 0; j < inode_map_size; ++j) {
		fwrite ((void*)&(inode_map[j]), 4, 1, file_p);
		// fputs (to_string(inode_map[j]).c_str(), file_p);
		// fputs ("\n", file_p);
	}
	fclose(file_p);
}
inode superblock::readInode (int32_t i_num) {
	return inode(i_num, true);
}
void superblock::readBlock (int32_t block_num, void* buff, int32_t byte_per_index, int32_t number_of_times) {
	FILE* file_p = fopen (LFS_FILE.c_str(), "rb+");
	fseek (file_p, block_size*block_num, SEEK_SET);
	fread (buff, byte_per_index, number_of_times, file_p);  // n byte long, m times
	fclose(file_p);
}
// void superblock::write_newInode (int32_t i_num, int32_t* data) {
// 	inode this_inode(i_num);
// 	inode_map[i_num] = this_inode.block_number;

// 	this_inode.set_pointer_to(data);
// 	this_inode.write();

// 	write(LFS_FILE);
// }

void superblock::writeBlock (int32_t block_num, void* buff, int32_t byte_per_index, int32_t number_of_times) {
	FILE* file_p = fopen (LFS_FILE.c_str(), "rb+");
	fseek (file_p, block_size*block_num, SEEK_SET);
	fwrite (buff, byte_per_index, number_of_times, file_p);
	fclose(file_p);	
}

void interface(char *argv[]) {
	if (mkdir(argv[3], S_IRWXU) == -1) {
		if (errno != EEXIST) {
			cout << "error creating lfs_mount_dir\n";
			exit(0); 
		}
	}

	string filename = ( current_path + "/" + string(argv[4]) );
	LFS_FILE = filename;
	// string filename = ( string(argv[3]) + "/" + string(argv[4]) );
	ifstream rdfile(filename.c_str());
	if ( ! rdfile.good() ) {
		rdfile.close();
		cout << "Creating file "<< filename << endl;
		ofstream outfile(filename.c_str());
		if (! outfile.good() ) {
			cout << "File could not be created" << endl; exit(0);
		} else {
			outfile.close();
			SUPER_BLOCK = new superblock();
			// SUPER_BLOCK->write(filename);
		}
	} else {
		SUPER_BLOCK = new superblock (filename);
		// ofstream("abc.txt");
		// SUPER_BLOCK->write(string("abc.txt"));
		rdfile.close();
	}
}

