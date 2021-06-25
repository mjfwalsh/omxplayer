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
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>

#include "OMXReader.h"
#include "OMXDvdPlayer.h"

bool OMXDvdPlayer::Open(const std::string &filename)
{
	static int audio_id[7] = {0x80, 0, 0xC0, 0xC0, 0xA0, 0, 0x88};
	device_path = filename;

	// Open DVD device or file
	dvd_device = DVDOpen(device_path.c_str());
	if(!dvd_device) {
		puts("Error on DVDOpen");
		return false;
	}

	// Get device name and checksum
	read_title_name();
	read_disc_checksum();

	// Open dvd meta data header
	ifo_handle_t *ifo_zero, **ifo;
	ifo_zero = ifoOpen(dvd_device, 0);
	if ( !ifo_zero ) {
		fprintf( stderr, "Can't open main ifo!\n");
		return false;
	}

	ifo = new ifo_handle_t*[ifo_zero->vts_atrt->nr_of_vtss + 1];
	//ifo = (ifo_handle_t **)malloc((ifo_zero->vts_atrt->nr_of_vtss + 1) * sizeof(ifo_handle_t *));
	for (int i=1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) {
		ifo[i] = ifoOpen(dvd_device, i);
		if ( !ifo[i] && i == 0 ) {
			fprintf( stderr, "Can't open ifo %d!\n", i);
			return false;
		}
	}

	// loop through title sets
	title_count = ifo_zero->tt_srpt->nr_of_srpts;
	titles = new title_info[title_count];
	for (int j=0, h=0; j < title_count; j++, h++) {
		int title_set_nr = ifo_zero->tt_srpt->title[j].title_set_nr;
		int vts_ttn = ifo_zero->tt_srpt->title[j].vts_ttn;

		if (!ifo[title_set_nr]->vtsi_mat) {
			h--;
			title_count--;
			continue;
		}

		pgcit_t    *vts_pgcit  = ifo[title_set_nr]->vts_pgcit;
		vtsi_mat_t *vtsi_mat   = ifo[title_set_nr]->vtsi_mat;
		pgc_t      *pgc        = vts_pgcit->pgci_srp[ifo[title_set_nr]->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn - 1].pgc;

		if(pgc->cell_playback == NULL || pgc->program_map == NULL) {
			h--;
			title_count--;
			continue;
		}

		titles[h].enabled = true;
		titles[h].vts = title_set_nr;
		titles[h].length = dvdtime2msec(&pgc->playback_time);
		titles[h].chapter_count = pgc->nr_of_programs;
		int cell_count = pgc->nr_of_cells;

		// Tracks
		// ignore non-contiguous ends of tracks
		titles[h].first_sector = pgc->cell_playback[0].first_sector;
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
				titles[h].length -= missing_time;
				break;
			}
		}
		if(last_sector == -1) {
			int last = cell_count - 1;
			last_sector = pgc->cell_playback[last].last_sector;
		}
		titles[h].last_sector = last_sector;

		// Chapters
		titles[h].chapters = new int[titles[h].chapter_count];

		int acc_chapter = 0;
		for (int i=0; i<titles[h].chapter_count; i++) {
			int idx = pgc->program_map[i] - 1;
			int first_cell_sector = pgc->cell_playback[idx].first_sector;
			if(first_cell_sector > last_sector) {
				titles[h].chapter_count = i;
				break;
			}
			titles[h].chapters[i] = acc_chapter;

			acc_chapter += dvdtime2msec(&pgc->cell_playback[idx].playback_time);
		}

		// Streams
		titles[h].audiostream_count = 0;
		titles[h].subtitle_count = 0;
		for (int k = 0; k < 8; k++)
			if (pgc->audio_control[k] & 0x8000)
				titles[h].audiostream_count++;

		for (int k = 0; k < 32; k++)
			if (pgc->subp_control[k] & 0x80000000)
				titles[h].subtitle_count++;

		titles[h].streams = new title_info::stream_info[titles[h].audiostream_count + titles[h].subtitle_count];
		int stream_index = 0;

		// Audio streams
		for (int i=0, k=0; i<8; i++) {
			if ((pgc->audio_control[i] & 0x8000) == 0) continue;

			audio_attr_t *audio_attr = &vtsi_mat->vts_audio_attr[i];
			titles[h].streams[stream_index++] = {
				k++,
				audio_id[audio_attr->audio_format] + (pgc->audio_control[i] >> 8 & 7),
				audio_attr->lang_code
			};
		}

		// Subtitles
		int x = vtsi_mat->vts_video_attr.display_aspect_ratio == 0 ? 24 : 8;
		for (int i=0, k=0; i<32; i++) {
			if ((pgc->subp_control[i] & 0x80000000) == 0) continue;

			subp_attr_t *subp_attr = &vtsi_mat->vts_subp_attr[i];
			titles[h].streams[stream_index++] = {
				k++,
				(int)((pgc->subp_control[i] >> x) & 0x1f) + 0x20,
				subp_attr->lang_code,
			};
		}
	}

	// close dvd meta data filehandles
	for (int i=1; i <= ifo_zero->vts_atrt->nr_of_vtss; i++) ifoClose(ifo[i]);
	delete ifo;
	ifoClose(ifo_zero);
	m_allocated = true;
	puts("Finished parsing DVD meta data");
	return true;
}

