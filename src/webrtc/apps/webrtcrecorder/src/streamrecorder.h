#ifndef SCY_WebRTCRecorder_StreamRecorder_H
#define SCY_WebRTCRecorder_StreamRecorder_H


#include "scy/av/avencoder.h"

#include "webrtc/api/peerconnectioninterface.h"


namespace scy {


class StreamRecorderVideoTrack;
class StreamRecorderAudioTrack;


class StreamRecorder:
    public rtc::VideoSinkInterface<cricket::VideoFrame>,
    public webrtc::AudioTrackSinkInterface
{
public:
    StreamRecorder(const av::EncoderOptions& options);
    ~StreamRecorder();

    void setVideoTrack(webrtc::VideoTrackInterface* track);
    void setAudioTrack(webrtc::AudioTrackInterface* track);

    void onPacketEncoded(av::MediaPacket& packet);

    // VideoSinkInterface implementation
    void OnFrame(const cricket::VideoFrame& frame) override;

    // AudioTrackSinkInterface implementation
    void OnData(const void* audio_data,
                        int bits_per_sample,
                        int sample_rate,
                        size_t number_of_channels,
                        size_t number_of_frames) override;

protected:
    av::AVEncoder _encoder;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> _videoTrack;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> _audioTrack;
    bool _awaitingVideo;
    bool _awaitingAudio;
};


} // namespace scy


#endif
