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
    if (b != NULL){
        delete [] b;
    }
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

    auto * const data = new uint16_t[MaxRawDataFrameQueueSize * MaxRawDataFrameSize * 10];
    uint64_t datapt = 0;


    for (int index = start_index; index <= end_index; index++) {
        RawDataFrame *dataFrame = obj.getRawDataFrame(index);
        int events = dataFrame->getNEvents();
        if(events == 0) {
            continue; // skip the empty ones
        }

        for(int i = 0; i<events; i++){
            data[datapt+(i*10)] = dataFrame->getEFine(i);
            data[datapt+((i*10)+1)] = dataFrame->getTFine(i);
            data[datapt+((i*10)+2)] = dataFrame->getECoarse(i);
            data[datapt+((i*10)+3)] = dataFrame->getTCoarse(i);
            data[datapt+((i*10)+4)] = dataFrame->getTacID(i);
            data[datapt+((i*10)+5)] = dataFrame->getChannelID(i);

            uint64_t frame_id = dataFrame->getFrameID();
            data[datapt+((i*10)+6)] =  (frame_id & 0xFFFF);
            data[datapt+((i*10)+7)] =  frame_id  >> 16 & 0XFFFF;
            data[datapt+((i*10)+8)] =  frame_id >> 32 & 0XFFFF;
            data[datapt+((i*10)+9)] =  frame_id  >> 48 & 0XFFFF;



        }

        datapt += events*10;

    }


    boost::python::tuple shape = boost::python::make_tuple(datapt);
    boost::python::tuple stride = boost::python::make_tuple(sizeof(uint16_t));
    np::dtype dt = np::dtype::get_builtin<uint16_t>();

    // This sets up a python object whose destruction will free the data
    PyObject *capsule = ::PyCapsule_New((void *)data, NULL, (PyCapsule_Destructor)&destroyManagerCObject);
    boost::python::handle<> h_capsule{capsule};
    boost::python::object owner_capsule{h_capsule};
    return np::from_data(data, dt, shape, stride, owner_capsule);

};








struct unpacked_event_t {
	unsigned long long frameID;
	unsigned int channelID;
	unsigned short tacID;
	unsigned short tCoarse;
	unsigned short eCoarse;
	unsigned short tFine;
	unsigned short eFine;
};

static object events_as_bytes(SHM_RAW &self, int start, int end)
{
	auto shm_raw_size = self.getSizeInFrames();
	int total_events = 0;
	for(auto ii = start; ii < end; ii++) {
		auto index = ii % shm_raw_size;
		total_events += self.getNEvents(index);
	}

	auto buffer_size = total_events * sizeof(unpacked_event_t);
	unpacked_event_t * buf = (unpacked_event_t *)malloc(buffer_size);
	unpacked_event_t *p = buf;
	for(auto ii = start; ii < end; ii++) {
		auto index = ii % shm_raw_size;

		unsigned long long frameID = self.getFrameID(index);
		auto n_events = self.getNEvents(index);
		for(auto j = 0; j < n_events; j++) {
			p->frameID = frameID;
			p->channelID = self.getChannelID(index, j);
			p->tacID = self.getTacID(index, j);
			p->tCoarse = self.getTCoarse(index, j);
			p->eCoarse = self.getECoarse(index, j);
			p->tFine = self.getTFine(index, j);
			p->eFine = self.getEFine(index, j);
			p++;
		}
	}

	PyObject* py_buf = PyBytes_FromStringAndSize((char *)buf, buffer_size);
	object retval = object(handle<>(py_buf));
	return retval;
}

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
		.def("events_as_bytes", &events_as_bytes, (arg("self"), arg("start"), arg("end")))
        .def("getNumpyFrame", getNumpyFrame)
        .def("getNumpyFrameBlock", getNumpyFrameBlock)
	;
}
