
#include "metadata.c"

std::vector<string> get_root_dir () {
	std::vector<string> vec;
	string root_dir = SUPER_BLOCK -> read_rootDIR();
	unsigned int index = root_dir.find ('\n', 0);
	string line = root_dir.substr (0,index);
	root_dir = root_dir.substr (index+1,root_dir.size() - (index-1));
	int index2 = line.find ("->", 0);
	string filename = line.substr (0,index2);
	// string filename(strlen(filename111.c_str())+1,'\0');
	// strcpy (&(filename[0]), filename111.c_str());

	vec.push_back(filename);

	while ((index = root_dir.find ("\n", 0)) != string::npos) {
		line = root_dir.substr (0,index);
		root_dir = root_dir.substr (index+1,root_dir.size() - (index-1));
		index2 = line.find ("->", 0);
		filename = line.substr (0,index2);
		vec.push_back(filename);
	}
	return vec;
}
int32_t get_file_inode_num(string _filename) {
	string root_dir = SUPER_BLOCK -> read_rootDIR();
	unsigned int index = root_dir.find ('\n', 0);
	string line = root_dir.substr (0,index);
	root_dir = root_dir.substr (index+1,root_dir.size() - (index-1));
	int index2 = line.find ("->", 0);

	string filename = line.substr (0,index2);
	if (filename ==_filename) {
		string inode_num = line.substr (index2+2, line.size() - index2-1-index2-2);
		return (int32_t)stoi(inode_num);
	}

	while ((index = root_dir.find ("\n", 0)) != string::npos) {
		line = root_dir.substr (0,index);
		root_dir = root_dir.substr (index+1,root_dir.size() - (index-1));
		index2 = line.find ("->", 0);
		filename = line.substr (0,index2);
		if (filename ==_filename) {
			string inode_num = line.substr (index2+2, line.size() - index2-1-index2-2);
			return (int32_t)stoi(inode_num);
		}
	}

	return -1;
}
int32_t get_size (int32_t inode_num) {
	if (inode_num == -1) return 0;
	// cout << "**********" << inode_num<<"*"<<((SUPER_BLOCK->readInode(inode_num)).size)<<"*******\n";
	return ((SUPER_BLOCK->readInode(inode_num)).size);
}
static int lfs_getattr(const char *path, struct stat *stbuf)
{
	// Return file attributes. 
	// The "stat" structure is described in detail in the stat(2) manual page. 
	// For the given pathname, this should fill in the elements of the "stat" structure.
	// If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value. 
	// This call is pretty much required for a usable filesystem. 
	
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} else {
		std::vector<string> filenames = get_root_dir();
		for (string filename : filenames) {
			if (string(path+1).compare(filename) == 0) {
				stbuf->st_mode = S_IFREG | 0;
				// stbuf->st_mode = S_IFREG | 0444;
				stbuf->st_nlink = 1;
				stbuf->st_size = get_size(get_file_inode_num(filename));
				return 0;
			}
		}
		// cout << "**********" << path<<"*"<<filename<<"****\n";
		return -ENOENT;
	}
}

static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	// Return one or more directory entries (struct dirent) to the caller.
	// It is related to, but not identical to, the readdir(2) and getdents(2) system calls, and readdir(3)

	// It starts at a given offset and returns results in a caller-supplied buffer.
	// However, the offset not a byte offset, and the results are a series of struct dirents rather than being uninterpreted bytes. 
	// "filler" function will help you put things into the buffer.

 //    Find the first directory entry following the given offset (see below).

 //    Call the filler function with arguments of buf, the null-terminated filename, 
 //    the address of your struct stat (or NULL if you have none), and the offset of the next directory entry.
 //    If filler returns nonzero, or if there are no more files, return 0.
 //    Find the next file in the directory.
 //    Go back to step 2. 

 //    From FUSE's point of view, the offset is an uninterpreted off_t (i.e., an unsigned integer). 
 //    You provide an offset when you call filler, and it's possible that such an offset 
 //    might come back to you as an argument later. 
 //    Typically, it's simply the byte offset (within your directory layout) of the directory entry, but it's really up to you.

 //    It's also important to note that readdir can return errors in a number of instances;
 //    in particular it can return -EBADF if the file handle is invalid, or -ENOENT if you use
 //    the path argument and the path doesn't exist. 
	
	// (void) offset;
	// (void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	std::vector<string> filenames = get_root_dir();
	for (string filename : filenames) {
		filler(buf, filename.c_str(), NULL, 0);
	}

	return 0;
}

