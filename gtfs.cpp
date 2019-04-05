#include "gtfs.hpp"

#define VERBOSE_PRINT(verbose, str...) do { \
	if (verbose) cout << "VERBOSE: "<< __FILE__ << ":" << __LINE__ << ":" << __func__ << "():" << str; \
	} while(0)

int do_verbose;
static struct sembuf op1, op2;

static void define_sem_op(struct sembuf* op1, struct sembuf* op2) {
	op1->sem_num = 0;
	op1->sem_op = -1;
	op1->sem_flg = 0;
	op2->sem_num = 0;
	op2->sem_op = 1;
	op2->sem_flg = 0;
}

gtfs_t* gtfs_init(string directory, int verbose_flag) {
	do_verbose = verbose_flag;
	gtfs_t *gtfs = NULL;
	VERBOSE_PRINT(do_verbose, "Initializing GTFileSystem inside directory " << directory << "\n");
	//TODO: Any additional initializations and checks
	// obtain the shared memory id and semaphore id of metadata
	key_t exist_key = ftok((directory + "/log").c_str(), 1);
	//printf("exist_key: %d\n", exist_key);
	gtfs = new gtfs_t;
	gtfs->dirname = string(directory);
	key_t dir_key = ftok(directory.c_str(), 1);
	int sem_id = semget(dir_key, 1, 0666 | IPC_CREAT);
	short one = 1;
	int shm_id = shmget(dir_key, (MAX_FILENAME_LEN + sizeof(int))*MAX_NUM_FILES_PER_DIR, 0666 | IPC_CREAT);
	// get or create log file for current directory and write the initial tail position
	// we keep the log file open until a clean() operation happens
	define_sem_op(&op1, &op2);
	int log_fd = open((directory + "/log").c_str(), O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
	//printf("log_fd: %d\n", log_fd);
	// if the log file is not created, then initialize the log tail to zero
	if (exist_key == -1) {
		stringstream ss;
		if (semctl(sem_id, 0, SETALL, &one) == -1) perror("In semctl()");
	}

	//cout << "sem_id: " << sem_id << ", shm_id: " << shm_id << endl;
	gtfs->sem_id = sem_id;
	gtfs->shm_id = shm_id;
	gtfs->log_fd = log_fd;

	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.
	return gtfs;
}

int gtfs_clean(gtfs_t *gtfs) {
	int ret = -1;
	if (gtfs) { 
		VERBOSE_PRINT(do_verbose, "Cleaning up GTFileSystem inside directory " << gtfs->dirname << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "GTFileSystem is not existed\n");
		return ret;
	}
	//TODO: Any additional initializations and checks
	semop(gtfs->sem_id, &op1, 1);
	lseek(gtfs->log_fd, 0, SEEK_SET);
	// persist the changes in log file to actual files under this directory
	int filename_length;
	while (read(gtfs->log_fd, &filename_length, sizeof(int)) > 0) {
		char* filename = new char[MAX_FILENAME_LEN];
		char* file_path = new char[(gtfs->dirname).size() + MAX_FILENAME_LEN + 1];
		read(gtfs->log_fd, filename, filename_length);
		strcpy(file_path, (gtfs->dirname + "/").c_str());
		strcat(file_path, filename); // file_path: "dirname/filename"
		//cout << "In gtfs_clean(), file path: " << file_path << endl;
		int offset, data_length;
		read(gtfs->log_fd, &offset, sizeof(int));
		read(gtfs->log_fd, &data_length, sizeof(int));
		char* data = new char[data_length];
		char valid;
		read(gtfs->log_fd, data, data_length);
		read(gtfs->log_fd, &valid, 1);
		//cout << "offset: " << offset << ", length: " << data_length << ", data: " << data << ", valid: " << (int) valid << endl;
		if (valid == 1) {
			struct stat statbuf;
			stat(file_path, &statbuf);
			int file_len = statbuf.st_size;
			int fd;
			string file_path_str(file_path);
			fd = open(file_path, O_RDWR);
			//cout << "fd: " << fd << endl;
			char* mapped_data = (char*) mmap(NULL, file_len, PROT_WRITE, MAP_SHARED, fd, 0);
			memcpy(mapped_data + offset, data, data_length);
			fsync(fd);
			munmap(mapped_data, file_len);
			close(fd);
		}
	}
	semop(gtfs->sem_id, &op2, 1);
	stringstream ss;
	ss << "truncate --size=0" << " " << gtfs->dirname << "/log";
	system(ss.str().c_str());

	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	ret = 0;
	return ret;
}

file_t* gtfs_open_file(gtfs_t* gtfs, string filename, int file_length) {
	file_t *fl = NULL;
	if (gtfs) {
		VERBOSE_PRINT(do_verbose, "Opening file " << filename << " inside directory " << gtfs->dirname << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "GTFileSystem is not existed\n");
		return NULL;
	}
	//TODO: Any additional initializations and checks
	// retieve (or create) the target file
	stringstream ss;
	ss << gtfs->dirname << "/" << filename;
	string filename_complete = ss.str();
	const char* pathname = filename_complete.c_str();
	int fd = open(pathname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	//cout << "file descriptor for " << pathname << " is: " << fd << endl;
	if (fd == -1) {
		perror("failed to open file");
		return fl;
	}
	lockf(fd, F_TLOCK, 0); // if another process acquire the lock, return an error

	// fail the operation if file_length arg is smaller than the actual file size
	// adjust file size if needed if file_length arg is greater than the actual file size
	struct stat statbuf;
	stat(pathname, &statbuf);
	if (file_length < statbuf.st_size) {
		perror("Cannot truncate file into smaller file\n");
		return fl;
	}
	else if (file_length > statbuf.st_size) {
		stringstream cmd_ss;
		cmd_ss << "truncate --size=" << file_length << " " << filename_complete;
		system(cmd_ss.str().c_str());
	}

	fl = new file_t;
	fl->filename = string(filename);
	fl->fd = fd;
	fl->file_length = file_length;
	fl->data = new char[file_length];
	read(fd, fl->data, file_length);

	// write a record into shared memory
	// apply the changes in log file to in-memory version of data
	semop(gtfs->sem_id, &op1, 1);
	int filename_length;
	off_t start_pos = lseek(gtfs->log_fd, 0, SEEK_SET);
	if (start_pos == -1) perror("In lseek() in gtfs_open_file");
	while (read(gtfs->log_fd, &filename_length, sizeof(int)) > 0) {
		//cout << "filename_length: " << filename_length << endl;
		char fname[MAX_FILENAME_LEN];
		ssize_t nr = read(gtfs->log_fd, fname, filename_length);
		if (nr == -1) perror("In read() (reading log file) in gtfs_open_file");
		//cout << "fl->filename: " << fl->filename << ", fname: " << string(fname) << endl;
		int offset, data_length;
		read(gtfs->log_fd, &offset, sizeof(int));
		read(gtfs->log_fd, &data_length, sizeof(int));
		char* data = new char[data_length];
		char valid;
		read(gtfs->log_fd, data, data_length);
		//cout << "offset: " << offset << ", length: " << data_length << ", data: " << data << endl;
		read(gtfs->log_fd, &valid, 1);
		//cout << "valid: " << (int) valid << endl;
		if (strcmp(fname, filename.c_str()) == 0 && valid == 1) {
			//cout << "found log related to " << fname << endl;
			memcpy(fl->data + offset, data, data_length);
		}
	}
	semop(gtfs->sem_id, &op2, 1);

	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.
	return fl;
}

int gtfs_close_file(gtfs_t* gtfs, file_t* fl) {
	int ret = -1;
	if (gtfs and fl) {
		VERBOSE_PRINT(do_verbose, "Closing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "GTFileSystem or file is not existed\n");
		return ret;
	}
	//TODO: Any additional initializations and checks
	lockf(fl->fd, F_ULOCK, 0);
	ret = close(fl->fd);
	if (ret == -1) perror("In close() in gtfs_close_file");
	semop(gtfs->sem_id, &op1, 1);
	int ret2 = close(gtfs->log_fd);
	semop(gtfs->sem_id, &op2, 1);
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;
}

int gtfs_remove_file(gtfs_t* gtfs, file_t* fl) {
	int ret = -1;
	if (gtfs and fl) {
	    VERBOSE_PRINT(do_verbose, "Removing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
	} else {
	    VERBOSE_PRINT(do_verbose, "GTFileSystem or file is not existed\n");
	    return ret;
	}
	//TODO: Any additional initializations and checks
	// acquire for semaphore so that the operation cannot be performed when the file is open
	stringstream ss1;
	ss1 << gtfs->dirname << "/" << fl->filename;
	string filepath = ss1.str();
	semop(gtfs->sem_id, &op1, 1);
	if (remove(filepath.c_str()) == -1) perror("In gtfs_remove_file");
	// traverse the log file to invalidate all the log records related to this file
	string logpath = gtfs->dirname + "/log";
	int log_fd = open(logpath.c_str(), O_RDWR);
	struct stat statbuf;
	stat(logpath.c_str(), &statbuf);
	int log_length = statbuf.st_size;
	char* log_data = (char*) mmap(NULL, log_length, PROT_WRITE, MAP_SHARED, log_fd, 0);
	if (log_data == MAP_FAILED) perror("In gtfs_remove_file(), when mapping log data");
	int pos = 0;
	while (pos < log_length) {
		int filename_length, offset, data_length;
		//cout << "pos: " << pos << endl;
		memcpy(&filename_length, log_data + pos, sizeof(int));
		pos += sizeof(int);
		char* fname = new char[MAX_FILENAME_LEN];
		memcpy(fname, log_data + pos, filename_length);
		pos += filename_length;
		memcpy(&offset, log_data + pos, sizeof(int));
		pos += sizeof(int);
		memcpy(&data_length, log_data + pos, sizeof(int));
		pos += sizeof(int);
		char* data = new char[data_length];
		char valid;
		memcpy(data, log_data + pos, data_length);
		pos += data_length;
		memcpy(&valid, log_data + pos, 1);
		//cout << "filename: " << fname << ", offset: " << offset << ", length: " << data_length << ", data: " << data << ", valid: " << (int) valid << endl;
		if ((fl->filename).compare(string(fname)) == 0 && valid == 1) {
			char invalid = 0;
			memcpy(log_data + pos, &invalid, 1);
		}
		pos += 1;
	}
	fsync(log_fd);
	if (munmap(log_data, log_length) == -1) perror("In gtfs_remove_file(), when unmapping log data");
	close(log_fd);
	delete fl;
	semop(gtfs->sem_id, &op2, 1);

	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	ret = 0;
	return ret;	
	
}

char* gtfs_read_file(gtfs_t* gtfs, file_t* fl, int offset, int length) {
	char* ret_data = NULL;
	if (gtfs and fl) {
		VERBOSE_PRINT(do_verbose, "Reading " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "GTFileSystem or file is not existed\n");
		return NULL;
	}
	//TODO: Any additional initializations and checks
	ret_data = new char[length];
	memcpy(ret_data, fl->data + offset, length);
	//cout << "ret_data: " << ret_data << endl;
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns pointer to data read.
	return ret_data;	
}

write_t* gtfs_write_file(gtfs_t* gtfs, file_t* fl, int offset, int length, const char* data) {
	write_t *write_id = NULL;
	if (gtfs and fl) {
		VERBOSE_PRINT(do_verbose, "Writting " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "GTFileSystem or file is not existed\n");
		return NULL;
	}
	//TODO: Any additional initializations and checks
	write_id = new write_t;
	if (write_id == 0) perror("In initialization of write_id");
	write_id->filename = fl->filename;
	write_id->offset = offset;
	write_id->length = length;
	write_id->old_data = new char[length];
	write_id->data = new char[length];
	memcpy(write_id->old_data, fl->data + offset, length);
	memcpy(write_id->data, data, length);
	memcpy(fl->data + offset, write_id->data, length);
	write_id->shm_id = gtfs->shm_id;
	write_id->sem_id = gtfs->sem_id;
	write_id->log_fd = gtfs->log_fd;
	write_id->file = fl; 
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.

	return write_id;	
}

int gtfs_sync_write_file(write_t* write_id) {
	int ret = -1;
	if (write_id) {
		VERBOSE_PRINT(do_verbose, "Persisting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "Write operation is not exist\n");
		return ret;
	}
	//TODO: Any additional initializations and checks
	semop(write_id->sem_id, &op1, 1);
	//off_t cur_pos = lseek(write_id->log_fd, 0, SEEK_CUR);
	//cout << "cur_pos: " << cur_pos << endl;
	int filename_length = strlen((write_id->filename).c_str());
	char valid = 1;
	//cout << "In gtfs_sync_write_file, filename: " << (write_id->filename).c_str() << endl;
	if (write(write_id->log_fd, &filename_length, sizeof(int)) == -1) perror("In gtfs_sync_write_file, when writing filename_length");
	if (write(write_id->log_fd, (write_id->filename).c_str(), filename_length) == -1) perror("In gtfs_sync_write_file, when writing filename");
	if (write(write_id->log_fd, &(write_id->offset), sizeof(int)) == -1) perror("In gtfs_sync_write_file, when writing offset");
	if (write(write_id->log_fd, &(write_id->length), sizeof(int)) == -1) perror("In gtfs_sync_write_file, when writing length");
	if (write(write_id->log_fd, write_id->data, write_id->length) == -1) perror("In gtfs_sync_write_file, when writing data");
	if (write(write_id->log_fd, &valid, 1) == -1) perror("In gtfs_sync_write_file, when writing valid bit");
	semop(write_id->sem_id, &op2, 1);
	delete write_id;
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns number of bytes written.
	return ret;	
}

int gtfs_abort_write_file(write_t* write_id) {
	int ret = -1;
	if (write_id) {
		VERBOSE_PRINT(do_verbose, "Aborting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
	} else {
		VERBOSE_PRINT(do_verbose, "Write operation is not exist\n");
		return ret;
	}
	//TODO: Any additional initializations and checks
	char* file_data = write_id->file->data;
	memcpy(file_data + write_id->offset, write_id->old_data, write_id->length);
	delete write_id;
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;	
}

