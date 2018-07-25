#include <RawReader.hpp>
#include <OverlappedEventHandler.hpp>
#include <getopt.h>
#include <assert.h>

#include <boost/lexical_cast.hpp>

#include <TFile.h>
#include <TNtuple.h>

#include <TH3S.h>
#include <TProfile.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace PETSYS;

const int MAX_ASIC_ID = 32*32*64;
const int MAX_TAC_ID = MAX_ASIC_ID * 64*4;

	
class DataStore {
private:
	const char *what;
	int nBins1, nBins2;
	float min1, min2;
	float max1, max2;
	float hw1, hw2;
	
public:
	TH3 **hList_T, **hList_E;

public:
	DataStore(const char *what, int nBins1, float min1, float max1, int nBins2, float min2, float max2) {
		this->what = what;
		this->nBins1 = nBins1;
		this->nBins2 = nBins2;
		this->min1 = min1;
		this->min2 = min2;
		this->max1 = max1;
		this->max2 = max2;
		this->hw1 = (max1 - min1)/(2 * nBins1);
		this->hw2 = (max2 - min2)/(2 * nBins2);
		
		
		this->hList_T = new TH3 *[MAX_TAC_ID];
		this->hList_E = new TH3 *[MAX_TAC_ID];
		
	};
	
	~DataStore() {
	}
	
	void closeStep(float step1, float step2) {
	};
	
	void addEvents(float step1, float step2,EventBuffer<RawHit> *buffer) {
		int N = buffer->getSize();
		for (int i = 0; i < N; i++) {
			RawHit &hit = buffer->get(i);
			int id = hit.channelID * 4 + hit.tacID;
			
			if (hList_T[id] == NULL) {
				char hName[128];
				char hTitle[128];
				sprintf(hName, "h_%s_t_%08d_%d", what, hit.channelID, hit.tacID);
				sprintf(hTitle, "%s T %08d %d", what, hit.channelID, hit.tacID);
				hList_T[id] = new TH3S(hName, hTitle, nBins1, min1, max1, nBins2, min2, max2, 1024, 0, 1024);
				sprintf(hName, "h_%s_e_%08d_%d", what, hit.channelID, hit.tacID);
				sprintf(hTitle, "%s E %08d %d", what, hit.channelID, hit.tacID);
				hList_E[id] = new TH3S(hName, hTitle, nBins1, min1, max1, nBins2, min2, max2, 1024, 0, 1024);
			}
			
			
			
			hList_T[id]->Fill(step1 + hw1, step2 + hw2, hit.tfine);
			hList_E[id]->Fill(step1 + hw1, step2 + hw2, hit.efine);
		}
		
	}
	
};

class DataStoreHelper : public OverlappedEventHandler<RawHit, RawHit> {
private: 
	DataStore *dataStore;
	float step1;
	float step2;
public:
	DataStoreHelper(DataStore *dataStore, float step1, float step2, EventSink<RawHit> *sink) :
		OverlappedEventHandler<RawHit, RawHit>(sink, true),
		dataStore(dataStore), step1(step1), step2(step2)
	{
	};
	
	EventBuffer<RawHit> * handleEvents(EventBuffer<RawHit> *buffer) {
		dataStore->addEvents(step1, step2,buffer);
		return buffer;
	};
	
	void pushT0(double t0) { };
	void report() { };
};

void displayHelp(char * program)
{
	fprintf(stderr, "Usage: %s -i <input_file_prefix> -o <output_file_prefix> [optional arguments]\n", program);
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr,  "  -i \t\t\t Input file prefix - raw data\n");
	fprintf(stderr,  "  -o \t\t\t Output file name - containins raw event data in ROOT format.\n");
};


void displayUsage(char *argv0)
{
	printf("Usage: %s -i <input_file_prefix> -o <output_file_prefix> [optional arguments]\n", argv0);
}


struct result_t {
	bool	pass;
	int	tdca_count;
	int	qdca_count;
	int	fetp_count;
	float	tdca_t_slope;
	float	tdca_t_rms;
	float	tdca_e_slope;
	float	tdca_e_rms;
	float	qdca_slope;
	float	qdca_rms;
	float	fetp_t_rms;
	float	fetp_q_rms;
	
};
bool check_tdc(int idx, TH3* h, float &slope_out, float &rms_out);
bool check_qdc(int idx, TH3* h, float &slope_out, float &rms_out);
bool check_fetp_rms(int idx, TH3* h, float &rms_out);

