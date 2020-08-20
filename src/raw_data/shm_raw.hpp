#ifndef __PETSYS__SHM_RAW_HPP__DEFINED__
#define __PETSYS__SHM_RAW_HPP__DEFINED__

#include <stdint.h>
#include <string>
#include <event_decode.hpp>
#include <sys/mman.h>
#include <boost/python/numpy.hpp>
using namespace boost::python;

namespace PETSYS {
	
static const int MaxRawDataFrameSize = 2048;
static const unsigned MaxRawDataFrameQueueSize = 16*1024;


struct RawDataFrame {
	uint64_t data[MaxRawDataFrameSize];

	unsigned getFrameSize() {
		uint64_t eventWord = data[0];
		return (eventWord >> 36) & 0x7FFF;
	};
	

	unsigned long long getFrameID() {
		uint64_t eventWord = data[0];
		return eventWord & 0xFFFFFFFFFULL;
	};

	bool getFrameLost() {
		uint64_t eventWord = data[1];
		return (eventWord & 0x18000) != 0;
	};

	int getNEvents() {
		uint64_t eventWord = data[1];
		return eventWord & 0x7FFF;
	}; 
	
	unsigned getEFine(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getEFine();
	};

	unsigned getTFine(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getTFine();
	};

	unsigned getECoarse(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getECoarse();
	};
	
	unsigned getTCoarse(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getTCoarse();
	};
	
	unsigned getTacID(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getTacID();
	};
	
	unsigned getChannelID(int event) {
		RawEventWord rawEvent(data[event+2]);
		return rawEvent.getChannelID();
	};
	
};

typedef struct PackedEvent {
    uint16_t asic_id;
    uint16_t chan_id;
    uint16_t tac_id;
    uint16_t tcoarse;
    uint16_t tfine;
    uint16_t ecoarse;
    uint16_t efine;

    bool operator== (const PackedEvent& rhs) {
        return this->asic_id == rhs.asic_id;
    }
} PackedEvent_t;

typedef std::vector<PackedEvent_t> PackedEventVec;

class SHM_RAW {
public:
	SHM_RAW(std::string path);
	~SHM_RAW();

	unsigned long long getSizeInBytes();
	unsigned long long  getSizeInFrames() { 
		return MaxRawDataFrameQueueSize;
	};
	
	RawDataFrame *getRawDataFrame(int index) {
		RawDataFrame *dataFrame = &shm[index];
		return dataFrame;
	}

	unsigned long long getFrameWord(int index, int n) {
		RawDataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[n];
		return eventWord;
	};

	unsigned getFrameSize(int index) {
		return  getRawDataFrame(index)->getFrameSize();
	};
	

	unsigned long long getFrameID(int index) {
		return  getRawDataFrame(index)->getFrameID();
	};

	bool getFrameLost(int index) {
		return  getRawDataFrame(index)->getFrameLost();
	};

	int getNEvents(int index) {
		return  getRawDataFrame(index)->getNEvents();
	}; 
	
	unsigned getEFine(int index, int event) {
		return  getRawDataFrame(index)->getEFine(event);
	};

	unsigned getTFine(int index, int event) {
		return  getRawDataFrame(index)->getTFine(event);
	};

	unsigned getECoarse(int index, int event) {
		return  getRawDataFrame(index)->getECoarse(event);
	};
	
	unsigned getTCoarse(int index, int event) {
		return  getRawDataFrame(index)->getTCoarse(event);
	};
	
	unsigned getTacID(int index, int event) {
		return  getRawDataFrame(index)->getTacID(event);
	};
	
	unsigned getChannelID(int index, int event) {
		return  getRawDataFrame(index)->getChannelID(event);
	};
	
	std::shared_ptr<PackedEventVec>  getRawFrame(int index) {
	    int nevents = getNEvents(index);
	    int evCount = 0;
	    RawDataFrame *dataFrame = &shm[index];
	    std::shared_ptr<PackedEventVec> procFrame(new PackedEventVec());

	    for (int i = 0; i < nevents; ++i) {
	        //if (dataFrame->feType[i+2] == 0) {
            uint64_t eventWord = dataFrame->data[i+2];
            PackedEvent_t p;

            p.tcoarse = (eventWord >> 38) & 0x3FF;
            p.ecoarse = (eventWord >> 18) & 0x3FF;
            p.tfine = (eventWord >> 28) & 0x3FF;
            p.efine = (eventWord >> 8) & 0x3FF;

            p.tac_id = eventWord & 0x3;
            p.chan_id = (eventWord >> 2) & 0x3F;

            uint64_t idWord = eventWord >> 48;
            uint64_t asicID = idWord & 0x3F;
            uint64_t slaveID = (idWord >> 6) & 0x1F;
            uint64_t portID = (idWord >> 11) & 0x1F;
            p.asic_id = (slaveID & 0x1) * 16*16 + (portID & 0xF) * 16 + (asicID & 0xF);

            procFrame->push_back(p);
	        //}
	    }

	    return procFrame;
	}

private:
	int shmfd;
	RawDataFrame *shm;
	off_t shmSize;
};

}
#endif


