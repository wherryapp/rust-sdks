/*
 * Copyright 2025 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <optional>

#include "api/environment/environment_factory.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "livekit/adm_proxy.h"
#include "livekit/audio_track.h"
#include "livekit/audio_device_controller.h"
#include "media_stream.h"
#include "rtp_parameters.h"
#include "rust/cxx.h"
#include "webrtc.h"

namespace livekit_ffi {
class PeerConnectionFactory;
class AudioDeviceController;
class PeerConnectionObserverWrapper;
}  // namespace livekit_ffi
#include "webrtc-sys/src/peer_connection_factory.rs.h"

namespace livekit_ffi {

class PeerConnection;
struct RtcConfiguration;

webrtc::PeerConnectionInterface::RTCConfiguration to_native_rtc_configuration(
    RtcConfiguration config);

class PeerConnectionFactory {
 public:
  explicit PeerConnectionFactory(std::shared_ptr<RtcRuntime> rtc_runtime);
  PeerConnectionFactory(std::shared_ptr<RtcRuntime> rtc_runtime,
                        bool zero_playout_delay);
  PeerConnectionFactory(std::shared_ptr<RtcRuntime> rtc_runtime,
                        bool zero_playout_delay,
                        bool enable_warp);
  ~PeerConnectionFactory();

  std::shared_ptr<PeerConnection> create_peer_connection(
      RtcConfiguration config,
      rust::Box<PeerConnectionObserverWrapper> observer) const;

  std::shared_ptr<VideoTrack> create_video_track(
      rust::String label,
      std::shared_ptr<VideoTrackSource> source) const;

  std::shared_ptr<AudioTrack> create_audio_track(
      rust::String label,
      std::shared_ptr<AudioTrackSource> source) const;

  // Create an audio track that uses the ADM for capture (microphone)
  // This creates a track that captures from the selected recording device.
  // `options` become the source's AudioOptions: AudioRtpSender::SetSend
  // hands them to the voice engine, which applies them to the APM every
  // time the track is sent (publish, unmute), so they have to say the same
  // thing as set_audio_processing or the send path undoes it.
  std::shared_ptr<AudioTrack> create_device_audio_track(
      rust::String label,
      AudioSourceOptions options) const;

  RtpCapabilities rtp_sender_capabilities(MediaType type) const;

  RtpCapabilities rtp_receiver_capabilities(MediaType type) const;

  // Configure the software audio processing module (echo cancellation,
  // noise suppression, gain control) for every audio send stream, now and
  // for the rest of the factory's life. Applied to the APM directly, and
  // remembered: WebRtcVoiceEngine::Init applies its own defaults (all on)
  // once, lazily, when the first PeerConnection is created, so the switches
  // are re-applied after every PeerConnection is initialised. Later option
  // passes start from the APM's current config and leave them alone.
  void set_audio_processing(bool echo_cancellation,
                            bool noise_suppression,
                            bool auto_gain_control) const;

  std::shared_ptr<RtcRuntime> rtc_runtime() const { return rtc_runtime_; }
  std::shared_ptr<AudioDeviceController> audio_device() const;
  bool zero_playout_delay_enabled() const;

 private:
  // Declaration order matters: rtc_runtime_ must be declared before
  // adm_proxy_/audio_device_ so it is destroyed last. Releasing the proxy
  // does a BlockingCall onto the runtime's worker thread, which must still
  // be running at that point.
  std::shared_ptr<RtcRuntime> rtc_runtime_;
  webrtc::scoped_refptr<AdmProxy> adm_proxy_;
  std::shared_ptr<AudioDeviceController> audio_device_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_factory_;
  struct AudioProcessingSwitches {
    bool echo_cancellation;
    bool noise_suppression;
    bool auto_gain_control;
  };
  void apply_audio_processing_switches(
      const AudioProcessingSwitches& switches) const;

  // The factory's own APM, built here rather than by the media engine so
  // set_audio_processing has a handle to it, and the last switches asked
  // for (re-applied after each PeerConnection is initialised, see above).
  webrtc::scoped_refptr<webrtc::AudioProcessing> audio_processing_;
  mutable std::optional<AudioProcessingSwitches> audio_processing_switches_;
  webrtc::Environment env_;
};

std::shared_ptr<PeerConnectionFactory> create_peer_connection_factory();
std::shared_ptr<PeerConnectionFactory>
create_peer_connection_factory_with_zero_playout_delay();
std::shared_ptr<PeerConnectionFactory>
create_peer_connection_factory_with_options(bool zero_playout_delay,
                                            bool enable_warp);
}  // namespace livekit_ffi
