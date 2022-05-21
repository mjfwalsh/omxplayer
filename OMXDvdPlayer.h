#pragma once

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <string>

class DVD;

class OMXDvdPlayer
{
  public:
	OMXDvdPlayer(const std::string &filename);
	~OMXDvdPlayer();

	void CloseTrack();
	bool ChangeTrack(int delta, int &t);
	bool OpenTrack(int ct);

	int Read(unsigned char *lpBuf, int64_t uiBufSize);
	int64_t Seek(int64_t iFilePosition, int iWhence);
	bool IsEOF();
	int64_t GetSizeInBytes();
	int getCurrentTrackLength();
	int TotalChapters();
	int64_t GetChapterStartTime(int i);
	int GetCurrentTrack() const { return current_track; }
	int GetAudioStreamCount();
	int GetSubtitleStreamCount();
	int GetAudioStreamId(int i);
	int GetSubtitleStreamId(int i);
	const char *GetAudioStreamLanguage(int i);
	const char *GetSubtitleStreamLanguage(int i);
	std::string GetID() const { return disc_checksum; }
	std::string GetTitle() const { return disc_title; }
	void enableHeuristicTrackSelection();
	uint32_t *getPalette();

  private:
	int dvdtime2msec(dvd_time_t *dt);
	const char* convertLangCode(uint16_t lang);
	void read_title_name();
	void read_disc_checksum();
	void read_disc_serial_number();

	bool m_open = false;
	bool m_allocated = false;
	int pos = 0;
	int pos_byte_offset = 0;
	DVD *dvdread = NULL;

	dvd_reader_t *dvd_device = NULL;
	dvd_file_t *dvd_track = NULL;

	int current_track = -1;
	int total_blocks = 0;

	std::string device_path;
	std::string disc_title;
	std::string disc_checksum;

	typedef struct title_info {
		int title_num;
		int audiostream_count;
		struct stream_info {
			int id; // as in the hex number in "Stream #0:2[0x85]:"
			uint16_t lang; // lang code
		} *audio_streams;
		int subtitle_count;
		struct stream_info *subtitle_streams;
		uint32_t palette[16];
	} title_info;

	int track_count = 0;
	struct track_info {
		bool enabled;
		title_info *title;
		int length;
		int chapter_count;
		int *chapters;
		int first_sector;
		int last_sector;
	} *tracks;

	float frames_per_s[4] = {-1.0, 25.00, -1.0, 29.97};

	int yvu2rgb(int c);
};