static int lfs_open(const char *path, struct fuse_file_info *fi)
{
	// Open a file. If you aren't using file handles, 
    // this function should just check for existence and permissions and return either success or an error code.
	
	// if ((fi->flags & 3) != O_RDONLY)
	// 	return -EACCES;

	std::vector<string> filenames = get_root_dir();
	for (string filename : filenames)
		if (string(path+1).compare(filename) == 0)
			return 0;

	// cout << "**********" << path<<"****\n\n\n\n\n";
	return -ENOENT;
}
string read_data(int32_t inode_num, int32_t size, int32_t offset) {
	stringstream data_stream;
	inode root = SUPER_BLOCK -> readInode(inode_num);
	int32_t block_num = 0;
	int32_t curr_off = 0;
	while (true) {
		curr_off += root.usage[block_num];
		if (curr_off < offset)
			++block_num;
		else
			break;
	}

	int data_read = 0;
	while (data_read < size) {
		if (root.pointers[block_num] < 0) 
			break;
		string buff(root.usage[block_num], '\0');
		SUPER_BLOCK -> readBlock (root.pointers[block_num], &(buff[0]), 1, root.usage[block_num]);
		data_stream << buff.substr (0,root.usage[block_num]);
		data_read += root.usage[block_num];
		++block_num;
	}
	string res = data_stream.str();
	return res;
}
static int lfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	// Read size bytes from the given file into the buffer buf,
	// beginning offset bytes into the file. 
	// See read(2) for full details. Returns the number of bytes transferred, 
	// or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem. 
	if (lfs_open(path, fi) == -ENOENT)
		return -ENOENT;
	int32_t inode_num = get_file_inode_num(string(path+1));
	if (inode_num < 0) return -ENOENT;
	int32_t filesize = get_size(inode_num);

	if (offset >= filesize) return 0;
	if (offset + size > filesize) size = filesize - offset;
	string res = read_data(inode_num, size, offset);
	size = 0;
	for (unsigned int i = 0; i < res.size(); ++i) {
		buf[i] = res[i];
		if (buf[i] == '\0') break;
		size++;
		// cout << (int)buf[i]<< "*******"<<endl;
	}


	return size;
}
int32_t write_data(int32_t inode_num, const char *buf, int32_t size, int32_t offset) {
	inode node = SUPER_BLOCK -> readInode(inode_num);

	int32_t block_num = 0;
	int32_t curr_off = 0;
	// check if modifying existing block or appending to file
	while (true) {
		curr_off += node.usage[block_num];
		if (node.usage[block_num] == -1) ++curr_off;
		if (curr_off < offset)
			++block_num;
		else break;
	}
	int32_t data_written = 0;
	while (data_written < size) {
		if (node.usage[block_num] != -1) {
			// copy old data and append new data
			++block_num;
		}
		// just write new data
		int32_t last_block = SUPER_BLOCK -> LAST_BLOCK();
		int32_t data_to_write = size - data_written;

		
		int32_t data_in_this_block;
		if ((data_to_write/block_size) > 0) {
			data_in_this_block = block_size;
			data_written += data_in_this_block;
		} else {
			data_in_this_block = data_to_write;
			data_written += data_in_this_block;
		}
		string buff(block_size, '\0');
		for (int i = 0; i < data_in_this_block; ++i) {
			buff[i] = buf[i];
		}
	// cout << "\n\n\n\n\n***************";
	// cout << "write "<<buf<<" to block "<<block_num<<" datasize = "<< data_in_this_block<<" data = "<< buff;
	// cout << "***************\n\n\n\n\n";
		SUPER_BLOCK -> writeBlock (last_block, (void*)buf, 1, block_size);
		buf += data_in_this_block;
		node.usage[block_num] = data_in_this_block;
		node.pointers[block_num] = last_block;
		++block_num;
	}
	node.write();
	SUPER_BLOCK -> inode_map[inode_num] = node.block_number;
	SUPER_BLOCK -> write(LFS_FILE);
	return data_written;
}
static int lfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // As for read above, except that it can't return 0. 
 //    cout << "\n\n\n\n\n***************";
	// cout << "write "<<buf<<" to "<<path<< " bytes";
	// cout << "***************\n\n\n\n\n";

 	int32_t inode_num = get_file_inode_num(string(path+1));
	if (inode_num < 0) return -ENOENT;

	int32_t data_written = write_data(inode_num, buf, size, offset);
	return data_written;
}
// unlink(const char* path)

 // rename(const char* from, const char* to) 
 // Note that the source and target don't have to be in the same directory, 
 // so it may be necessary to move the source to an entirely new directory. See rename(2) for full details. 
