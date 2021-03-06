// -- author: Martino Dall'Osso
// to set clk and deser160phase for single roc test

#include <stdlib.h>   // atof, atoi
#include <algorithm>  // std::find

#include "PixTestSetup.hh"
#include "log.h"
#include "constants.h"


using namespace std;
using namespace pxar;

ClassImp(PixTestSetup)

//------------------------------------------------------------------------------
PixTestSetup::PixTestSetup(PixSetup *a, std::string name) : PixTest(a, name)
{
	PixTest::init();
	init();
	LOG(logDEBUG) << "PixTestSetup ctor(PixSetup &a, string, TGTab *)";
}

//------------------------------------------------------------------------------
PixTestSetup::PixTestSetup() : PixTest()
{
	LOG(logDEBUG) << "PixTestSetup ctor()";
}

//------------------------------------------------------------------------------
bool PixTestSetup::setParameter(string parName, string sval)
{
  bool found(false);
  ParOutOfRange = false;
  std::transform(parName.begin(), parName.end(), parName.begin(), ::tolower);
  for (unsigned int i = 0; i < fParameters.size(); ++i) 
  {
    if (fParameters[i].first == parName) 
    {
    	found = true; 

	if (!parName.compare("clkmax")) 
	{
		fClkMax = atoi(sval.c_str()); 
		if(fClkMax < 0 || fClkMax > 25)	
		{ 
		 	LOG(logINFO) << "PixTestSetup::setParameter() ClkMax out of range (0-25)";
			found=false; ParOutOfRange = true;
		}
		//setToolTips();
        }
  	if (!parName.compare("desermax")) 
	{
		fDeserMax = atoi(sval.c_str()); 
		if(fDeserMax < 0 || fDeserMax > 7)	
		{ 
		 	LOG(logINFO) << "PixTestSetup::setParameter() DeserMax out of range (0-7)";
			found=false; ParOutOfRange = true;
		}
		//setToolTips();
	}
      	break;
    }
  }
  return found; 
}

//------------------------------------------------------------------------------
void PixTestSetup::init()
{
	LOG(logINFO) << "PixTestSetup::init()";

	fDirectory = gFile->GetDirectory(fName.c_str());
	if (!fDirectory)
		fDirectory = gFile->mkdir(fName.c_str());
	fDirectory->cd();
}

// ----------------------------------------------------------------------
void PixTestSetup::setToolTips()
{
	fTestTip = string(Form("scan testboard parameter settings and check for valid readout\n")
		+ string("TO BE IMPLEMENTED!!"))  //FIXME
		;
	fSummaryTip = string("summary plot to be implemented")  //FIXME
		;

}

//------------------------------------------------------------------------------
void PixTestSetup::bookHist(string name)
{
	fDirectory->cd();
	LOG(logDEBUG) << "nothing done with " << name;
}

//------------------------------------------------------------------------------
PixTestSetup::~PixTestSetup()
{
	LOG(logDEBUG) << "PixTestSetup dtor";
	std::list<TH1*>::iterator il;
	fDirectory->cd();
	for (il = fHistList.begin(); il != fHistList.end(); ++il) 
	{
		LOG(logINFO) << "Write out " << (*il)->GetName();
		(*il)->SetDirectory(fDirectory);
		(*il)->Write();
	}
}

