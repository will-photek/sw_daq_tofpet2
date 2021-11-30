#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <iostream>
#include <algorithm>
#include <iterator>
using namespace boost::python;

namespace np = boost::python::numpy;

#include "shm_raw.hpp"
using namespace PETSYS;


inline void destroyManagerCObject(PyObject* self) {
    auto * b = PyCapsule_GetPointer(self, NULL);
    delete [] b;
}

np::ndarray getNumpyFrame(SHM_RAW& obj, int index) {
/**
 * Returns a numpy array representing a frame.
 * @param index the index of the frame
 * @return the numpy frame
 */
    RawDataFrame *dataFrame = obj.getRawDataFrame(index);


    auto * const data = new uint64_t[MaxRawDataFrameSize];
    std::copy(std::begin(dataFrame->data), std::end(dataFrame->data), data);


    boost::python::tuple shape = boost::python::make_tuple(dataFrame->getFrameSize());
    boost::python::tuple stride = boost::python::make_tuple(sizeof(uint64_t));
    np::dtype dt = np::dtype::get_builtin<uint64_t>();

    // This sets up a python object whose destruction will free the data
    PyObject *capsule = ::PyCapsule_New((void *)data, NULL, (PyCapsule_Destructor)&destroyManagerCObject);
    boost::python::handle<> h_capsule{capsule};
    boost::python::object owner_capsule{h_capsule};

    return np::from_data(data, dt, shape, stride, owner_capsule);

};

np::ndarray getNumpyFrameBlock(SHM_RAW& obj, int start_index, int end_index) {
/**
 * Returns a numpy array representing a block of frames, represented as an array of uint64_t to be decoded at a later point.
 *
 * @param index the index of the frame
 * @return the numpy frame
 */

    auto * const data = new uint64_t[MaxRawDataFrameSize * MaxRawDataFrameQueueSize]; // maybe too big?
    uint64_t datapt = 0;
    for (int index = start_index; index <= end_index; index++) {
        RawDataFrame *dataFrame = obj.getRawDataFrame(index);
        auto dataframe_start = std::begin(dataFrame->data);
        std::advance(dataframe_start, 2);  // +2 to ignore headers
        std::copy(dataframe_start, std::end(dataFrame->data), data + datapt);
        datapt += dataFrame->getFrameSize() - 2; // do not account for headers here
    }

    boost::python::tuple shape = boost::python::make_tuple(datapt);
    boost::python::tuple stride = boost::python::make_tuple(sizeof(uint64_t));
    np::dtype dt = np::dtype::get_builtin<uint64_t>();

    // This sets up a python object whose destruction will free the data
    PyObject *capsule = ::PyCapsule_New((void *)data, NULL, (PyCapsule_Destructor)&destroyManagerCObject);
    boost::python::handle<> h_capsule{capsule};
    boost::python::object owner_capsule{h_capsule};

    return np::from_data(data, dt, shape, stride, owner_capsule);

};


BOOST_PYTHON_MODULE(shm_raw)
{
    Py_Initialize();
    np::initialize();

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
        .def("getNumpyFrame", getNumpyFrame)
        .def("getNumpyFrameBlock", getNumpyFrameBlock)
	;
}
