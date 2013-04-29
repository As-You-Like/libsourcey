#include "Sourcey/Logger.h"
#include "Sourcey/Runner.h"
#include "Sourcey/PacketStream.h"
#include "Sourcey/Util.h"
#include "Sourcey/Media/MediaFactory.h"
#include "Sourcey/Media/MotionDetector.h"
#include "Sourcey/Media/AVEncoder.h"
#include "Sourcey/Media/FLVMetadataInjector.h"
#include "Sourcey/Net/TCPService.h"
#include "Sourcey/HTTP/MultipartPacketizer.h"
#include "Sourcey/TURN/client/TCPClient.h"

#include <conio.h>


using namespace std;
using namespace Sourcey;
using namespace Sourcey::Net;
using namespace Sourcey::Media;
using namespace Sourcey::TURN;
using namespace Sourcey::Util;


/*
// Detect Memory Leaks
#ifdef _DEBUG
#include "MemLeakDetect/MemLeakDetect.h"
#include "MemLeakDetect/MemLeakDetect.cpp"
CMemLeakDetect memLeakDetect;
#endif
*/


namespace Sourcey { 
namespace TURN {	

		
// ----------------------------------------------------------------------------
//
// Media Formats
//-------		
Format MP344100 = Format("MP3", "mp3", 
	VideoCodec(),
	AudioCodec("MP3", "libmp3lame", 2, 44100, 128000, "s16p"));	
	// Adobe Flash Player requires that audio files be 16bit and have a sample rate of 44.1khz.
	// Flash Player can handle MP3 files encoded at 32kbps, 48kbps, 56kbps, 64kbps, 128kbps, 160kbps or 256kbps.
	// NOTE: 128000 works fine for 44100, but 64000 is broken!
			
Format MP38000  = Format("MP3", "mp3", 
	VideoCodec(),
	AudioCodec("MP3", "libmp3lame", 1, 8000, 64000));
			
Format MP311000  = Format("MP3", "mp3", 
	VideoCodec(),
	AudioCodec("MP3", "libmp3lame", 1, 11000, 16000));

Format MP348000 = Format("MP3", "mp3", 
	VideoCodec(),
	AudioCodec("MP3", "libmp3lame", 2, 48000, 128000, "s16p"));	//, "s16p")
 						
Format FLVNoAudio = Format("FLV", "flv", 
	VideoCodec("FLV", "flv", 320, 240));	//, 6, 9000, 64000

Format FLVSpeex16000 = Format("FLV", "flv", 
	VideoCodec("FLV", "flv", 320, 240),
	AudioCodec("Speex", "libspeex", 1, 16000));	

Format FLVSpeex16000NoVideo = Format("FLV", "flv", 
	VideoCodec(),
	AudioCodec("Speex", "libspeex", 1, 16000));	//, 16800

Format MJPEG = Format("MJPEG", "mjpeg", 
	VideoCodec("MJPEG", "mjpeg", 640, 480, 25));


// Global for now
class TURNMediaProvider;
Format ourMediaFormat = MJPEG; 
VideoCapture* ourVideoCapture = NULL;
AudioCapture* ourAudioCapture = NULL;
TURNMediaProvider* ourMediaProvider = NULL;


// ----------------------------------------------------------------------------
//
// TCP TURN Initiator
//-------
class TURNMediaProvider: public Sourcey::TURN::ITCPClientObserver
{
public:
	TURN::TCPClient client;
	Net::IP			peerIP;	
	Net::Address	ourPeerAddr;	
	PacketStream    stream;

	Signal<TURN::Client&> AllocationCreated;
	Signal2<TURN::Client&, const Net::Address&> ConnectionCreated;

	TURNMediaProvider(Net::Reactor& reactor, Runner& runner, const TURN::Client::Options options, const Net::IP& peerIP) : 
		client(*this, reactor, runner, options), peerIP(peerIP)
	{
	}

	virtual ~TURNMediaProvider() 
	{ 
		client.terminate(); 
		stopMedia();
	}

	void initiate() 
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] Initializing" << endl;		
		terminate();

		try	{
			client.addPermission(peerIP);	
			client.initiate();
		} 
		catch (Poco::Exception& exc) {
			Log("error") << "[TURNMediaProvider: " << this << "] Error: " << exc.displayText() << std::endl;
		}
	}

	void terminate() {
		client.terminate();
		stopMedia();
	}