bool OMXDvdPlayer::ChangeTrack(int delta, int &t)
{
	int ct;
	if(delta == -1)
		ct = findPrevEnabledTrack(t);
	else
		ct = findNextEnabledTrack(t);

	if(ct == -1)
		return false;

	bool r = OpenTrack(ct);
	t = current_track;
	return r;
}

bool OMXDvdPlayer::OpenTrack(int ct)
{
	if(m_open && titles[ct].vts != titles[current_track].vts)
		CloseTrack();

	if(ct < 0 || ct > title_count - 1)
		return false;

	// select track
	current_track = ct;

	// seek to beginning to track
	pos = 0;
	pos_locked = false;

	// blocks for this track
	total_blocks = titles[current_track].last_sector - titles[current_track].first_sector + 1;

	// open dvd track
	if(!m_open) {
		dvd_track = DVDOpenFile(dvd_device, titles[current_track].vts, DVD_READ_TITLE_VOBS );

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

	// capture pos in cpos to avoid it changing midway through read
	int cpos = pos;
	pos_locked = false;

	// read in block in whole numbers
	int blocks_to_read = uiBufSize / 2048;

	if(cpos + blocks_to_read > total_blocks) {
		blocks_to_read = total_blocks - cpos;

		if(blocks_to_read < 1)
			return 0;
	}

	int read_blocks;
	if(pos_byte_offset > 0) {
		unsigned char *buffer;
		buffer = (unsigned char *)malloc(uiBufSize);
		read_blocks = DVDReadBlocks(dvd_track, titles[current_track].first_sector + cpos, blocks_to_read, buffer);
		memcpy(lpBuf, buffer + pos_byte_offset, (read_blocks * 2048) - pos_byte_offset);
		pos_byte_offset = 0;
		free(buffer);
	} else {
		read_blocks = DVDReadBlocks(dvd_track, titles[current_track].first_sector + cpos, blocks_to_read, lpBuf);
	}

	if(!pos_locked) pos = cpos + read_blocks;
	return read_blocks * 2048;
}

int OMXDvdPlayer::getCurrentTrackLength()
{
	return titles[current_track].length;
}

int64_t OMXDvdPlayer::Seek(int64_t iFilePosition, int iWhence)
{
	if (!m_open || iWhence != SEEK_SET)
		return -1;

	// seek in blocks
	pos_locked = true;
	pos = iFilePosition / 2048;
	pos_byte_offset = iFilePosition % 2048;

	return 0;
}

int64_t OMXDvdPlayer::GetSizeInBytes()
{
	return (int64_t)total_blocks * 2048;
}

bool OMXDvdPlayer::IsEOF()
{
	if(!m_open)
		return false;

	return pos >= total_blocks;
}

int OMXDvdPlayer::TotalChapters()
{
	return titles[current_track].chapter_count;
}

float OMXDvdPlayer::GetChapterStartTime(int i)
{
	return (float)(titles[current_track].chapters[i] / 1000.0);
}

void OMXDvdPlayer::CloseTrack()
{
	DVDCloseFile(dvd_track);
	m_open = false;
}

// A sanity check. Before copying over the meta data, make sure the number of streams found
// in the vob file correspond to streams listed in the DVD meta data
bool OMXDvdPlayer::MetaDataCheck(int audiostream_count, int subtitle_count)
{
	return titles[current_track].audiostream_count == audiostream_count &&
		titles[current_track].subtitle_count == subtitle_count;
}

void OMXDvdPlayer::GetStreamInfo(OMXStream *stream)
{
	int len = titles[current_track].audiostream_count + titles[current_track].subtitle_count;

	for (int i=0; i < len; i++) {
		if(titles[current_track].streams[i].id == stream->stream->id) {
			stream->index = titles[current_track].streams[i].index;
			strcpy(stream->language, convertLangCode(titles[current_track].streams[i].lang));
			return;
		}
	}

	// fail
	stream->index = -1;
}

OMXDvdPlayer::~OMXDvdPlayer()
{
	if(m_open)
		CloseTrack();

	if(m_allocated) {
		for (int i=0; i < title_count; i++) {
			delete titles[i].chapters;
			delete titles[i].streams;
		}
		delete titles;
	}

	if(dvd_device)
		DVDClose(dvd_device);
}

int OMXDvdPlayer::dvdtime2msec(dvd_time_t *dt)
{
	double fps = frames_per_s[(dt->frame_u & 0xc0) >> 6];
	long   ms;
	ms  = (((dt->hour &   0xf0) >> 3) * 5 + (dt->hour   & 0x0f)) * 3600000;
	ms += (((dt->minute & 0xf0) >> 3) * 5 + (dt->minute & 0x0f)) * 60000;
	ms += (((dt->second & 0xf0) >> 3) * 5 + (dt->second & 0x0f)) * 1000;

	if(fps > 0)
	ms += (((dt->frame_u & 0x30) >> 3) * 5 + (dt->frame_u & 0x0f)) * 1000.0 / fps;

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
	if (DVDDiscID(dvd_device, &buf[0]) == -1) return;

	char hex[33];
	for (int i = 0; i < 16; i++)
		sprintf(hex + 2 * i, "%02x", buf[i]);

	disc_checksum = hex;
}

//enable heuristic track skip
void OMXDvdPlayer::enableHeuristicTrackSelection()
{
	// Disable tracks which are shorter than two minutes
	for(int i = 0; i < title_count; i++) {
		if(titles[i].length < 120000) {
			titles[i].enabled = false;
		}
	}

	// Search for and disable composite tracks
	for(int i = 0; i < title_count - 1; i++) {
		for(int j = i + 1; j < title_count; j++) {
			if(titles[i].vts == titles[j].vts
					&& titles[i].first_sector == titles[j].first_sector
					&& titles[i].enabled && titles[j].enabled) {

				if(titles[i].length > titles[j].length)
					titles[i].enabled = false;
				else
					titles[j].enabled = false;
			}
		}
	}
}

int OMXDvdPlayer::findNextEnabledTrack(int i)
{
	for(i++; i < title_count; i++) {
		if(titles[i].enabled)
			return i;
		else printf("Skipping Track %d\n", i+1);
	}
	return -1;
}

int OMXDvdPlayer::findPrevEnabledTrack(int i)
{
	for(i--; i > -1; i--) {
		if(titles[i].enabled)
			return i;
		else printf("Skipping Track %d\n", i+1);
	}
	return -1;
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