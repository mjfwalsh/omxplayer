/*
 * Copyright (C) 2020 by Michael J. Walsh
 *
 * Much of this file is a slimmed down version of lsdvd by Chris Phillips
 * and Henk Vergonet, and Martin Thierer's fork.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <dlfcn.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>
#include <memory>

#include "OMXDvdPlayer.h"

class DVD
{
private:
	typedef decltype(::DVDOpen)* DVDOpen_t;
	typedef decltype(::DVDClose)* DVDClose_t;
	typedef decltype(::DVDOpenFile)* DVDOpenFile_t;
	typedef decltype(::DVDCloseFile)* DVDCloseFile_t;
	typedef decltype(::DVDReadBlocks)* DVDReadBlocks_t;
	typedef decltype(::DVDDiscID)* DVDDiscID_t;
	typedef decltype(::ifoOpen)* ifoOpen_t;
	typedef decltype(::ifoClose)* ifoClose_t;

	void *load_function(const char *func_name)
	{
		void *r = dlsym(dll_handle, func_name);
		if(!r)
			throw "Failed to load libdvdread\n";

		return r;
	}

	void *dll_handle;

public:
	DVDOpen_t	   DVDOpen;
	DVDClose_t	  DVDClose;
	DVDOpenFile_t   DVDOpenFile;
	DVDCloseFile_t  DVDCloseFile;
	DVDReadBlocks_t DVDReadBlocks;
	DVDDiscID_t	 DVDDiscID;
	ifoOpen_t	   ifoOpen;
	ifoClose_t	  ifoClose;

	DVD()
	{
		dll_handle = dlopen("libdvdread.so", RTLD_LAZY);
		if(!dll_handle) {
			throw "Failed to load libdvdread\n";
		}

		DVDOpen		= (DVDOpen_t)load_function("DVDOpen");
		DVDClose	   = (DVDClose_t)load_function("DVDClose");
		DVDOpenFile	= (DVDOpenFile_t)load_function("DVDOpenFile");
		DVDCloseFile   = (DVDCloseFile_t)load_function("DVDCloseFile");
		DVDReadBlocks  = (DVDReadBlocks_t)load_function("DVDReadBlocks");
		DVDDiscID	  = (DVDDiscID_t)load_function("DVDDiscID");
		ifoOpen		= (ifoOpen_t)load_function("ifoOpen");
		ifoClose	   = (ifoClose_t)load_function("ifoClose");
	}

	~DVD()
	{
		dlclose(dll_handle);
	}
};

OMXDvdPlayer::OMXDvdPlayer(const std::string &filename)
{
	// let's see if we have libdvdread installed
	dvdread = new DVD();

	const int audio_id[7] = {0x80, 0, 0xC0, 0xC0, 0xA0, 0, 0x88};

	device_path = filename;

	// Open DVD device or file
	dvd_device = dvdread->DVDOpen(device_path.c_str());
	if(!dvd_device)
		throw "Error on DVDOpen";

	// Get device name and checksum
	read_title_name();
	read_disc_checksum();

	// Open dvd meta data header
	ifo_handle_t *ifo_zero = dvdread->ifoOpen(dvd_device, 0);
	if(!ifo_zero) {
		fprintf( stderr, "Can't open main ifo!\n");
		throw "Failed to open DVD";
	}

	ifo_handle_t **ifo = new ifo_handle_t*[ifo_zero->vts_atrt->nr_of_vtss + 1];

	for (int i = 1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) {
		ifo[i] = dvdread->ifoOpen(dvd_device, i);
		if (!ifo[i]) {
			fprintf( stderr, "Can't open ifo %d!\n", i);
			throw "Failed to open DVD";
		}
	}

	// alloc space for tracks
	int track_count = ifo_zero->tt_srpt->nr_of_srpts;
	tracks.reserve(track_count);

	for(int track = 0; track < track_count; track++) {
		int title_set_num = ifo_zero->tt_srpt->title[track].title_set_nr;
		int vts_ttn = ifo_zero->tt_srpt->title[track].vts_ttn;

		if (!ifo[title_set_num]->vtsi_mat)
			continue;

		pgcit_t	   *vts_pgcit = ifo[title_set_num]->vts_pgcit;
		vtsi_mat_t *vtsi_mat  = ifo[title_set_num]->vtsi_mat;
		pgc_t	   *pgc		  = vts_pgcit->pgci_srp[ifo[title_set_num]->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn - 1].pgc;

		if(pgc->cell_playback == NULL || pgc->program_map == NULL)
			continue;

		// Ignore tracks which are shorter than two minutes
		int track_length = dvdtime2msec(&pgc->playback_time);
		if(track_length < 120000)
			continue;

		// start a new track
		tracks.emplace_back();
		auto &this_track = tracks.back();
		this_track.length = track_length;
		int cell_count = pgc->nr_of_cells;

		// Tracks
		// ignore non-contiguous ends of tracks
		this_track.first_sector = pgc->cell_playback[0].first_sector;
		int last_sector = -1;

		for (int i = 0; i < cell_count - 1; i++) {
			int end = pgc->cell_playback[i].last_sector;
			int next = pgc->cell_playback[i+1].first_sector - 1;
			if(end != next) {
				last_sector = end;
				int missing_time = 0;
				for (i++; i < cell_count; i++){
					missing_time += dvdtime2msec(&pgc->cell_playback[i].playback_time);
				}
				this_track.length -= missing_time;
				break;
			}
		}
		if(last_sector == -1)
			last_sector = pgc->cell_playback[cell_count - 1].last_sector;

		this_track.last_sector = last_sector;

		// Chapters
		this_track.chapters.reserve(pgc->nr_of_programs);

		int acc_chapter = 0;
		int cell_i = 0;
		for (int i = 0; i < pgc->nr_of_programs; i++) {
			int idx = pgc->program_map[i] - 1;
			int cell_start = pgc->cell_playback[idx].first_sector;
			if(cell_start > last_sector)
				break;

			this_track.chapters.push_back({
				.cell = cell_start,
				.time = acc_chapter
			});

			for(; cell_i <= idx; cell_i++) {
				acc_chapter += dvdtime2msec(&pgc->cell_playback[idx].playback_time);
			}
		}

		// stream data is the same for each title set
		// check if we've already seen this title set
		for(int i = (int)tracks.size() - 2; i > -1; i--) {
			if(tracks[i].title->title_num == title_set_num) {
				this_track.title = tracks[i].title;
				break;
			}
		}
		if(this_track.title)
			continue;

		// allocate a new title
		this_track.title.reset(new title_info);
		this_track.title->title_num = title_set_num;

		// Audio streams
		for (int i = 0; i < 8; i++) {
			if ((pgc->audio_control[i] & 0x8000) == 0) continue;

			audio_attr_t *audio_attr = &vtsi_mat->vts_audio_attr[i];

			this_track.title->audio_streams.push_back({
				.id = audio_id[audio_attr->audio_format] + (pgc->audio_control[i] >> 8 & 7),
				.lang = audio_attr->lang_code,
			});
		}

		// Subtitles
		int x = vtsi_mat->vts_video_attr.display_aspect_ratio == 0 ? 24 : 8;
		for (int i = 0; i < 32; i++) {
			if ((pgc->subp_control[i] & 0x80000000) == 0) continue;

			subp_attr_t *subp_attr = &vtsi_mat->vts_subp_attr[i];

			this_track.title->subtitle_streams.push_back({
				.id = (int)((pgc->subp_control[i] >> x) & 0x1f) + 0x20,
				.lang = subp_attr->lang_code,
			});
		}

		// Palette
		for (int i = 0; i < 16; i++)
			this_track.title->palette[i] = yvu2rgb(pgc->palette[i]);
	}

	// close dvd meta data filehandles
	for (int i = 1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) dvdread->ifoClose(ifo[i]);
	delete ifo;
	dvdread->ifoClose(ifo_zero);
	puts("Finished parsing DVD meta data");
}

bool OMXDvdPlayer::ChangeTrack(int delta, int &t)
{
	int ct = t + delta;

	bool r = OpenTrack(ct);
	t = current_track;
	return r;
}

bool OMXDvdPlayer::OpenTrack(int ct)
{
	if(ct < 0 || ct > (int)tracks.size() - 1)
		return false;

	if(m_open && tracks[ct].title->title_num != tracks[current_track].title->title_num)
		CloseTrack();

	// select track
	current_track = ct;

	// seek to beginning to track
	pos = 0;
	pos_byte_offset = 0;

	// blocks for this track
	total_blocks = tracks[current_track].last_sector - tracks[current_track].first_sector + 1;

	// open dvd track
	if(!m_open) {
		dvd_track = dvdread->DVDOpenFile(dvd_device, tracks[current_track].title->title_num, DVD_READ_TITLE_VOBS );

		if(!dvd_track) {
			puts("Error on DVDOpenFile");
			return false;
		}
	}

	m_open = true;
	return true;
}

int OMXDvdPlayer::Read(unsigned char *lpBuf, int64_t uiBufSize)
{
	if(!m_open)
		return 0;

	// read in block in whole numbers
	int blocks_to_read = uiBufSize / DVD_VIDEO_LB_LEN;

	if(pos + blocks_to_read > total_blocks) {
		blocks_to_read = total_blocks - pos;

		if(blocks_to_read < 1)
			return 0;
	}

	int read_blocks = dvdread->DVDReadBlocks(dvd_track, tracks[current_track].first_sector + pos, blocks_to_read, lpBuf);

	if(read_blocks <= 0) {
		return read_blocks;
	} else if(pos_byte_offset > 0) {
		int bytes_read = read_blocks * DVD_VIDEO_LB_LEN - pos_byte_offset;

		// shift the contents of the buffer to the left
		memmove(lpBuf, lpBuf + pos_byte_offset, bytes_read);

		pos_byte_offset = 0;
		pos += read_blocks;
		return bytes_read;
	} else {
		pos += read_blocks;
		return read_blocks * DVD_VIDEO_LB_LEN;
	}
}

int OMXDvdPlayer::getCurrentTrackLength()
{
	return tracks[current_track].length;
}

uint32_t *OMXDvdPlayer::getPalette()
{
	return tracks[current_track].title->palette;
}

int64_t OMXDvdPlayer::Seek(int64_t iFilePosition, int iWhence)
{
	if (!m_open || iWhence != SEEK_SET)
		return -1;

	// seek in blocks
	pos = iFilePosition / DVD_VIDEO_LB_LEN;
	pos_byte_offset = iFilePosition % DVD_VIDEO_LB_LEN;

	return 0;
}

// search for the chapter the timestamp is in
int OMXDvdPlayer::getChapter(int64_t timestamp)
{
	int needle = timestamp / 1000;
	int l = 0;
	int r = (int)tracks[current_track].chapters.size() - 1;

	if(needle >= tracks[current_track].chapters[r].time)
		return r;

	while(r - l > 1)
	{
		int m = (l + r) / 2;

		if(needle >= tracks[current_track].chapters[m].time)
			l = m;
		else
			r = m;
	}

	return l;
}

int OMXDvdPlayer::GetChapterInfo(int64_t &seek_ts, int64_t &byte_pos)
{
	int seek_ch = getChapter(seek_ts);
	seek_ts  = (int64_t)tracks[current_track].chapters[seek_ch].time * 1000;
	byte_pos = (int64_t)tracks[current_track].chapters[seek_ch].cell * DVD_VIDEO_LB_LEN;

	return seek_ch;
}

int64_t OMXDvdPlayer::GetChapterBytePos(int seek_ch)
{
	return (int64_t)tracks[current_track].chapters[seek_ch].cell * DVD_VIDEO_LB_LEN;
}

int64_t OMXDvdPlayer::GetSizeInBytes()
{
	return (int64_t)total_blocks * DVD_VIDEO_LB_LEN;
}

bool OMXDvdPlayer::IsEOF()
{
	if(!m_open)
		return true;

	return pos >= total_blocks;
}

int OMXDvdPlayer::TotalChapters()
{
	return tracks[current_track].chapters.size();
}

void OMXDvdPlayer::CloseTrack()
{
	dvdread->DVDCloseFile(dvd_track);
	m_open = false;
}

int OMXDvdPlayer::GetAudioStreamCount()
{
	return tracks[current_track].title->audio_streams.size();
}

int OMXDvdPlayer::GetSubtitleStreamCount()
{
	return tracks[current_track].title->subtitle_streams.size();
}

int OMXDvdPlayer::GetAudioStreamId(int i)
{
	return tracks[current_track].title->audio_streams[i].id;
}

int OMXDvdPlayer::GetSubtitleStreamId(int i)
{
	return tracks[current_track].title->subtitle_streams[i].id;
}

const char *OMXDvdPlayer::GetAudioStreamLanguage(int i)
{
	return convertLangCode(tracks[current_track].title->audio_streams[i].lang);
}

const char *OMXDvdPlayer::GetSubtitleStreamLanguage(int i)
{
	return convertLangCode(tracks[current_track].title->subtitle_streams[i].lang);
}

OMXDvdPlayer::~OMXDvdPlayer()
{
	if(m_open)
		CloseTrack();

	dvdread->DVDClose(dvd_device);
	delete dvdread;
}

int OMXDvdPlayer::dvdtime2msec(dvd_time_t *dt)
{
	int ms  = ((dt->hour   >> 4) * 10 + (dt->hour   & 0x0f)) * 3600000;
	ms     += ((dt->minute >> 4) * 10 + (dt->minute & 0x0f)) * 60000;
	ms     += ((dt->second >> 4) * 10 + (dt->second & 0x0f)) * 1000;

	int frames = ((dt->frame_u & 0x30) >> 4) * 10 + (dt->frame_u & 0x0f);

	if((dt->frame_u >> 6) == 1)
		// 25fps equals 40 millisecs per frame
		ms += frames * 40;
	else
		// 29.97fps equals 33.3667 millisecs per frame
		ms += (int)((frames * 33.3667) + 0.5);

	return ms;
}

/*
 *  The following method is based on code from vobcopy, by Robos, with thanks.
 */