	void playMedia() {
		Log("debug") << "[TURNMediaProvider: " << this << "] Play Media" << endl;

		RecorderOptions options;
		options.oformat = ourMediaFormat;			
		if (options.oformat.video.enabled && ourVideoCapture) {
			AllocateOpenCVInputFormat(ourVideoCapture, options.iformat);
			stream.attach(ourVideoCapture, false);
		}
		if (options.oformat.audio.enabled && ourAudioCapture) {
			AllocateRtAudioInputFormat(ourAudioCapture, options.iformat);
			stream.attach(ourAudioCapture, false);	
		}
		
		AVEncoder* encoder = new AVEncoder(options);
		encoder->initialize();
		stream.attach(encoder, 5, true);

		if (options.oformat.label == "MJPEG") {
			HTTP::MultipartPacketizer* packetizer = new HTTP::MultipartPacketizer("image/jpeg", false);
			stream.attach(packetizer, 10, true);
		}
		
		stream += packetDelegate(this, &TURNMediaProvider::onMediaEncoded);
		stream.start();	
		Log("debug") << "[TURNMediaProvider: " << this << "] Play Media: OK" << endl;
	}

	void stopMedia() {
		Log("debug") << "[TURNMediaProvider: " << this << "] Stop Media" << endl;
		stream -= packetDelegate(this, &TURNMediaProvider::onMediaEncoded);
		stream.close();	
		Log("debug") << "[TURNMediaProvider: " << this << "] Stop Media: OK" << endl;
	}

protected:
	void onRelayStateChange(TURN::Client& client, TURN::ClientState& state, const TURN::ClientState&) 
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] State Changed: " << state.toString() << endl;

		switch(state.id()) {
		case TURN::ClientState::Waiting:				
			break;
		case TURN::ClientState::Allocating:				
			break;
		case TURN::ClientState::Authorizing:
			break;
		case TURN::ClientState::Success:
			AllocationCreated.emit(this, this->client);
			break;
		case TURN::ClientState::Failed:
			assert(0 && "Allocation failed");
			break;
		case TURN::ClientState::Terminated:	
			break;
		}
	}
	
	void onClientConnectionCreated(TURN::TCPClient& client, Net::IPacketSocket* sock, const Net::Address& peerAddr) //UInt32 connectionID, 
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] Connection Created: " << peerAddr << endl;
		ourPeerAddr = peerAddr; // Current peer

		ConnectionCreated.emit(this, this->client, peerAddr);

		// Restart the media steram for each new peer
		stopMedia();
		playMedia();
		Log("debug") << "[TURNMediaProvider: " << this << "] Connection Created: OK: " << peerAddr << endl;
	}
	
	void onClientConnectionState(TURN::TCPClient& client, Net::IPacketSocket*, 
		Net::SocketState& state, const Net::SocketState& oldState) 
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] Connection State: " << state.toString() << endl;
	}

	void onRelayedData(TURN::Client& client, const char* data, int size, const Net::Address& peerAddr)
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] Received Data: " << string(data, size) <<  ": " << peerAddr << endl;
	}
	
	void onAllocationPermissionsCreated(TURN::Client& client, const TURN::PermissionList& permissions)
	{
		Log("debug") << "[TURNMediaProvider: " << this << "] Permissions Created" << endl;
	}

	void onMediaEncoded(void* sender, DataPacket& packet)
	{
		Log("trace") << "[TURNMediaProvider: " << this << "] Sending Packet: " << packet.size() << endl;
		
		try {
			// Send the media to our peer
			client.sendData((const char*)packet.data(), packet.size(), ourPeerAddr);
		}
		catch (Exception& exc) {
			Log("error") << "[TURNMediaProvider: " << this << "] Send Error: " << exc.displayText() << endl;
			terminate();
		}
	}
};



// ----------------------------------------------------------------------------
//
// Media Connection
//-------
class AddressRequestHandler: public Poco::Net::TCPServerConnection
{
public:
	AddressRequestHandler(const Poco::Net::StreamSocket& sock) : 
		Poco::Net::TCPServerConnection(sock)
	{		
		Log("trace") << "[AddressRequestHandler] Creating" << endl;
	}

	~AddressRequestHandler() 
	{
		Log("trace") << "[AddressRequestHandler] Destroying" << endl;
	}
		
	void run()
	{
		Log("trace") << "[AddressRequestHandler] Running" << endl;
		
		// Reinitiate the provider session
		ourMediaProvider->initiate();
		ourMediaProvider->AllocationCreated += delegate(this, &AddressRequestHandler::onAllocationCreated);
		
		wakeUp.wait();
		
		ourMediaProvider->AllocationCreated -= delegate(this, &AddressRequestHandler::onAllocationCreated);

		Log("trace") << "[AddressRequestHandler] Exiting" << endl;
	}
	