int main(int argc, char *argv[])
{
        char *inputFilePrefix = NULL;
	char *outputFileName = NULL;
    
        static struct option longOptions[] = {
        };

        while(true) {
                int optionIndex = 0;
                int c = getopt_long(argc, argv, "i:o:",longOptions, &optionIndex);

                if(c == -1) break;
                else if(c != 0) {
                        // Short arguments
                        switch(c) {
                        case 'i':       inputFilePrefix = optarg; break;
                        case 'o':       outputFileName = optarg; break;
			default:        displayUsage(argv[0]); exit(1);
			}
		}
		else if(c == 0) {
			switch(optionIndex) {
			default:	displayUsage(argv[0]); exit(1);
			}
		}
		else {
			assert(false);
		}
	}
	
	if(inputFilePrefix == NULL) {
		fprintf(stderr, "-i must be specified\n");
		exit(1);
	}

	
	FILE *tacReportFile = NULL;
	if(outputFileName == NULL) {
		tacReportFile = fopen("/dev/null", "w");
	}
	else {
		tacReportFile = fopen(outputFileName, "w");
	}
	

	char fName[1024];
	RawReader *reader;
	
	
	/*
	 * TDCA data
	 */
	sprintf(fName, "%s_tdca", inputFilePrefix);
	reader = RawReader::openFile(fName);
	DataStore *tdcaDataStore = new DataStore("tdca", 4, 0, 1, 1, 0, 1);
	
	for(int stepIndex = 0; stepIndex < reader->getNSteps(); stepIndex++) {
		float step1, step2;
		reader->getStepValue(stepIndex, step1, step2);
		reader->processStep(stepIndex, false,
				new DataStoreHelper (tdcaDataStore, step1, step2,
				new NullSink<RawHit>()
				));
		
		tdcaDataStore->closeStep(step1, step2);
	}
	delete reader;
	
	/*
	 * QDCA data
	 */
	sprintf(fName, "%s_qdca", inputFilePrefix);
	reader = RawReader::openFile(fName);
	DataStore *qdcaDataStore = new DataStore("qdca", 4, 0, 1, 3, 60, 120);
	
	for(int stepIndex = 0; stepIndex < reader->getNSteps(); stepIndex++) {
		float step1, step2;
		reader->getStepValue(stepIndex, step1, step2);
		reader->processStep(stepIndex, false,
				new DataStoreHelper (qdcaDataStore, step1, step2,
				new NullSink<RawHit>()
				));
		
		qdcaDataStore->closeStep(step1, step2);
	}
	delete reader;
	
	/*
	 * FETP data
	 */
	sprintf(fName, "%s_fetp", inputFilePrefix);
	reader = RawReader::openFile(fName);
	DataStore *fetpDataStore = new DataStore("fetp", 3, 0, 1, 1, 0, 1);
	
	for(int stepIndex = 0; stepIndex < reader->getNSteps(); stepIndex++) {
		float step1, step2;
		reader->getStepValue(stepIndex, step1, step2);
		reader->processStep(stepIndex, false,
				new DataStoreHelper (fetpDataStore, step1, step2,
				new NullSink<RawHit>()
				));
		
		fetpDataStore->closeStep(step1, step2);
	}
	delete reader;

	sprintf(fName, "%s_expected.txt", inputFilePrefix);
	FILE * expectedListFile = fopen(fName, "r");
	int expectedAsicID;
	
	result_t * tac_result_list = (result_t *)mmap(NULL, sizeof(result_t)*MAX_TAC_ID, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	int maxWorkers = sysconf(_SC_NPROCESSORS_ONLN);
	int nWorkers = 0;

	rewind(expectedListFile);
	while(fscanf(expectedListFile, "%d\n", &expectedAsicID) == 1) {
		while(nWorkers >= maxWorkers) {
			wait(NULL);
			nWorkers -= 1;
		}
		
		if (fork() == 0) {
			// We are in child
			for(int i = (expectedAsicID*256); i < (expectedAsicID*256 + 256); i++) {
				result_t &tac_result = tac_result_list[i];
				tac_result = { true , 0, 0, 0, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
				
				tac_result.tdca_count = (tdcaDataStore->hList_T[i] == NULL) ? 0 : tdcaDataStore->hList_T[i]->GetEntries();
				tac_result.qdca_count = (qdcaDataStore->hList_T[i] == NULL) ? 0 : qdcaDataStore->hList_T[i]->GetEntries();
				tac_result.fetp_count = (fetpDataStore->hList_T[i] == NULL) ? 0 : fetpDataStore->hList_T[i]->GetEntries();
				
				if (tac_result.tdca_count > 200) {
					tac_result.pass &= check_tdc(i, tdcaDataStore->hList_T[i], tac_result.tdca_t_slope, tac_result.tdca_t_rms);
					tac_result.pass &= check_tdc(i, tdcaDataStore->hList_E[i], tac_result.tdca_e_slope, tac_result.tdca_e_rms);
				}
				else {
					tac_result.pass = false;
				}
				
				
				if(tac_result.qdca_count > 200) {
					tac_result.pass &= check_qdc(i, qdcaDataStore->hList_E[i], tac_result.qdca_slope, tac_result.qdca_rms);
				}
				else {
					tac_result.pass = false;
				}

				if(tac_result.fetp_count > 50) {
					tac_result.pass &= check_fetp_rms(i, fetpDataStore->hList_T[i], tac_result.fetp_t_rms);
					tac_result.pass &= check_fetp_rms(i, fetpDataStore->hList_E[i], tac_result.fetp_q_rms);
				}
				else {
					tac_result.pass = false;
				}				
			}
			return 0;
		} else {
			nWorkers += 1;
		}

	}
	
	while(nWorkers > 0) {
		wait(NULL);
		nWorkers -= 1;
	}	

	bool * asic_list = new bool [MAX_ASIC_ID];
	for(int asicID = 0; asicID < MAX_ASIC_ID; asicID++) {
		asic_list[asicID] = true;
		
		for(int tacID = 0; tacID < 64*4; tacID++) {
			asic_list[asicID] &= tac_result_list[asicID*64*4 + tacID].pass;
		}
	}

	rewind(expectedListFile);
	bool allOK = true;
	while(fscanf(expectedListFile, "%d\n", &expectedAsicID) == 1) {
		for(int i = expectedAsicID * 256; i < (expectedAsicID+1)*256; i++) {
			result_t &tac_result = tac_result_list[i];
			fprintf(tacReportFile, "TAC %8d %s %4d %4d %4d %5.1f % 5.3f %5.1f % 5.3f %5.1f % 5.3f %5.3f % 5.3f\n",
                                i,
                                tac_result.pass ? "PASS" : "FAIL",
                                tac_result.tdca_count, tac_result.qdca_count, tac_result.fetp_count,
                                tac_result.tdca_t_slope, tac_result.tdca_t_rms,
                                tac_result.tdca_e_slope, tac_result.tdca_e_rms,
                                tac_result.qdca_slope, tac_result.qdca_rms,
                                tac_result.fetp_t_rms, tac_result.fetp_q_rms
                        );
		}

		bool asicOK = asic_list[expectedAsicID];
		printf("ASIC %d is %s\n", expectedAsicID, asicOK ? "PASS" : "FAIL");
		allOK &= asicOK;
	}
		
	
	delete fetpDataStore;
	delete qdcaDataStore;
	delete tdcaDataStore;
	fclose(tacReportFile);
	return allOK ? 0 : 1;
}

bool check_tdc(int idx, TH3 *h, float &slope_out, float &rms_out)
{
	int nBins = h->GetXaxis()->GetNbins();
	int lower = h->GetXaxis()->GetXmin();
	int upper = h->GetXaxis()->GetXmax();
	
	TProfile *p = new TProfile("tmp", "tmp", nBins, lower, upper, "s");
	for (int xi = 1; xi <= h->GetXaxis()->GetNbins(); xi ++) {
		float x = h->GetXaxis()->GetBinLowEdge(xi);

		for(int yi = 1; yi <= h->GetYaxis()->GetNbins(); yi ++) {
			float y = h->GetYaxis()->GetBinLowEdge(yi);
			
			for(int zi = 1; zi <= h->GetZaxis()->GetNbins(); zi ++) {
				float z = h->GetZaxis()->GetBinLowEdge(zi);
				
				int bi = h->GetBin(xi, yi, zi);
				int count = h->GetBinContent(bi);
				
				for(int n = 0; n < count; n++)
					p->Fill(x, z);
			}
			
		}
	}
	
	
	int r = false;
	for(int i = 0; i < nBins; i++) {
		int bin_a = 1 + i;
		int bin_b = 1 + ((i + 1) % nBins);
		int bin_c = 1 + ((i + 2) % nBins);
		float t_a = p->GetBinLowEdge(bin_a);
		float t_b = t_a + 1.0 * p->GetBinWidth(1);
		float t_c = t_a + 2.0 * p->GetBinWidth(1);
		
		float rms_a = p->GetBinError(bin_a);
		float rms_b = p->GetBinError(bin_b);
		float rms_c = p->GetBinError(bin_c);
		
		float avg_a = p->GetBinContent(bin_a);
		float avg_b = p->GetBinContent(bin_b);
		float avg_c = p->GetBinContent(bin_c);
		
		bool badRMS = (rms_b < 0.3) || (rms_b > 2.0);
		float slope = (avg_a - avg_c)/(t_a - t_c);
		bool badSlope = (rms_a < 10) && (rms_c < 10) && (slope > -100) || (slope < -200);

		if(idx == -1) {
			fprintf(stderr, "(%5.1f-%5.1f)/(%5.3f - %5.3f) ", avg_a, avg_c, t_a, t_c);
			fprintf(stderr, "%d %5.3f %5.1f %s\n", i, rms_b, slope, (badRMS || badSlope) ? "BAD" : "OK ");
		}
		
		
		if(badRMS) {
			// Not accepting due to RMS
			rms_out = rms_b;
		}
		else if(badSlope) {
			// Not accepting due to slope
			slope_out = slope;
		}
		else {
			// Accepting
			rms_out = rms_b;
			slope_out = slope;
			r = true;
			break;
		}
		
	}
	
	delete p;
	return r;

}


bool check_qdc(int idx, TH3 *h, float &slope_out, float &rms_out)
{
	int nBins = h->GetYaxis()->GetNbins();
	int lower = h->GetYaxis()->GetXmin();
	int upper = h->GetYaxis()->GetXmax();
	
	bool r = false;
	TProfile *p = new TProfile("tmp", "tmp", nBins, lower, upper, "s");

	for (int xi = 1; xi <= h->GetXaxis()->GetNbins(); xi ++) {
		float x = h->GetXaxis()->GetBinLowEdge(xi);

		p->Reset();
		for(int yi = 1; yi <= h->GetYaxis()->GetNbins(); yi ++) {
			float y = h->GetYaxis()->GetBinLowEdge(yi);
			
			for(int zi = 1; zi <= h->GetZaxis()->GetNbins(); zi ++) {
				float z = h->GetZaxis()->GetBinLowEdge(zi);
				
				int bi = h->GetBin(xi, yi, zi);
				int count = h->GetBinContent(bi);
		
				for(int n = 0; n < count; n++)
					p->Fill(y, z);
			}
			
		}
		
		
		float min_rms = INFINITY;
		float max_rms = -INFINITY;
		
		for (int i = 0; i < nBins; i++) {
			int bin = i + 1;
			float rms = p->GetBinError(bin);
			min_rms = (min_rms < rms) ? min_rms : rms;
			max_rms = (max_rms > rms) ? max_rms : rms;
		}
		
		float min_slope = INFINITY;
		float max_slope = -INFINITY;
		for (int i = 0; i < (nBins-1); i++) {
			int bin1 = i + 1;
			int bin2 = bin1 + 1;
			float avg1 = p->GetBinContent(bin1);
			float avg2 = p->GetBinContent(bin2);
			float t1 = p->GetBinLowEdge(bin1);
			float t2 = p->GetBinLowEdge(bin2);
 			float slope = (avg1 - avg2)/(t1 - t2);
			
			min_slope = (min_slope < slope) ? min_slope : slope;
			max_slope = (max_slope > slope) ? max_slope : slope;

			if(idx == -1) {
				fprintf(stderr, "%d %d (%5.1f - %5.1f)/(%5.1f - %5.1f) = %5.1f\n", xi, i, avg1, avg2, t1, t2, slope);
			}
		}
		
		
		if (min_rms < 0.3) {
			rms_out = min_rms;
			continue;
		}
		else if (max_rms > 2.0) {
			rms_out = max_rms;
			continue;
		}
		else if (min_slope < 3.0) {
			rms_out = max_rms;
			slope_out = min_slope;
		}
		else if (max_slope > 10.0) {
			rms_out = max_rms;
			slope_out = max_slope;
		}
		else {
			rms_out = max_rms;
			slope_out = min_slope;
			r = true;
			break;
		}
	}
	
	
	delete p;
	return r;
}

bool check_fetp_rms(int idx, TH3 *h, float &rms_out)
{
	int nBins = h->GetYaxis()->GetNbins();
	int lower = h->GetYaxis()->GetXmin();
	int upper = h->GetYaxis()->GetXmax();
	
	bool r = false;
	TH1 *p = new TH1S("tmp", "tmp", 1024, 0, 1024);

	for (int xi = 1; xi <= h->GetXaxis()->GetNbins(); xi ++) {
		float x = h->GetXaxis()->GetBinLowEdge(xi);

		p->Reset();
		for(int yi = 1; yi <= h->GetYaxis()->GetNbins(); yi ++) {
			float y = h->GetYaxis()->GetBinLowEdge(yi);
			
			for(int zi = 1; zi <= h->GetZaxis()->GetNbins(); zi ++) {
				float z = h->GetZaxis()->GetBinLowEdge(zi);
				
				int bi = h->GetBin(xi, yi, zi);
				int count = h->GetBinContent(bi);
		
				for(int n = 0; n < count; n++)
					p->Fill(z);
			}
			
		}
		
		
		float rms = p->GetRMS();

		if(rms < 0.3) {
			rms_out = rms;
			continue;
		}
		else if(rms > 2.0) {
			rms_out = rms;
			continue;
		}
		else {
			rms_out = rms;
			r = true;
			break;
		}
	}
	
	
	delete p;
	return r;
}
