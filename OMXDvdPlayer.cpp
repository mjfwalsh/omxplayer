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
#include <limits.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>
#include <memory>

#include "OMXDvdPlayer.h"

OMXDvdPlayer::OMXDvdPlayer(const std::string &filename)
{
	const int audio_id[7] = {0x80, 0, 0xC0, 0xC0, 0xA0, 0, 0x88};

	device_path = filename;

	// Open DVD device or file
	dvd_device = DVDOpen(device_path.c_str());
	if(!dvd_device)
		throw "Error on DVDOpen";

	// Get device name and checksum
	read_title_name();
	read_disc_checksum();

	// Open dvd meta data header
	ifo_handle_t *ifo_zero = ifoOpen(dvd_device, 0);
	if(!ifo_zero)
		throw "Failed to open DVD: Can't open main ifo!";

	ifo_handle_t **ifo = new ifo_handle_t*[ifo_zero->vts_atrt->nr_of_vtss + 1];

	for (int i = 1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) {
		ifo[i] = ifoOpen(dvd_device, i);
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

		// allocate new track
		tracks.emplace_back();
		auto &this_track = tracks.back();
		this_track.length = track_length;

		// some DVD tracks have dummy cells at the end, ignore them
		uint cell_count = pgc->nr_of_cells;
		int length_of_last_cell = dvdtime2msec(&pgc->cell_playback[cell_count - 1].playback_time);
		if(length_of_last_cell <= 1000)
		{
			this_track.length -= length_of_last_cell;
			cell_count--;
		}

		// scan for discontinuities (ie non-adjacent blocks of cells)
		for(uint i = 0, start_of_this_block = UINT_MAX; i < cell_count; i++)
		{
			// skips angle cell
			if(pgc->cell_playback[i].block_type == BLOCK_TYPE_ANGLE_BLOCK)
				continue;

			if(start_of_this_block == UINT_MAX)
				start_of_this_block = pgc->cell_playback[i].first_sector;

			// search for discontinuity or end
			if(i == cell_count - 1 || pgc->cell_playback[i].last_sector + 1 != pgc->cell_playback[i+1].first_sector)
			{
				this_track.parts.push_back({
					.first_sector = start_of_this_block,
					.blocks = (int)(pgc->cell_playback[i].last_sector - start_of_this_block + 1),
				});
				start_of_this_block = UINT_MAX;
			}
		}
		if(this_track.parts.size() == 0)
		{
			tracks.resize(tracks.size() - 1);
			continue;
		}

		// Chapters
		int acc_time = 0;
		int acc_cells = 0;
		int cell_i = 0;
		for (int i = 0; i < pgc->nr_of_programs; i++) {
			int cell_no = pgc->program_map[i] - 1;
			if(pgc->cell_playback[cell_no].block_type == BLOCK_TYPE_ANGLE_BLOCK)
				continue;

			this_track.chapters.push_back({
				.cell = acc_cells,
				.time = acc_time
			});

			for(; cell_i < cell_no; cell_i++)
			{
				// skip angle cells
				if(pgc->cell_playback[cell_i].block_type == BLOCK_TYPE_ANGLE_BLOCK)
					continue;

				acc_time += dvdtime2msec(&pgc->cell_playback[cell_i].playback_time);
				acc_cells += pgc->cell_playback[cell_i].last_sector - pgc->cell_playback[cell_i].first_sector + 1;
			}
		}

		// stream data is the same for each title set
		// check if we've already seen this title set
		for(int i = (int)tracks.size() - 2; i > -1; i--)
		{
			if(tracks[i].title->title_num == title_set_num)
			{
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
		for (int i = 0; i < 8; i++)
		{
			if ((pgc->audio_control[i] & 0x8000) == 0) continue;

			audio_attr_t *audio_attr = &vtsi_mat->vts_audio_attr[i];

			this_track.title->audio_streams.push_back({
				.id = audio_id[audio_attr->audio_format] + (pgc->audio_control[i] >> 8 & 7),
				.lang = audio_attr->lang_code,
			});
		}

		// Subtitles
		int x = vtsi_mat->vts_video_attr.display_aspect_ratio == 0 ? 24 : 8;
		for (int i = 0; i < 32; i++)
		{
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
	for (int i = 1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) ifoClose(ifo[i]);
	delete [] ifo;
	ifoClose(ifo_zero);

	removeCompositeTracks();

	tracks.shrink_to_fit();
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

	// seek to beginning to track/part
	pos = 0;
	pos_byte_offset = 0;
	current_part = 0;

	// open dvd track
	if(!m_open) {
		dvd_track = DVDOpenFile(dvd_device, tracks[current_track].title->title_num, DVD_READ_TITLE_VOBS );

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
	// read in block in whole numbers
	int blocks_to_read = uiBufSize / DVD_VIDEO_LB_LEN;

	if(pos + blocks_to_read > tracks[current_track].parts[current_part].blocks) {
		blocks_to_read = tracks[current_track].parts[current_part].blocks - pos;

		if(blocks_to_read < 1)
			return 0;
	}

    int read_start = tracks[current_track].parts[current_part].first_sector + pos;
	int read_blocks = DVDReadBlocks(dvd_track, read_start, blocks_to_read, lpBuf);

	// return any error
	if(read_blocks <= 0)
		return read_blocks;

	// move internal pointers forward
	pos += read_blocks;
	if(pos == tracks[current_track].parts[current_part].blocks
			&& current_part < (int)tracks[current_track].parts.size() - 1)
	{
		current_part++;
		pos = 0;
	}

	int bytes_read = read_blocks * DVD_VIDEO_LB_LEN - pos_byte_offset;

	if(pos_byte_offset > 0) {
		// shift the contents of the buffer to the left
		memmove(lpBuf, lpBuf + pos_byte_offset, bytes_read);

		pos_byte_offset = 0;
	}

	return bytes_read;
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
	pos_byte_offset = iFilePosition % DVD_VIDEO_LB_LEN;

	pos = iFilePosition / DVD_VIDEO_LB_LEN;
	current_part = 0;

	// loop through blocks (except the last)
	while(current_part + 1 < (int)tracks[current_track].parts.size()
			&& pos >= tracks[current_track].parts[current_part].blocks)
	{
		pos -= tracks[current_track].parts[current_part].blocks;
		current_part++;
	}

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
	int total_blocks = 0;
	for(uint i = 0; i < tracks[current_track].parts.size(); i++)
		total_blocks += tracks[current_track].parts[i].blocks;

	return (int64_t)total_blocks * DVD_VIDEO_LB_LEN;
}

bool OMXDvdPlayer::IsEOF()
{
	if(!m_open)
		return true;

	if(current_part > (int)tracks[current_track].parts.size() - 1)
		return true;

	if(current_part < (int)tracks[current_track].parts.size() - 1)
		return false;

	return pos >= tracks[current_track].parts[current_part].blocks;
}

int OMXDvdPlayer::TotalChapters()
{
	return tracks[current_track].chapters.size();
}

void OMXDvdPlayer::CloseTrack()
{
	DVDCloseFile(dvd_track);
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

	DVDClose(dvd_device);
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
	if (DVDDiscID(dvd_device, &buf[0]) == -1) {
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

// Search for and disable composite tracks
// Example layouts:

//  |-                  Track 1                 -|
//  |-        Track 2     -||-     Track 3      -|

//  |-        Track 1     -||-     Track 2      -|
//  |-                  Track 3                 -|

void OMXDvdPlayer::removeCompositeTracks()
{
	// build a subset of tracks longer than 17.5 minutes
	std::vector<uint> lt;
	for(uint i = 0; i < tracks.size(); i++)
		if(tracks[i].length > 1050000)
			lt.push_back(i);

	// we need at least three of these tracks
	if(lt.size() < 3)
		return;

	// all the tracks should be in the same title set (VOB file)
	int title_set = tracks[lt[0]].title->title_num;
	for(uint i = 1; i < lt.size(); i++)
		if(tracks[lt[i]].title->title_num != title_set)
			return;

	// find the longer of the first and last tracks
	int longest_track_of_subset;
	if(tracks[lt[0]].length > tracks[lt.back()].length)
		longest_track_of_subset = 0;
	else
		longest_track_of_subset = lt.size() - 1;

	// extract it
	int longest_track = lt[longest_track_of_subset];
	lt.erase(lt.begin() + longest_track_of_subset);

	// compare begin point
	if(tracks[longest_track].parts[0].first_sector != tracks[lt[0]].parts[0].first_sector)
		return;

	// get the length of the remaining tracks within the subset
	int length_of_other_tracks = 0;
	for(uint &i : lt)
		length_of_other_tracks += tracks[i].length;

	// compare lengths - allow a margin of two seconds
	int diff = tracks[longest_track].length - length_of_other_tracks;
	if(diff < -2000 || diff > 2000)
		return;

	// now delete the composite track
	tracks.erase(tracks.begin() + longest_track);
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

void OMXDvdPlayer::info_dump()
{
    printf("DVD Tracks: %u\n", tracks.size());
    for(uint i = 0; i < tracks.size(); i++)
    {
        printf("    %2u. %02d:%02d:%02d\n",
            i + 1,
            tracks[i].length / 3600000,
            (tracks[i].length / 60000) % 60,
            (tracks[i].length / 1000) % 60);
    }
}