	void onAllocationCreated(void* sender, TURN::Client& client) //
	{
		Log("debug") << "[AddressRequestHandler] Allocation Created" << endl;
					
		// Write the relay address	
		assert(ourMediaProvider);
		string data = ourMediaProvider->client.relayedAddress().toString();
		socket().sendBytes(data.data(), data.size());

		Log("debug") << "[AddressRequestHandler] Allocation Created 1" << endl;		
		wakeUp.set();
		Log("debug") << "[AddressRequestHandler] Allocation Created 2" << endl;
	}
	
	Poco::Event wakeUp;
};


// ----------------------------------------------------------------------------
//
// HTTP Connection Factory
//-------
class AddressRequestHandlerFactory: public Poco::Net::TCPServerConnectionFactory
{
public:
	Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) 
	{  
		Log("trace") << "[AddressRequestHandlerFactory] Creating Connection" << endl;
	
		Poco::Net::StreamSocket sock(socket);

		try 
		{
			// Wait until we have some data in the sock read buffer
			// then parse and redirect accordingly.
			char buffer[4096];
			sock.setReceiveTimeout(Poco::Timespan(2,0));
			int size = sock.receiveBytes(buffer, sizeof(buffer));
			string request(buffer, size);

			Log("trace") << "HTTP Connection:\n"
				<< "\n\tClient Address: " << sock.peerAddress().toString()
				<< "\n\tRequest: " << request
				<< endl;

			// Flash policy-file-request
			if ((request.find("policy-file-request") != string::npos) ||
				(request.find("crossdomain") != string::npos)) {
				Log("trace") << "[AddressRequestHandlerFactory] Sending Flash Crossdomain Policy" << endl;
				return new FlashPolicyRequestHandler(sock);
			}

			return new AddressRequestHandler(sock);
		}
		catch (Exception& exc)
		{
			Log("error") << exc.displayText() << endl;
		}

		return new BadRequestHandler(sock);
	}
};


// ----------------------------------------------------------------------------
//
// HTTP Relay Address Service
//-------
class RelayAddressService: public TCPService
{
public:
	RelayAddressService(unsigned short port) :
		TCPService(new AddressRequestHandlerFactory(), port)
	{
	}

	~RelayAddressService()
	{
	}
};


} } // namespace Sourcey::TURN



int main(int argc, char** argv)
{
	Logger::instance().add(new ConsoleChannel("debug", TraceLevel));

	//MediaFactory::initialize();
	//MediaFactory::instance()->loadVideo();
	//MediaFactory::instance()->loadAudio();
	
	// Init input devices
	ourVideoCapture = new VideoCapture(0);
	//ourAudioCapture = MediaFactory::instance()->audio.getCapture(0, 
	//	ourMediaFormat.audio.channels, 
	//	ourMediaFormat.audio.sampleRate);	

	// Create client
	Sourcey::Net::Reactor reactor;
	Runner runner;
	Net::IP peerIP("127.0.0.1");
	TURN::Client::Options co;
	co.serverAddr = Net::Address("127.0.0.1", 3478); // "173.230.150.125"
	co.lifetime  = 120 * 1000; // 2 minute
	co.timeout = 10 * 1000;
	co.timerInterval = 3 * 1000;
	co.username = Anionu_API_USERNAME;
	co.password = Anionu_API_PASSWORD;
	ourMediaProvider = new TURNMediaProvider(reactor, runner, co, peerIP);

	// Create address service
	RelayAddressService service(800);
	service.start();

	char o = 0;
	while (o != 'Q') 
	{	
		cout << 
			"COMMANDS:\n\n"
			"  A	Create the Allocation\n"
			"  Z	Print the Allocation Address\n"
			"  Q	Quit\n\n";
		
		o = toupper(getch());
		
		// Create the Allocation
		if (o == 'A') {			
			ourMediaProvider->terminate();
			ourMediaProvider->initiate();
		}

		// Print the Allocation Address
		else if (o == 'Z') {
			cout << 
				"########################################" <<
				"\n\n" <<
				"  Allocation Address: " << (ourMediaProvider ? ourMediaProvider->client.relayedAddress().toString() : "None") <<
				"\n\n" <<
				"########################################\n\n";
		}
	}

	// Wait for user input...
	Util::pause();

	if (ourMediaProvider)
		delete ourMediaProvider;
	MediaFactory::uninitialize();

	return 0;
}