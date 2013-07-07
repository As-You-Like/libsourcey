#include "Sourcey/Base.h"
#include "Sourcey/Logger.h"
#include "Sourcey/Runner.h"
#include "Sourcey/Util/Timer.h"

#include "Poco/NamedEvent.h"

#include <assert.h>


using namespace std;
using namespace Poco;
using namespace scy;


/*
// Detect Memory Leaks
#ifdef _DEBUG
#include "MemLeakDetect/MemLeakDetect.h"
CMemLeakDetect memLeakDetect;
#endif
*/


namespace scy {
	

static NamedEvent ready("TestEvent");
	

class Tests
{
	Runner runner;

public:
	Tests()
	{	
		runTimerTest();
		
		util::pause();
	}
	
	
	// ---------------------------------------------------------------------
	//
	// Timer Tests
	//
	// ---------------------------------------------------------------------
	struct TimerTest
	{
		string name;
		int iteration;
		int iterations;
		time_t startAt;

		TimerTest(const string& name, int timeout = 1000, int iterations = 5) : 
			name(name), startAt(util::getTime()), iteration(0), iterations(iterations)
		{
			Log("debug") << "[TimerTest:" << name << "] Creating" << endl;	
			Timer::getDefault().start(TimerCallback<TimerTest>(this, &TimerTest::onTimer, timeout, timeout));
		}

		~TimerTest()
		{
			Log("debug") << "[TimerTest:" << name << "] Destroying" << endl;	
			Timer::getDefault().stop(TimerCallback<TimerTest>(this, &TimerTest::onTimer));
		}

		void onTimer(TimerCallback<TimerTest>& timer)
		{
			iteration++;
			if (iteration < iterations) {
				Log("debug") << "[TimerTest:" << name << "] Callback"
					<< "\n\tElapsed: " << util::getTime() - startAt
					<< "\n\tInterval: " << timer.periodicInterval()
					<< "\n\tIteration: " << iteration
					<< endl;
			}
			else {
				Log("debug") << "[TimerTest:" << name << "] Complete #####################################"
					<< "\n\tTotal Time: " << util::getTime() - startAt
					<< "\n\tCorrect Time: " << timer.periodicInterval() * iterations
					<< "\n\tIterations: " << iterations
					<< endl;			
				delete this;
			}
		}
	};
	 
	void runTimerTest() {
		Log("trace") << "Running Timer Test" << endl;
		new TimerTest("Periodic", 1000, 3);
		new TimerTest("Single", 3000, 1);
		new TimerTest("Single 1", 5000, 1);
		new TimerTest("Periodic 1", 1000, 5);
		new TimerTest("Single 2", 5000, 1);
		new TimerTest("Periodic 2", 1000, 5);
		util::pause();
		Log("trace") << "Running Timer Test: END" << endl;
	}
};


} // namespace scy


int main(int argc, char** argv) 
{	
	Logger::instance().add(new ConsoleChannel("Test", TraceLevel));
	{
		scy::Tests app;
	}	
	scy::Logger::uninitialize();
	util::pause();
	return 0;
}