void truncate(int32_t inode_num, int32_t offset) {
	inode node = SUPER_BLOCK -> readInode(inode_num);

	int32_t block_num = 0;
	int32_t curr_off = 0;
	int32_t last_used_block_num = 0;
	int32_t last_used_data_offset = 0;
	int32_t last_block_starting_offset = 0;
	// check if modifying existing block or appending to file
	while (true) {
		if (node.usage[block_num] == -1) {
			curr_off += block_size;
		} else {
			curr_off += node.usage[block_num];
			last_used_data_offset += node.usage[block_num];
			++last_used_block_num;
		}

		if (curr_off < offset) {
			++block_num;
		} else {
			last_block_starting_offset = last_used_data_offset - node.usage[block_num];
			break;
		}
	}
	if (last_used_block_num < block_num) {  // increase size
		int32_t data_to_write = offset - last_used_data_offset;
		int32_t data_written = 0;
		++block_num;  // new block

		while (data_written < data_to_write) {
			int32_t last_block = SUPER_BLOCK -> LAST_BLOCK();
			string buff(block_size, '\0');

			int32_t data_in_this_block;
			if ((data_to_write/block_size) > 0) {
				data_in_this_block = block_size;
				data_written += data_in_this_block;
			} else {
				data_in_this_block = data_to_write;
				data_written += data_in_this_block;
			}
			node.usage[block_num] = -1;
			node.pointers[block_num] = last_block;
			SUPER_BLOCK -> writeBlock (last_block, (void*)&(buff[0]), 1, block_size);
			++block_num;
		}
		node.write();
		SUPER_BLOCK -> inode_map[inode_num] = node.block_number;
		SUPER_BLOCK -> write(LFS_FILE);
	} else if (last_used_block_num >= block_num) {  // decrease size
		// copy partial data from block
		string old_data = read_data(inode_num, offset - last_block_starting_offset, last_block_starting_offset);
		string buff(block_size, '\0');
		for (unsigned int i = 0; i < old_data.size(); ++i) {
			buff[i] = old_data[i];
		}
		node.usage[block_num] = offset - last_block_starting_offset;
		int32_t last_block = SUPER_BLOCK -> LAST_BLOCK();
		node.pointers[block_num] = last_block;
		SUPER_BLOCK -> writeBlock (last_block, (void*)&(buff[0]), 1, block_size);

		for (int i = block_num+1; i < pointer_per_inode; ++i) {
			node.usage[block_num] = -1;
			node.pointers[block_num] = -1;
		}

		node.write();
		SUPER_BLOCK -> inode_map[inode_num] = node.block_number;
		SUPER_BLOCK -> write(LFS_FILE);
	}
}
static int lfs_truncate(const char* path, off_t size) {
// Truncate or extend the given file so that it is precisely size bytes long. See truncate(2) for details. 
// This call is required for read/write filesystems, because recreating a file will first truncate it.
 	int32_t inode_num = get_file_inode_num(string(path+1));
	if (inode_num < 0) return -ENOENT;
	truncate(inode_num, size);
	// cout << "\n\n\n\n\n***************";
	// cout << "truncate"<<path<<" to "<<size<< " bytes";
	// cout << "***************\n\n\n\n\n";
	return 0;
}
static int lfs_create (const char * path, mode_t mode, struct fuse_file_info * fi) {
	// cout << "\n\n\n\n\n***************";
	// cout << "create"<<path;
	// cout << "\n" << SUPER_BLOCK -> read_rootDIR();
	// cout << "***************\n\n\n\n\n";
	std::vector<string> filenames = get_root_dir();
	for (string filename : filenames) {
		if (string(path+1).compare(filename) == 0) {
			return -EEXIST;
		}
	}
	int32_t inode_num = SUPER_BLOCK -> get_freeI_num();
	inode node(inode_num);
	node.write();

	SUPER_BLOCK -> inode_map[inode_num] = node.block_number;
	SUPER_BLOCK -> write(LFS_FILE);

	string old_root_dir = SUPER_BLOCK -> read_rootDIR();
	stringstream str;
	str << old_root_dir << string(path+1) << "->" << inode_num << "\n";

	int32_t* root_file_block_info = SUPER_BLOCK -> write_rootDIR(str.str());

	inode root_inode(root_inode_number);
	root_inode.set_pointer_to (root_file_block_info);
	root_inode.set_usage_to (root_file_block_info+pointer_per_inode);
	root_inode.write();
	SUPER_BLOCK -> inode_map[root_inode_number] = root_inode.block_number;
	SUPER_BLOCK -> write(LFS_FILE);
	// cout << "\n\n\n\n\n***************";
	// cout << "create"<<path;
	// cout << "\n" << SUPER_BLOCK -> read_rootDIR();
	// cout << "***************\n\n\n\n\n";
	return 0;
}
static int lfs_rename(const char *old_path , const char *new_path) {
	std::vector<string> filenames = get_root_dir();
	bool exists = false;
	for (string filename : filenames) {
		if (string(old_path+1).compare(filename) == 0) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		return -2;
	}
	string old_root_dir = SUPER_BLOCK -> read_rootDIR();
	// replace
	unsigned int index = 0;
	if((index = old_root_dir.find (string(old_path+1), 0)) != string::npos) {
		int32_t len = string(old_path+1).size();
		old_root_dir.replace(index,len,string(new_path+1));
	}

	int32_t* root_file_block_info = SUPER_BLOCK -> write_rootDIR(old_root_dir);

	inode root_inode(root_inode_number);
	root_inode.set_pointer_to (root_file_block_info);
	root_inode.set_usage_to (root_file_block_info+pointer_per_inode);
	root_inode.write();
	SUPER_BLOCK -> inode_map[root_inode_number] = root_inode.block_number;
	SUPER_BLOCK -> write(LFS_FILE);
	return 0;
}
static int lfs_unlink(const char* old_path) {
	std::vector<string> filenames = get_root_dir();
	bool exists = false;
	for (string filename : filenames) {
		if (string(old_path+1).compare(filename) == 0) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		return -2;
	}
	string old_root_dir = SUPER_BLOCK -> read_rootDIR();
	// replace
	unsigned int index = 0;
	unsigned int index2 = 0;
	if((index = old_root_dir.find (string(old_path+1), 0)) != string::npos) {
		if((index2 = old_root_dir.find (string("\n"), index)) != string::npos) {
			int32_t len = index2 - index;  // string(old_path+1).size();
			old_root_dir.replace(index,len+1,"");
		}
	}

	int32_t* root_file_block_info = SUPER_BLOCK -> write_rootDIR(old_root_dir);

	inode root_inode(root_inode_number);
	root_inode.set_pointer_to (root_file_block_info);
	root_inode.set_usage_to (root_file_block_info+pointer_per_inode);
	root_inode.write();
	int32_t inode_num = get_file_inode_num(string(old_path+1));
	SUPER_BLOCK -> inode_map[inode_num] = -1;
	SUPER_BLOCK -> inode_map[root_inode_number] = root_inode.block_number;
	SUPER_BLOCK -> write(LFS_FILE);
	return 0;
} 
int main(int argc, char *argv[]) {
	lfs_operations.getattr	= lfs_getattr;
	lfs_operations.readdir	= lfs_readdir;
	lfs_operations.open		= lfs_open;
	lfs_operations.read		= lfs_read;
	lfs_operations.write    = lfs_write;
	lfs_operations.truncate = lfs_truncate;
	lfs_operations.create   = lfs_create;
	lfs_operations.rename   = lfs_rename;
	lfs_operations.unlink 	= lfs_unlink;
	umask(0);
	// umask(077);
	interface(argv);	
    return fuse_main(argc-1, argv, &lfs_operations, NULL);
}


