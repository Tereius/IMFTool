/* Copyright(C) 2020 Björn Stresing, Denis Manthey, Wolfgang Ruppel, Krispin Weiss
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QThreadPool>
#include <chrono>
#include "ImfPackage.h"
#include "PreviewCommon.h"
#include "JP2K_Decoder.h"
#ifdef APP5_ACES
#include "ACES_Decoder.h"
#endif
#ifdef CODEC_HTJ2K
#include "HTJ2K_Decoder.h"
#endif

class FrameRequest
{
public:
	qint64 frameNr; // current frame in asset
	qint64 TframeNr; // current frame in track
	QImage decoded; // decoded image
	bool done; // set to 'true' when decoding completed
	bool error; // set to 'true' in case of error
	QString errorMsg; // error details
	QSharedPointer<AssetMxfTrack> asset; // reference to asset
	int fps; // current playback rate
	int layer; // current layer to decode
	bool mShowActiveArea; // Show Active Area (true) or Show Native Resolution (false)
};



class Player : public QObject, public PreviewCommon
{
public:
	enum eDecoder {
		Decoder_JP2K,
#ifdef APP5_ACES
		Decoder_ACES,
#endif
#ifdef CODEC_HTJ2K
		Decoder_HTJ2K,
#endif
		Decoder_UNKNOWN,
	};
	Q_OBJECT
public:
	Player(enum eDecoder rDecoder);
	~Player();

	// methods
	void setFps(int fps);
	void setLayer(int layer); // layer to decode
	void showActiveArea (bool); // Show Active Area (true) or Show Native Resolution (false)
	void setPlaylist(QVector<VideoResource>& rPlaylist);
	void setPos(qint64 frameNr, qint64 frame_total, int playlist_index);
	void clean();
	void convert_to_709(bool convert);

	float frame_playing_total_float = 0; // current playing position (within track)
	bool playing = false; // currently playing
	bool realspeed = false; // play every frame or skip frames
	float skip_frames = 0; // play every x frames when realspeed == true
	bool show_subtitles = true; // display subtitles during playback
	float frame_playing_asset_float = 0;
private:

	// methods
	void playLoop();

	// decoders
	static const int decoders = 50;
	QThreadPool* threadPool; // threadpool used by the n decoders
	FrameRequest* request_queue[decoders]; // array were n frame requests are stored
	JP2K_Decoder* decoder_queue_JP2K[decoders]; // array were n decoder instances are stored
#ifdef APP5_ACES
	ACES_Decoder* decoder_queue_ACES[decoders]; // array were n decoder instances are stored
#endif
#ifdef CODEC_HTJ2K
	HTJ2K_Decoder* decoder_queue_HTJ2K[decoders]; // array were n decoder instances are stored
#endif
	QSharedPointer<FrameRequest> pointer_queue[decoders];
	QSharedPointer<DecodedFrames> decoded_shared; // decoding status shared among player and all decoders

	// player settings
	int layer = 0; // quality layer to decode (best = 0, default = 5)
	bool mShowActiveArea = false; // Show Native Resolution by default
	qint64 frameNr = 0; // player position within asset
	qint64 TframeNr = 0; // player position within track
	int playlist_index = 0; // playlist asset index
	int fps = 0; // nr of images to play/request per second
	int ms_wait = 1000; // default wait intervall between cycles in play-loop
	QImage nullimage; // empty image
	QElapsedTimer* mTimer; // timer used for play-loop
	qreal video_framerate = 0;

	// player control
	qint64 last_frame_total = 0; // last frame in track
	bool started_playing = false; // buffer was filled and playing out of frames has started

	// decoding
	float frame_decoding_asset_float = 0; // current decoding position (within asset)
	float frame_decoding_total_float = 0; // current decoding position (within track)
	int decoding_index = 0; // decoding asset at this playlist index
	int request_index = 0; // [0...decoders]

	// playing
	int playing_index = 0; // playing asset at this playlist index
	int played_frames_total = 0;  // [0...decoders]
	int player_position_counter = 0; // count from 0...fps, then move frame indicator
	int requested_frames_total = 0; // total requests sent during play cycle
	int last_frame_played = 0;

	// playlist
	QVector<VideoResource> playlist; // all playlist elements

	// buffer management
	int buffer_size = 0; // current buffer size
	bool buffering = true; // currently buffering?
	int buffer_fill_count = 0; // play-loop cycles it took to fill the buffer to fps
	enum eDecoder mDecoder = Decoder_UNKNOWN;

	signals :
	void ShowMsgBox(const QString&, int); // Show MsgBox if set playback speed exceeds processing power
	void playerInfo(const QString&); // send QString from player to WidgetVideoPreview
	void showFrame(const QImage&); // send QImage to WdigetImagePreview
	void currentPlayerPosition(qint64,bool); // set frame indicator position
	void playbackEnded();
	void playTTML(); // get subtitles for current frame indicator position

public slots:
	void startPlay(); // starts playback
};
