////////////////////////////////////////////////////////////////////////////
// Model ball2.cc               SIMLIB/C++
//
// Bouncing ball (combined simulation, v2)
//

#include "simlib.h"

// PARAMETERS

// GLOBAL VARIABLES
Stat response_time_stat("Response time");
Histogram response_time_hist("Response time histogram", 0, 0.05, 20);


// CONTAINER CLASS
class Container {
public: 
    int id;  // container id
    int active_requests; // number of active requests
    double load;      // current load
    Stat* load_stat;  // load statistics

    // constructor
    Container(int id) {
        this->id = id;
        this->active_requests = 0;
        this->load = 0.0;
        this->load_stat = new Stat();
    }
    
    void AddRequest(double load) {
        active_requests++;
        UpdateLoad();
    }

    void RemoveRequest(double load) {
        active_requests--;
        UpdateLoad();
    }

    void UpdateLoad(){
        load = active_requests;
        (*load_stat)(load);
    }
};

int main() {                    // experiment control
    // DebugON();
    SetOutput("ball2.dat");
    Print("# Ball2 - bouncing ball model output\n");
    Print("# Time    y     v \n");
    Init(0);                    // initial time 0
    SetStep(1e-10, 0.5);        // minstep defines accuracy of detection
    SetAccuracy(1e-5, 0.001);   // accuracy of numerical method
    Run();                      // simulation
    SIMLIB_statistics.Output(); // print run statistics
}

//
