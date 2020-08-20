#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
using namespace boost::python;

#include "shm_raw.hpp"
using namespace PETSYS;

inline void destroyArrayOwnerPtr(_object *p) {
   std::shared_ptr<PackedEventVec> *b = reinterpret_cast<std::shared_ptr<PackedEventVec>*>(p);
   delete b;
}

inline boost::python::object makeArrayOwner(std::shared_ptr<PackedEventVec> & x) {
    boost::python::handle<> h(PyCapsule_New(new std::shared_ptr<PackedEventVec>(x), NULL,
        &destroyArrayOwnerPtr));
    return boost::python::object(h);
}

numpy::ndarray frame2numpy(SHM_RAW& obj, int index) {
	// Get one frame of data, the PackedEventVec contains a continuous
	// region of structs with even packing. A shared_ptr must be used
	// here as otherwise C++ would free the Vector's memory once this
	// func returns.
	std::shared_ptr<PackedEventVec> frame = obj.getRawFrame(index);

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

BOOST_PYTHON_MODULE(shm_raw)
{
	numpy::initialize();

	class_<SHM_RAW>("SHM_RAW", init<std::string>())
		.def("getSizeInBytes", &SHM_RAW::getSizeInBytes)
		.def("getSizeInFrames", &SHM_RAW::getSizeInFrames)
		.def("getFrameSize", &SHM_RAW::getFrameSize)
		.def("getFrameWord", &SHM_RAW::getFrameWord)
		.def("getFrameID", &SHM_RAW::getFrameID)
		.def("getFrameLost", &SHM_RAW::getFrameLost)
		.def("getNEvents", &SHM_RAW::getNEvents)
		.def("getTCoarse", &SHM_RAW::getTCoarse)
		.def("getECoarse", &SHM_RAW::getECoarse)
		.def("getTFine", &SHM_RAW::getTFine)
		.def("getEFine", &SHM_RAW::getEFine)
		.def("getTacID", &SHM_RAW::getTacID)
		.def("getChannelID", &SHM_RAW::getChannelID)
		.def("getNumpyFrame", &frame2numpy)
	;
}