void OMXDvdPlayer::read_title_name()
{
	FILE *filehandle = 0;
	char title[33];
	int  i;

	if (! (filehandle = fopen(device_path.c_str(), "r"))) {
		disc_title = "unknown";
		return;
	}

	if ( fseek(filehandle, 32808, SEEK_SET ) || 32 != (i = fread(title, 1, 32, filehandle)) ) {
		fclose(filehandle);
		disc_title = "unknown";
		return;
	}

	fclose (filehandle);

	// Add terminating null char
	title[32] = '\0';

	// trim end whitespace
	while(--i > -1 && title[i] == ' ')
		title[i] = '\0';

	// Replace underscores with spaces
	i++;
	while(--i > -1)
		if(title[i] == '_') title[i] = ' ';

	disc_title = title;
}

void OMXDvdPlayer::read_disc_checksum()
{
	unsigned char buf[16];
	if (dvdread->DVDDiscID(dvd_device, &buf[0]) == -1) {
		fprintf(stderr, "Failed to get DVD checksum\n");
		read_disc_serial_number(); // fallback
		return;
	}

	char hex[33];
	for (int i = 0; i < 16; i++)
		sprintf(hex + 2 * i, "%02x", buf[i]);

	disc_checksum = hex;
}

/*
 *  The following method is based on code from vobcopy, by Robos, with thanks.
 *  Modified to also read serial number and alternative title based on
 *  libdvdnav's src/vm/vm.c
 */