//------------------------------------------------------------------------------
void PixTestSetup::doTest()
{
    cacheDacs();
	if (ParOutOfRange) return;
	fDirectory->cd();
	fHistList.clear();
	PixTest::update();
	
	//fixed number of trigger (unstable with only 1 trigger)
	int Ntrig = 2;
	
	LOG(logINFO) << "PixTestSetup::doTest() ntrig = " << Ntrig;

	bookHist("bla"); //FIXME

	//set pattern with only res and token:
	pg_setup.clear();
	pg_setup.push_back(make_pair(0x0800, 25));    // PG_RESR b001000
	pg_setup.push_back(make_pair(0x0100, 0));     // PG_TOK  b000001
	uint16_t period = 28;

	// Set the pattern generator:
	fApi->setPatternGenerator(pg_setup);
	LOG(logINFO) << "PixTestSetup:: pg set to RES|TOK";

	TH2D *histo = new TH2D(Form("DeserphaseClkScan"), Form("DeserphaseClkScan"), fDeserMax, 0., (fDeserMax+1), fClkMax, 0., (fClkMax+1));
	histo->GetXaxis()->SetTitle("deser160phase");
	histo->GetYaxis()->SetTitle("clk");
	fHistList.push_back(histo);

	rawEvent daqRawEv;
	size_t daqRawEvsize;
	int ideser, iclk;
	bool goodvaluefound = false;
	int goodclk, goodeser = -1;

	printf("        0        1        2        3        4        5        6        7\n");
	for (iclk = 0; iclk <= fClkMax; iclk++)
	{
		printf("%2i:", iclk);
		for (ideser = 0; ideser <= fDeserMax; ideser++)
		{
			//delays initialization:
			setTbParameters(iclk, ideser);	
			sig_delays = fPixSetup->getConfigParameters()->getTbSigDelays();

			fApi->setTestboardDelays(sig_delays);

			// Start the DAQ:
			fApi->daqStart();
			// Send the triggers:
			fApi->daqTrigger(Ntrig,period);
			// Read the raw event:
			daqRawEv = fApi->daqGetRawEvent();
			daqRawEvsize = daqRawEv.GetSize();

			//from deser160 test structure. To find the Header (2040):
			if (daqRawEvsize)
			{
				int h = int(daqRawEv.data[0] & 0xffc);
				if (h == 0x7f8)
				 {
					printf(" <%03X>", int(daqRawEv.data[0] & 0xffc));
					histo->Fill(ideser, iclk);	
					goodvaluefound = true;
					goodclk = iclk; goodeser = ideser;					
				 }
				else
					printf("  %03X ", int(daqRawEv.data[0] & 0xffc));

				if (daqRawEvsize < 10)
					printf("[%u]", (unsigned int)(daqRawEvsize));
				else
					printf("[*]");
			}

			else cout << "  ......";

			// Stop the DAQ:
			fApi->daqStop();

		}
			cout << endl;
	}

	histo->Draw("colz");
	fDisplayedHist = find(fHistList.begin(), fHistList.end(), histo);
	PixTest::update(); //needed?
    
	int finalclk, finaldeser;

	if (goodvaluefound)
	{
	//algorithm to choose the best values - TO BE IMPLEMENTED FIXME
	//now initialized to the second to last (not good for long cable):
	finalclk = goodclk-1;
	finaldeser = goodeser;
//	LOG(logINFO) << "PixTestSetup:: good delays are:" << endl << "clk = "<<  finalclk << " -  deser160 = " << finaldeser << endl;
	setTbParameters(finalclk, finaldeser);
	}

	else
	{
	//back to default values
	finalclk = finaldeser = 4;
	LOG(logINFO) << "PixTestSetup::doTest() none good delays found. Back to default values (clk 4 - deser 4)"<< endl;
	setTbParameters(finalclk, finaldeser);
	}

	//set final clk and deser160:
	sig_delays = fPixSetup->getConfigParameters()->getTbSigDelays();
	fApi->setTestboardDelays(sig_delays);
	LOG(logINFO) << "PixTestSetup:: tb Delays set to:" << endl << "clk = " << finalclk << " -  deser160 = " << finaldeser << endl;

	//set pg_setup to default:
	pgToDefault(pg_setup);

	fHistList.clear();
	restoreDacs();   //needed?
	LOG(logINFO) << "PixTestSetup::doTest() done for " ;
}

// ----------------------------------------------------------------------
void PixTestSetup::setTbParameters(int clk, int deser160) {
	fPixSetup->getConfigParameters()->setTbParameter("clk", clk);
	fPixSetup->getConfigParameters()->setTbParameter("ctr", clk);
	fPixSetup->getConfigParameters()->setTbParameter("sda", clk + 15);
	fPixSetup->getConfigParameters()->setTbParameter("tin", clk + 5);
	fPixSetup->getConfigParameters()->setTbParameter("deser160phase", deser160);
}

// ----------------------------------------------------------------------
void PixTestSetup::saveTbParameters() {
  LOG(logINFO) << "Write Tb parameters to file"; 
  fPixSetup->getConfigParameters()->writeTbParameterFile();
}

// ----------------------------------------------------------------------
void PixTestSetup::runCommand(std::string command) {
  std::transform(command.begin(), command.end(), command.begin(), ::tolower);
  LOG(logDEBUG) << "running command: " << command;
  if (!command.compare("savetbparameters")) {
    saveTbParameters(); 
    return;
  }
  LOG(logDEBUG) << "did not find command ->" << command << "<-";
}

// ----------------------------------------------------------------------
void PixTestSetup::pgToDefault(vector<pair<uint16_t, uint8_t> > pg_setup) {
	pg_setup.clear();
	LOG(logDEBUG) << "PixTestPattern::PG_Setup clean";

	pg_setup.push_back(make_pair(0x0800, 25));               // PG_RESR b001000 
	pg_setup.push_back(make_pair(0x0400, 100 + 6));			// PG_CAL  b000100
	pg_setup.push_back(make_pair(0x0200, 16));			   // PG_TRG  b000010
	pg_setup.push_back(make_pair(0x0100, 0));		      // PG_TOK  		

	fApi->setPatternGenerator(pg_setup);
	LOG(logINFO) << "PixTestPattern::       pg_setup set to default.";
}
