#include "gtfs.hpp"

#define VERBOSE_PRINT(verbose, str...) do { \
	if (verbose) cout << "VERBOSE: "<< __FILE__ << ":" << __LINE__ << ":" << __func__ << "():" << str; \
	} while(0)

int do_verbose;

gtfs_t* gtfs_init(string directory, int verbose_flag) {
	do_verbose = verbose_flag;
	gtfs_t *gtfs = NULL;
	VERBOSE_PRINT(do_verbose, "Initializing GTFileSystem inside directory " << directory << "\n");
	//TODO: Any additional initializations and checks
	// obtain the shared memory id and semaphore id of metadata
	// if IPC structures don't exist, create them
	gtfs = (gtfs_t*) malloc(sizeof(gtfs_t));
	gtfs->dirname = directory;
	key_t dir_key = ftok(directory.c_str(), 1);
	int sem_id = semget(dir_key, 1, 0666 | IPC_CREAT);
	int shm_id = shmget(dir_key, sizeof(int), 0666 | IPC_CREAT);
	// create log file for current directory and write the initial tail position
	// we keep the log file open until a clean() operation happens
	int log_fd = open(directory.c_str(), O_RDWR|O_CREAT);
	struct sembuf op1, op2;
	op1.sem_num = 0;
	op1.sem_op = -1;
	op1.sem_flg = 0;
	op2.sem_num = 0;
	op2.sem_op = 1;
	op2.sem_flg = 0;
	semop(sem_id, &op1, 1);
	char* log_data = (char*) shmat(shm_id, NULL, 0);
	int init_tail = 0;
	memcpy(log_data, &init_tail, sizeof(int));
	shmdt(log_data);
	semop(sem_id, &op2, 1);

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
	// persist the changes in log file to files under this directory
	
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
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
	const char* pathname = (gtfs->dirname + filename).c_str();
	int fd = open(pathname, O_RDWR|O_CREAT);
	if (fd == -1) {
		printf("failed to open file %s\n", pathname);
		return fl;
	}
	lockf(fd, F_LOCK, 0);

	// fail the operation if file_length arg is smaller than the actual file size
	// adjust file size if needed if file_length arg is greater than the actual file size
	struct stat statbuf;
	stat(pathname, &statbuf);
	if (file_length < statbuf.st_size) {
		printf("failed to truncate file into smaller file\n");
		return fl;
	}
	else if (file_length > statbuf.st_size) {
		char cmd[50];
		sprintf(cmd, "truncate -s %d %s", file_length, (gtfs->dirname + filename).c_str());
		system(cmd);
	}

	fl = (file_t*) malloc(sizeof(file_t));
	fl->filename = filename;
	fl->fd = fd;
	fl->file_length = file_length;
	char file_data[file_length];
	read(fd, file_data, file_length);
	fl->data = file_data;

	// apply the changes in log file to in-memory version of data
	int filename_length;
	while (read(gtfs->log_fd, &filename_length, sizeof(int)) > 0) {
		char fname[filename_length];
		read(gtfs->log_fd, fname, filename_length);
		int offset, data_length;
		read(gtfs->log_fd, &data_length, sizeof(int));
		char data[data_length], valid;
		read(gtfs->log_fd, data, data_length);
		read(gtfs->log_fd, &valid, 1);
		if (strcmp(fname, filename.c_str()) == 0 && valid == 1) memcpy(fl->data + offset, data, data_length);
	}
	
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
	free(fl);
	
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
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
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
	ret_data = fl->data;
	
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
	write_id = (write_t*) malloc(sizeof(write_id));
	write_id->filename = fl->filename;
	write_id->offset = offset;
	write_id->length = length;
	char old_data[length], data_copy[length];
	memcpy(old_data, fl->data, offset);
	write_id->old_data = old_data;
	write_id->data = strcpy(data_copy, data);
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
	struct sembuf op1, op2;
	op1.sem_num = 0;
	op1.sem_op = -1;
	op1.sem_flg = 0;
	op2.sem_num = 0;
	op2.sem_op = 1;
	op2.sem_flg = 0;
	semop(write_id->sem_id, &op1, 1);
	// update tail position of log file
	char* shm_data = (char*) shmat(write_id->shm_id, NULL, 0);
	int log_tail = stoi(shm_data);
	int advanced_length = 3*sizeof(int) + write_id->length + write_id->filename.size() + 1, new_tail = stoi(shm_data) + advanced_length; // 1 is valid flag
	memcpy(shm_data, &new_tail, sizeof(int));
	// write a new redo log record <filename_length, filename, offset, data_length, data, valid>
	char* log_data = (char*) mmap(NULL, log_tail + advanced_length, PROT_WRITE, MAP_SHARED, write_id->log_fd, 0);
	int filename_length = write_id->filename.size();
	char filename_copy[filename_length], valid = 1;
	int pos = log_tail;
	memcpy(log_data + pos, &filename_length, sizeof(int));
	pos += sizeof(int);
	memcpy(log_data + pos, strcpy(filename_copy, write_id->filename.c_str()), filename_length);
	pos += filename_length;
	memcpy(log_data + pos, &(write_id->offset), sizeof(int));
	pos += sizeof(int);
	memcpy(log_data + pos, &(write_id->length), sizeof(int));
	pos += sizeof(int);
	memcpy(log_data + pos, write_id->data, write_id->length);
	pos += write_id->length;
	memcpy(log_data + pos, &valid, 1);
	
	munmap(log_data, new_tail);
	shmdt(shm_data);
	semop(write_id->sem_id, &op2, 1);
	free(write_id);
	
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
	free(write_id);
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;	
}

