#include "gtfs.hpp"

#define VERBOSE_PRINT(verbose, str...) (verbose ? cout << "VERBOSE: "<< __FILE__ << ":" << __LINE__ << ":" << __func__ << "():" << str : 0)

int do_verbose;

gtfs_t* gtfs_init(string directory, int verbose_flag) {
	do_verbose = verbose_flag;
	gtfs_t *gtfs = NULL;
	VERBOSE_PRINT(do_verbose, "Initializing GTFileSystem inside directory " << directory << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.
	return gtfs;
}

int gtfs_clean(gtfs_t *gtfs) {
	int ret = -1;
	VERBOSE_PRINT(do_verbose, "Cleaning up GTFileSystem inside directory " << gtfs->dirname << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;
}

file_t* gtfs_open_file(gtfs_t* gtfs, string filename, int file_length) {
	file_t *fl = NULL;
	VERBOSE_PRINT(do_verbose, "Opening file " << filename << " inside directory " << gtfs->dirname << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.
	return fl;
}

int gtfs_close_file(gtfs_t* gtfs, file_t* fl) {
	int ret = -1;
	VERBOSE_PRINT(do_verbose, "Closing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;
}

int gtfs_remove_file(gtfs_t* gtfs, file_t* fl) {
	int ret = -1;
	VERBOSE_PRINT(do_verbose, "Removing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;	
	
}

char* gtfs_read_file(gtfs_t* gtfs, file_t* fl, int offset, int length) {
	char* ret_data = NULL;
	VERBOSE_PRINT(do_verbose, "Reading " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns pointer to data read.
	return ret_data;	
}

write_t* gtfs_write_file(gtfs_t* gtfs, file_t* fl, int offset, int length, const char* data) {
	write_t *write_id = NULL;
	VERBOSE_PRINT(do_verbose, "Writting " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns non NULL.
	return write_id;	
}

int gtfs_sync_write_file(write_t* write_id) {
	int ret = -1;
	VERBOSE_PRINT(do_verbose, "Persisting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns number of bytes written.
	return ret;	
}

int gtfs_abort_write_file(write_t* write_id) {
	int ret = -1;
	VERBOSE_PRINT(do_verbose, "Aborting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
	//TODO: Any additional initializations and checks
	
	VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
	return ret;	
}