void OMXDvdPlayer::read_disc_serial_number()
{
	char serial_no[9];
	char buffer[DVD_VIDEO_LB_LEN];

	FILE *fh = fopen(device_path.c_str(), "r");
	if(!fh) {
		fprintf(stderr, "Failed to open %s\n", device_path.c_str());
		return;
	}

	if(fseek(fh, 65536, SEEK_SET) || fread(buffer, 1, DVD_VIDEO_LB_LEN, fh) != DVD_VIDEO_LB_LEN) {
		fclose(fh);
		fprintf(stderr, "IO Error on %s\n", device_path.c_str());
		return;
	}

	fclose (fh);

	int i = 0;
	while(i < 8 && isprint(buffer[i + 73])) {
		serial_no[i] = buffer[i + 73];
		i++;
	}
	serial_no[i] = '\0';

	disc_checksum = serial_no;
}

//enable heuristic track skip
void OMXDvdPlayer::enableHeuristicTrackSelection()
{
	int track_count = tracks.size();
	std::vector<bool> enabled(track_count, true);

	// Search for and disable composite tracks
	for(int i = 0; i < track_count - 1; i++) {
		for(int j = i + 1; j < track_count; j++) {
			if(tracks[i].title->title_num == tracks[j].title->title_num
					&& tracks[i].first_sector == tracks[j].first_sector) {

				if(tracks[i].length > tracks[j].length)
					enabled[i] = false;
				else
					enabled[j] = false;
			}
		}
	}

	// remove disabled tracks
	for(int i = track_count - 1; i > -1; i--)
		if(!enabled[i])
			tracks.erase(tracks.begin() + i);

	tracks.shrink_to_fit();
}

