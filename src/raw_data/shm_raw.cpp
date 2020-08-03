#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "shm_raw.hpp"

using namespace PETSYS;

SHM_RAW::SHM_RAW(std::string shmPath)
{
	shmfd = shm_open(shmPath.c_str(), 
			O_RDONLY, 
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (shmfd < 0) {
		fprintf(stderr, "Opening '%s' returned %d (errno = %d)\n", shmPath.c_str(), shmfd, errno );		
		exit(1);
	}
	shmSize = lseek(shmfd, 0, SEEK_END);
	assert(shmSize = MaxRawDataFrameQueueSize * sizeof(RawDataFrame));
	
	shm = (RawDataFrame *)mmap(NULL, 
				shmSize,
				PROT_READ, 
				MAP_SHARED, 
				shmfd,
				0);
}

SHM_RAW::~SHM_RAW()
{
	munmap(shm, shmSize);
	close(shmfd);
}

unsigned long long SHM_RAW::getSizeInBytes()
{
	assert (shmSize = MaxRawDataFrameQueueSize * sizeof(RawDataFrame));
	return shmSize;
}

numpy::ndarray frame2numpy(SHM_RAW& obj, int index) {
	// Get one frame of data, the PackedEventVec contains a continuous
	// region of structs with even packing. A shared_ptr must be used
	// here as otherwise C++ would free the Vector's memory once this
	// func returns.
	std::shared_ptr<RawDataFrame> frame = obj.getRawDataFrame(index);

	// Setup up numpy array parameters
	tuple shape = make_tuple(frame->size(), 7);
	tuple stride = make_tuple(sizeof(RawDataFrame), sizeof(uint16_t));
	numpy::dtype dt = numpy::dtype(str("u2"), false);

	// The created numpy array must have a way to indicate to C++ that
	// the created python object has been destroyed. To do this an empty
	// PyObject (or python::object to boost) is created, that when the
	// the numpy array's ref count in python reaches zero it calls a
	// destruction function (destroyArrayOwnerPtr), that deletes it's
	// reference to the shared_ptr, then C++ can decrement its reference
	// counter and free memory if required.
	object own = makeArrayOwner(frame);

	// Finally build the numpy array
	// frame is a uint64_t with methods now - this will need to be done differently.
	numpy::ndarray arr = numpy::from_data(frame->data(), dt, shape, stride, own);
	return arr;
}
