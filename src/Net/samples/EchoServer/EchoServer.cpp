#include "Sourcey/Application.h"
#include "EchoServer.h"


// Detect Memory Leaks
#ifdef _DEBUG
#include "MemLeakDetect/MemLeakDetect.h"
#include "MemLeakDetect/MemLeakDetect.cpp"
CMemLeakDetect memLeakDetect;
#endif


using namespace std;
using namespace scy::net;


namespace scy {
namespace net {

	
#define SERVER_PORT 1337
#define SERVER_TYPE TCPEchoServer
	
		
static void onShutdown(void* opaque)
{
	reinterpret_cast<SERVER_TYPE*>(opaque)->shutdown();
}


int main(int argc, char** argv)
{
	Logger::instance().add(new ConsoleChannel("debug", TraceLevel));	
	{	
		Application app;	
		SERVER_TYPE srv(SERVER_PORT);
		srv.shutdown();
		app.waitForShutdown(onShutdown, &srv);
	}
	Logger::uninitialize();
	return 0;
}


} } // namespace scy::Net