int clamp(float val)
{
	if(val > 255) return 255;
	else if(val < 0) return 0;
	else return (int)(val + 0.5f);
}

// This is a condensed version code from mpv
int OMXDvdPlayer::yvu2rgb(int color)
{
	int y = color >> 16 & 0xff;
	int u = color >> 8 & 0xff;
	int v = color & 0xff;

	float r = -222.9215f + 1.1643f * y +				 1.5960f * v;
	float g =  135.5752f + 1.1643f * y + -0.3917f * u + -0.8129f * v;
	float b = -276.8358f + 1.1643f * y +  2.0172f * u;

	return clamp(b) << 16 | clamp(g) << 8 | clamp(r);
}


const char* OMXDvdPlayer::convertLangCode(uint16_t lang)
{
	// Convert two letter ISO 639-1 language code to 3 letter ISO 639-2/T
	// Supports depreciated iw, in and ji codes.
	// If not found return two letter DVD code
	switch(lang) {
		case 0x6161: /* aa */ return "aar";
		case 0x6162: /* ab */ return "abk";
		case 0x6165: /* ae */ return "ave";
		case 0x6166: /* af */ return "afr";
		case 0x616B: /* ak */ return "aka";
		case 0x616D: /* am */ return "amh";
		case 0x616E: /* an */ return "arg";
		case 0x6172: /* ar */ return "ara";
		case 0x6173: /* as */ return "asm";
		case 0x6176: /* av */ return "ava";
		case 0x6179: /* ay */ return "aym";
		case 0x617A: /* az */ return "aze";
		case 0x6261: /* ba */ return "bak";
		case 0x6265: /* be */ return "bel";
		case 0x6267: /* bg */ return "bul";
		case 0x6268: /* bh */ return "bih";
		case 0x6269: /* bi */ return "bis";
		case 0x626D: /* bm */ return "bam";
		case 0x626E: /* bn */ return "ben";
		case 0x626F: /* bo */ return "bod";
		case 0x6272: /* br */ return "bre";
		case 0x6273: /* bs */ return "bos";
		case 0x6361: /* ca */ return "cat";
		case 0x6365: /* ce */ return "che";
		case 0x6368: /* ch */ return "cha";
		case 0x636F: /* co */ return "cos";
		case 0x6372: /* cr */ return "cre";
		case 0x6373: /* cs */ return "ces";
		case 0x6375: /* cu */ return "chu";
		case 0x6376: /* cv */ return "chv";
		case 0x6379: /* cy */ return "cym";
		case 0x6461: /* da */ return "dan";
		case 0x6465: /* de */ return "deu";
		case 0x6476: /* dv */ return "div";
		case 0x647A: /* dz */ return "dzo";
		case 0x6565: /* ee */ return "ewe";
		case 0x656C: /* el */ return "ell";
		case 0x656E: /* en */ return "eng";
		case 0x656F: /* eo */ return "epo";
		case 0x6573: /* es */ return "spa";
		case 0x6574: /* et */ return "est";
		case 0x6575: /* eu */ return "eus";
		case 0x6661: /* fa */ return "fas";
		case 0x6666: /* ff */ return "ful";
		case 0x6669: /* fi */ return "fin";
		case 0x666A: /* fj */ return "fij";
		case 0x666F: /* fo */ return "fao";
		case 0x6672: /* fr */ return "fra";
		case 0x6679: /* fy */ return "fry";
		case 0x6761: /* ga */ return "gle";
		case 0x6764: /* gd */ return "gla";
		case 0x676C: /* gl */ return "glg";
		case 0x676E: /* gn */ return "grn";
		case 0x6775: /* gu */ return "guj";
		case 0x6776: /* gv */ return "glv";
		case 0x6861: /* ha */ return "hau";

		case 0x6865: /* he */
		case 0x6977: /* iw */ return "heb";

		case 0x6869: /* hi */ return "hin";
		case 0x686F: /* ho */ return "hmo";
		case 0x6872: /* hr */ return "hrv";
		case 0x6874: /* ht */ return "hat";
		case 0x6875: /* hu */ return "hun";
		case 0x6879: /* hy */ return "hye";
		case 0x687A: /* hz */ return "her";
		case 0x6961: /* ia */ return "ina";

		case 0x6964: /* id */
		case 0x696E: /* in */ return "ind";

		case 0x6965: /* ie */ return "ile";
		case 0x6967: /* ig */ return "ibo";
		case 0x6969: /* ii */ return "iii";
		case 0x696B: /* ik */ return "ipk";
		case 0x696F: /* io */ return "ido";
		case 0x6973: /* is */ return "isl";
		case 0x6974: /* it */ return "ita";
		case 0x6975: /* iu */ return "iku";
		case 0x6A61: /* ja */ return "jpn";
		case 0x6A76: /* jv */ return "jav";
		case 0x6B61: /* ka */ return "kat";
		case 0x6B67: /* kg */ return "kon";
		case 0x6B69: /* ki */ return "kik";
		case 0x6B6A: /* kj */ return "kua";
		case 0x6B6B: /* kk */ return "kaz";
		case 0x6B6C: /* kl */ return "kal";
		case 0x6B6D: /* km */ return "khm";
		case 0x6B6E: /* kn */ return "kan";
		case 0x6B6F: /* ko */ return "kor";
		case 0x6B72: /* kr */ return "kau";
		case 0x6B73: /* ks */ return "kas";
		case 0x6B75: /* ku */ return "kur";
		case 0x6B76: /* kv */ return "kom";
		case 0x6B77: /* kw */ return "cor";
		case 0x6B79: /* ky */ return "kir";
		case 0x6C61: /* la */ return "lat";
		case 0x6C62: /* lb */ return "ltz";
		case 0x6C67: /* lg */ return "lug";
		case 0x6C69: /* li */ return "lim";
		case 0x6C6E: /* ln */ return "lin";
		case 0x6C6F: /* lo */ return "lao";
		case 0x6C74: /* lt */ return "lit";
		case 0x6C75: /* lu */ return "lub";
		case 0x6C76: /* lv */ return "lav";
		case 0x6D67: /* mg */ return "mlg";
		case 0x6D68: /* mh */ return "mah";
		case 0x6D69: /* mi */ return "mri";
		case 0x6D6B: /* mk */ return "mkd";
		case 0x6D6C: /* ml */ return "mal";
		case 0x6D6E: /* mn */ return "mon";
		case 0x6D72: /* mr */ return "mar";
		case 0x6D73: /* ms */ return "msa";
		case 0x6D74: /* mt */ return "mlt";
		case 0x6D79: /* my */ return "mya";
		case 0x6E61: /* na */ return "nau";
		case 0x6E62: /* nb */ return "nob";
		case 0x6E64: /* nd */ return "nde";
		case 0x6E65: /* ne */ return "nep";
		case 0x6E67: /* ng */ return "ndo";
		case 0x6E6C: /* nl */ return "nld";
		case 0x6E6E: /* nn */ return "nno";
		case 0x6E6F: /* no */ return "nor";
		case 0x6E72: /* nr */ return "nbl";
		case 0x6E76: /* nv */ return "nav";
		case 0x6E79: /* ny */ return "nya";
		case 0x6F63: /* oc */ return "oci";
		case 0x6F6A: /* oj */ return "oji";
		case 0x6F6D: /* om */ return "orm";
		case 0x6F72: /* or */ return "ori";
		case 0x6F73: /* os */ return "oss";
		case 0x7061: /* pa */ return "pan";
		case 0x7069: /* pi */ return "pli";
		case 0x706C: /* pl */ return "pol";
		case 0x7073: /* ps */ return "pus";
		case 0x7074: /* pt */ return "por";
		case 0x7175: /* qu */ return "que";
		case 0x726D: /* rm */ return "roh";
		case 0x726E: /* rn */ return "run";
		case 0x726F: /* ro */ return "ron";
		case 0x7275: /* ru */ return "rus";
		case 0x7277: /* rw */ return "kin";
		case 0x7361: /* sa */ return "san";
		case 0x7363: /* sc */ return "srd";
		case 0x7364: /* sd */ return "snd";
		case 0x7365: /* se */ return "sme";
		case 0x7367: /* sg */ return "sag";
		case 0x7369: /* si */ return "sin";
		case 0x736B: /* sk */ return "slk";
		case 0x736C: /* sl */ return "slv";
		case 0x736D: /* sm */ return "smo";
		case 0x736E: /* sn */ return "sna";
		case 0x736F: /* so */ return "som";
		case 0x7371: /* sq */ return "sqi";
		case 0x7372: /* sr */ return "srp";
		case 0x7373: /* ss */ return "ssw";
		case 0x7374: /* st */ return "sot";
		case 0x7375: /* su */ return "sun";
		case 0x7376: /* sv */ return "swe";
		case 0x7377: /* sw */ return "swa";
		case 0x7461: /* ta */ return "tam";
		case 0x7465: /* te */ return "tel";
		case 0x7467: /* tg */ return "tgk";
		case 0x7468: /* th */ return "tha";
		case 0x7469: /* ti */ return "tir";
		case 0x746B: /* tk */ return "tuk";
		case 0x746C: /* tl */ return "tgl";
		case 0x746E: /* tn */ return "tsn";
		case 0x746F: /* to */ return "ton";
		case 0x7472: /* tr */ return "tur";
		case 0x7473: /* ts */ return "tso";
		case 0x7474: /* tt */ return "tat";
		case 0x7477: /* tw */ return "twi";
		case 0x7479: /* ty */ return "tah";
		case 0x7567: /* ug */ return "uig";
		case 0x756B: /* uk */ return "ukr";
		case 0x7572: /* ur */ return "urd";
		case 0x757A: /* uz */ return "uzb";
		case 0x7665: /* ve */ return "ven";
		case 0x7669: /* vi */ return "vie";
		case 0x766F: /* vo */ return "vol";
		case 0x7761: /* wa */ return "wln";
		case 0x776F: /* wo */ return "wol";
		case 0x7868: /* xh */ return "xho";

		case 0x7969: /* yi */
		case 0x6A69: /* ji */ return "yid";

		case 0x796F: /* yo */ return "yor";
		case 0x7A61: /* za */ return "zha";
		case 0x7A68: /* zh */ return "zho";
		case 0x7A75: /* zu */ return "zul";
	}

	static char two_letter_code[3];
	sprintf(two_letter_code, "%c%c", lang >> 8, lang & 0xff);
	return two_letter_code;
}
